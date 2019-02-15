// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/* Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2015-2018 Google, Inc.
 */

#include <linux/module.h>
#include <linux/etherdevice.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <net/sch_generic.h>
#include "gve.h"
#include "gve_adminq.h"
#include "gve_register.h"

#define GVE_DEFAULT_RX_COPYBREAK	(256)

#define DEFAULT_MSG_LEVEL	(NETIF_MSG_DRV | NETIF_MSG_LINK)
#define GVE_VERSION		"0.0.0"
#define GVE_VERSION_PREFIX	"GVE-"

const char gve_version_str[] = GVE_VERSION;
const char gve_version_prefix[] = GVE_VERSION_PREFIX;

static void gve_get_stats(struct net_device *dev, struct rtnl_link_stats64 *s)
{
	struct gve_priv *priv = netdev_priv(dev);
	int ring;

	if (priv->rx) {
		for (ring = 0; ring < priv->rx_cfg.num_queues; ring++) {
			s->rx_packets += priv->rx[ring].rpackets;
			s->rx_bytes += priv->rx[ring].rbytes;
		}
	}
	if (priv->tx) {
		for (ring = 0; ring < priv->tx_cfg.num_queues; ring++) {
			s->tx_packets += priv->tx[ring].pkt_done;
			s->tx_bytes += priv->tx[ring].bytes_done;
		}
	}
}

static int gve_alloc_counter_array(struct gve_priv *priv)
{
	priv->counter_array =
		dma_zalloc_coherent(&priv->pdev->dev,
				    priv->num_event_counters *
				    sizeof(*priv->counter_array),
				    &priv->counter_array_bus, GFP_KERNEL);
	if (!priv->counter_array)
		return -ENOMEM;

	return 0;
}

static void gve_free_counter_array(struct gve_priv *priv)
{
	dma_free_coherent(&priv->pdev->dev,
			  priv->num_event_counters *
			  sizeof(*priv->counter_array),
			  priv->counter_array, priv->counter_array_bus);
	priv->counter_array = NULL;
}

static irqreturn_t gve_mgmnt_intr(int irq, void *arg)
{
	struct gve_priv *priv = arg;

	queue_work(priv->gve_wq, &priv->service_task);
	return IRQ_HANDLED;
}

static irqreturn_t gve_intr(int irq, void *arg)
{
	struct gve_notify_block *block = arg;
	struct gve_priv *priv = block->priv;
	writel(cpu_to_be32(GVE_IRQ_MASK), gve_irq_doorbell(priv, block));
	napi_schedule_irqoff(&block->napi);
	return IRQ_HANDLED;
}

int gve_napi_poll(struct napi_struct *napi, int budget)
{
	struct gve_notify_block *block;
	__be32 __iomem *irq_doorbell;
	bool reschedule = false;
	struct gve_priv *priv;

	block = container_of(napi, struct gve_notify_block, napi);
	priv = block->priv;

	if (block->tx)
		reschedule |= gve_tx_poll(block, budget);
	if (block->rx)
		reschedule |= gve_rx_poll(block, budget);

	if (reschedule)
		return budget;

	napi_complete(napi);
	irq_doorbell = gve_irq_doorbell(priv, block);
	writel(cpu_to_be32(GVE_IRQ_ACK | GVE_IRQ_EVENT), irq_doorbell);

	/* Double check we have no extra work */
	/* Ensure unmask synchronizes-with checking for work */
	dma_rmb();
	if (block->tx)
		reschedule |= gve_tx_poll(block, -1);
	if (block->rx)
		reschedule |= gve_rx_poll(block, -1);
	if (reschedule && napi_reschedule(napi))
		writel(cpu_to_be32(GVE_IRQ_MASK), irq_doorbell);

	return 0;
}

static int gve_alloc_notify_blocks(struct gve_priv *priv)
{
	int num_vecs_requested = priv->num_ntfy_blks + 1;
	struct device *hdev = &priv->pdev->dev;
	char *name = priv->dev->name;
	int vecs_enabled;
	int i, j;
	int err;

	priv->msix_vectors = kcalloc(num_vecs_requested,
				     sizeof(*priv->msix_vectors), GFP_KERNEL);
	if (!priv->msix_vectors)
		return -ENOMEM;
	for (i = 0; i < num_vecs_requested; i++)
		priv->msix_vectors[i].entry = i;
	vecs_enabled = pci_enable_msix_range(priv->pdev, priv->msix_vectors,
					     GVE_MIN_MSIX, num_vecs_requested);
	if (vecs_enabled < 0) {
		dev_err(hdev, "Could not enable min msix %d/%d\n",
			GVE_MIN_MSIX, vecs_enabled);
		err = vecs_enabled;
		goto abort_with_msix_vectors;
	}
	if (vecs_enabled != num_vecs_requested) {
		int new_num_ntfy_blks = vecs_enabled - 1;
		int vecs_per_type = new_num_ntfy_blks / 2;
		int vecs_left = new_num_ntfy_blks % 2;

		priv->num_ntfy_blks = new_num_ntfy_blks;
		priv->tx_cfg.max_queues = min_t(int, priv->tx_cfg.max_queues,
                                                vecs_per_type);
		priv->rx_cfg.max_queues = min_t(int, priv->rx_cfg.max_queues,
                                                vecs_per_type + vecs_left);
		dev_info(hdev, "Could not enable desired msix, only enabled %d, adjusting tx max queues to %d, and rx max queues to %d\n",
			 vecs_enabled, priv->tx_cfg.max_queues,
			 priv->rx_cfg.max_queues);
		if (priv->tx_cfg.num_queues > priv->tx_cfg.max_queues)
			priv->tx_cfg.num_queues = priv->tx_cfg.max_queues;
		if (priv->rx_cfg.num_queues > priv->rx_cfg.max_queues)
			priv->rx_cfg.num_queues = priv->rx_cfg.max_queues;
	}

	/* Setup Management Vector */
	snprintf(priv->mgmt_msix_name, sizeof(priv->mgmt_msix_name), "%s-mgmnt",
		 name);
	err = request_irq(priv->msix_vectors[priv->mgmt_msix_idx].vector,
			  gve_mgmnt_intr, 0, priv->mgmt_msix_name, priv);
	if (err) {
		dev_err(hdev, "Did not receive management vector.\n");
		goto abort_with_msix_enabled;
	}
	priv->ntfy_blocks =
		dma_zalloc_coherent(&priv->pdev->dev,
				    priv->num_ntfy_blks *
				    sizeof(*priv->ntfy_blocks),
				    &priv->ntfy_block_bus, GFP_KERNEL);
	if (!priv->ntfy_blocks) {
		dev_err(hdev, "Failed to allocate notification blocks\n");
		err = -ENOMEM;
		goto abort_with_mgmt_vector;
	}
	/* Setup the other blocks */
	for (i = 0; i < priv->num_ntfy_blks; i++) {
		struct gve_notify_block *block = &priv->ntfy_blocks[i];
		int msix_idx = i + priv->ntfy_blk_msix_base_idx;

		snprintf(block->name, sizeof(block->name), "%s-ntfy-block.%d",
			 name, i);
		block->priv = priv;
		err = request_irq(priv->msix_vectors[msix_idx].vector,
				  gve_intr, 0, block->name, block);
		if (err) {
			dev_err(hdev, "Failed to receive msix vector %d\n", i);
			goto abort_with_some_ntfy_blocks;
		}
		irq_set_affinity_hint(priv->msix_vectors[msix_idx].vector,
				get_cpu_mask((i - 1) % num_online_cpus()));
	}
	return 0;
abort_with_some_ntfy_blocks:
	for (j = 0; j < i; j++) {
		struct gve_notify_block *block = &priv->ntfy_blocks[j];
		int msix_idx = j + priv->ntfy_blk_msix_base_idx;

		irq_set_affinity_hint(priv->msix_vectors[msix_idx].vector,
				      NULL);
		free_irq(priv->msix_vectors[msix_idx].vector, block);
	}
	dma_free_coherent(&priv->pdev->dev, priv->num_ntfy_blks *
			  sizeof(*priv->ntfy_blocks),
			  priv->ntfy_blocks, priv->ntfy_block_bus);
	priv->ntfy_blocks = NULL;
abort_with_mgmt_vector:
	free_irq(priv->msix_vectors[priv->mgmt_msix_idx].vector, priv);
abort_with_msix_enabled:
	pci_disable_msix(priv->pdev);
abort_with_msix_vectors:
	kfree(priv->msix_vectors);
	priv->msix_vectors = NULL;
	return err;
}

static void gve_free_notify_blocks(struct gve_priv *priv)
{
	int i;

	/* Free the irqs */
	for (i = 0; i < priv->num_ntfy_blks; i++) {
		struct gve_notify_block *block = &priv->ntfy_blocks[i];
		int msix_idx = i + priv->ntfy_blk_msix_base_idx;

		irq_set_affinity_hint(priv->msix_vectors[msix_idx].vector,
				      NULL);
		free_irq(priv->msix_vectors[msix_idx].vector, block);
	}
	dma_free_coherent(&priv->pdev->dev,
			  priv->num_ntfy_blks * sizeof(*priv->ntfy_blocks),
			  priv->ntfy_blocks, priv->ntfy_block_bus);
	priv->ntfy_blocks = NULL;
	free_irq(priv->msix_vectors[priv->mgmt_msix_idx].vector, priv);
	pci_disable_msix(priv->pdev);
	kfree(priv->msix_vectors);
	priv->msix_vectors = NULL;
}

static int gve_setup_device_resources(struct gve_priv *priv)
{
	int err;

	err = gve_alloc_counter_array(priv);
	if (err)
		return err;
	err = gve_alloc_notify_blocks(priv);
	if (err)
		goto abort_with_counter;
	err = gve_adminq_configure_device_resources(priv,
						    priv->counter_array_bus,
						    priv->num_event_counters,
						    priv->ntfy_block_bus,
						    priv->num_ntfy_blks);
	if (unlikely(err)) {
		dev_err(&priv->pdev->dev,
			"could not setup device_resources: err=%d\n", err);
		err = -ENXIO;
		goto abort_with_ntfy_blocks;
	}
	set_bit(GVE_PRIV_FLAGS_DEVICE_RESOURCES_OK, &priv->state_flags);
	return 0;
abort_with_ntfy_blocks:
	gve_free_notify_blocks(priv);
abort_with_counter:
	gve_free_counter_array(priv);
	return err;
}

static void gve_trigger_reset(struct gve_priv *priv);

static void gve_teardown_device_resources(struct gve_priv *priv)
{
	int err;

	/* Tell device its resources are being freed */
	if (test_bit(GVE_PRIV_FLAGS_DEVICE_RESOURCES_OK, &priv->state_flags)) {
		err = gve_adminq_deconfigure_device_resources(priv);
		if (err)
			gve_trigger_reset(priv);
	}
	gve_free_counter_array(priv);
	gve_free_notify_blocks(priv);
	clear_bit(GVE_PRIV_FLAGS_DEVICE_RESOURCES_OK, &priv->state_flags);
}

void gve_add_napi(struct gve_priv *priv, struct gve_notify_block *block)
{
	netif_napi_add(priv->dev, &block->napi, gve_napi_poll,
		       NAPI_POLL_WEIGHT);
}

void gve_remove_napi(struct gve_notify_block *block)
{
	netif_napi_del(&block->napi);
}

static int gve_register_qpls(struct gve_priv *priv)
{
	int num_qpls = gve_num_tx_qpls(priv) + gve_num_rx_qpls(priv);
	int err;
	int i;

	for (i = 0; i < num_qpls; i++) {
		err = gve_adminq_register_page_list(priv, &priv->qpls[i]);
		if (err) {
			netif_err(priv, drv, priv->dev,
				  "failed to register queue page list %d\n",
				  priv->qpls[i].id);
			return err;
		}
	}
	return 0;
}

static int gve_unregister_qpls(struct gve_priv *priv)
{
	int num_qpls = gve_num_tx_qpls(priv) + gve_num_rx_qpls(priv);
	int err;
	int i;

	for (i = 0; i < num_qpls; i++) {
		err = gve_adminq_unregister_page_list(priv, priv->qpls[i].id);
		if (err)
			return err;
	}
	return 0;
}

static int gve_create_rings(struct gve_priv *priv)
{
	int err;
	int i;

	for (i = 0; i < priv->tx_cfg.num_queues; i++) {
		err = gve_adminq_create_tx_queue(priv, i);
		if (err) {
			netif_err(priv, drv, priv->dev, "failed to create tx queue %d\n",
				  i);
			return err;
		}
		netif_dbg(priv, drv, priv->dev, "created tx queue %d\n", i);
	}
	for (i = 0; i < priv->rx_cfg.num_queues; i++) {
		err = gve_adminq_create_rx_queue(priv, i);
		if (err) {
			netif_err(priv, drv, priv->dev, "failed to create rx queue %d\n",
				  i);
			return err;
		}
		/* Rx data ring has been prefilled with packet buffers at
		 * queue allocation time.
		 * Write the doorbell to provide descriptor slots and packet
		 * buffers to the NIC.
		 */
		gve_rx_write_doorbell(priv, &priv->rx[i]);
		netif_dbg(priv, drv, priv->dev, "created rx queue %d\n", i);
	}

	return 0;
}

static int gve_alloc_rings(struct gve_priv *priv)
{
	int err;

	/* Setup tx rings */
	priv->tx = kcalloc(priv->tx_cfg.num_queues, sizeof(*priv->tx),
			   GFP_KERNEL);
	if (!priv->tx)
		return -ENOMEM;
	err = gve_tx_alloc_rings(priv);
	if (err)
		goto free_tx;
	/* Setup rx rings */
	priv->rx = kcalloc(priv->rx_cfg.num_queues, sizeof(*priv->rx),
			   GFP_KERNEL);
	if (!priv->rx) {
		err = -ENOMEM;
		goto free_tx_queue;
	}
	err = gve_rx_alloc_rings(priv);
	if (err)
		goto free_rx;

	return 0;

free_rx:
	kfree(priv->rx);
free_tx_queue:
	gve_tx_free_rings(priv);
free_tx:
	kfree(priv->tx);
	return err;
}

static int gve_destroy_rings(struct gve_priv *priv)
{
	int err;
	int i;

	for (i = 0; i < priv->tx_cfg.num_queues; i++) {
		err = gve_adminq_destroy_tx_queue(priv, i);
		if (err) {
			netif_err(priv, drv, priv->dev,
				  "failed to destroy tx queue %d\n",
				  i);
			return err;
		}
		netif_dbg(priv, drv, priv->dev, "destroyed tx queue %d\n", i);
	}
	for (i = 0; i < priv->rx_cfg.num_queues; i++) {
		err = gve_adminq_destroy_rx_queue(priv, i);
		if (err) {
			netif_err(priv, drv, priv->dev,
				  "failed to destroy rx queue %d\n",
				  i);
			return err;
		}
		netif_dbg(priv, drv, priv->dev, "destroyed rx queue %d\n", i);
	}
	return 0;
}

static void gve_free_rings(struct gve_priv *priv)
{
	if (priv->tx) {
		gve_tx_free_rings(priv);
		kfree(priv->tx);
	}
	if (priv->rx) {
		gve_rx_free_rings(priv);
		kfree(priv->rx);
	}
}

static int gve_alloc_queue_page_list(struct gve_priv *priv, u32 id,
				     int pages)
{
	struct gve_queue_page_list *qpl = &priv->qpls[id];
	struct device *dev = &priv->pdev->dev;
	int i;

	if (pages + priv->num_registered_pages > priv->max_registered_pages)
		return -EINVAL;

	qpl->id = id;
	qpl->num_entries = pages;
	qpl->pages = kcalloc(pages, sizeof(*qpl->pages), GFP_KERNEL);
	if (!qpl->pages)
		return -ENOMEM;
	qpl->page_buses = kcalloc(pages, sizeof(*qpl->page_buses), GFP_KERNEL);
	if (!qpl->page_buses)
		return -ENOMEM;
	qpl->page_ptrs = kcalloc(pages, sizeof(*qpl->page_ptrs), GFP_KERNEL);
	if (!qpl->page_ptrs)
		return -ENOMEM;

	for (i = 0; i < pages; i++) {
		qpl->page_ptrs[i] = dma_alloc_coherent(dev, PAGE_SIZE,
						       &qpl->page_buses[i],
						       GFP_KERNEL);
		if (!qpl->page_ptrs[i])
			return -ENOMEM;
		if (is_vmalloc_addr(qpl->page_ptrs[i]))
			qpl->pages[i] = vmalloc_to_page(qpl->page_ptrs[i]);
		else
			qpl->pages[i] = virt_to_page(qpl->page_ptrs[i]);
		if (!qpl->pages[i])
			return -ENOMEM;
	}
	priv->num_registered_pages += pages;

	return 0;
}

static void gve_free_queue_page_list(struct gve_priv *priv,
				     int id)
{
	struct gve_queue_page_list *qpl = &priv->qpls[id];
	struct device *dev = &priv->pdev->dev;
	int i;

	if (!qpl->pages)
		return;
	if (!qpl->page_buses)
		goto free_pages;
	if (!qpl->page_ptrs)
		goto free_buses;

	for (i = 0; i < qpl->num_entries; i++) {
		if (qpl->page_ptrs[i])
			dma_free_coherent(dev, PAGE_SIZE, qpl->page_ptrs[i],
					  qpl->page_buses[i]);
	}

	kfree(qpl->page_ptrs);
free_buses:
	kfree(qpl->page_buses);
free_pages:
	kfree(qpl->pages);
	priv->num_registered_pages -= qpl->num_entries;
}

static int gve_alloc_qpls(struct gve_priv *priv)
{
	int num_qpls = gve_num_tx_qpls(priv) + gve_num_rx_qpls(priv);
	int i, j;
	int err;

	priv->qpls = kcalloc(num_qpls, sizeof(*priv->qpls), GFP_KERNEL);
	if (!priv->qpls)
		return -ENOMEM;

	for (i = 0; i < gve_num_tx_qpls(priv); i++) {
		err = gve_alloc_queue_page_list(priv, i,
						priv->tx_pages_per_qpl);
		if (err)
			goto free_qpls;
	}
	for (; i < num_qpls; i++) {
		err = gve_alloc_queue_page_list(priv, i,
						priv->rx_pages_per_qpl);
		if (err)
			goto free_qpls;
	}

	priv->qpl_cfg.qpl_map_size = BITS_TO_LONGS(num_qpls) *
				     sizeof(unsigned long) * BITS_PER_BYTE;
	priv->qpl_cfg.qpl_id_map = kcalloc(BITS_TO_LONGS(num_qpls),
					   sizeof(unsigned long), GFP_KERNEL);
	if (!priv->qpl_cfg.qpl_id_map)
		goto free_qpls;

	return 0;

free_qpls:
	for (j = 0; j < i; j++)
		gve_free_queue_page_list(priv, j);
	kfree(priv->qpls);
	return err;
}

static void gve_free_qpls(struct gve_priv *priv)
{
	int num_qpls = gve_num_tx_qpls(priv) + gve_num_rx_qpls(priv);
	int i;

	kfree(priv->qpl_cfg.qpl_id_map);

	for (i = 0; i < num_qpls; i++)
		gve_free_queue_page_list(priv, i);

	kfree(priv->qpls);
}

void gve_schedule_reset(struct gve_priv *priv)
{
	set_bit(GVE_PRIV_FLAGS_DO_RESET, &priv->service_task_flags);
	queue_work(priv->gve_wq, &priv->service_task);
}

static int gve_change_mtu(struct net_device *dev, int new_mtu)
{
	struct gve_priv *priv = netdev_priv(dev);

	if (new_mtu < GVE_MIN_MTU || new_mtu > priv->max_mtu)
		return -EINVAL;

	dev->mtu = new_mtu;
	return 0;
}

static void gve_reset_and_teardown(struct gve_priv *priv, bool was_up);
static int gve_reset_recovery(struct gve_priv *priv, bool was_up);
static void gve_turndown(struct gve_priv *priv);
static void gve_turnup(struct gve_priv *priv);

static int gve_open(struct net_device *dev)
{
	struct gve_priv *priv = netdev_priv(dev);
	int err;

	err = gve_alloc_qpls(priv);
	if (err)
		return err;
	err = gve_alloc_rings(priv);
	if (err) {
		gve_free_qpls(priv);
		return err;
	}

	err = netif_set_real_num_tx_queues(dev, priv->tx_cfg.num_queues);
	if (err) {
		gve_free_rings(priv);
		gve_free_qpls(priv);
		return err;
	}
	err = netif_set_real_num_rx_queues(dev, priv->rx_cfg.num_queues);
	if (err) {
		gve_free_rings(priv);
		gve_free_qpls(priv);
		return err;
	}

	err = gve_register_qpls(priv);
	if (err) {
		gve_schedule_reset(priv);
		return err;
	}
	err = gve_create_rings(priv);
	if (err) {
		gve_schedule_reset(priv);
		return err;
	}
	set_bit(GVE_PRIV_FLAGS_DEVICE_RINGS_OK, &priv->state_flags);

	gve_turnup(priv);
	netif_carrier_on(dev);
	return 0;
}

static int gve_close(struct net_device *dev)
{
	struct gve_priv *priv = netdev_priv(dev);
	int err;

	netif_carrier_off(priv->dev);
	if (test_bit(GVE_PRIV_FLAGS_DEVICE_RINGS_OK, &priv->state_flags)) {
		gve_turndown(priv);
		err = gve_destroy_rings(priv);
		if (err)
			goto err;
		err = gve_unregister_qpls(priv);
		if (err)
			goto err;
		clear_bit(GVE_PRIV_FLAGS_DEVICE_RINGS_OK, &priv->state_flags);
	}

	gve_free_rings(priv);
	gve_free_qpls(priv);
	return 0;
err:
	if (test_bit(GVE_PRIV_FLAGS_RESET_IN_PROGRESS,
		     &priv->service_task_flags)) {
		/* If we already have a reset going, just return the error */
		return err;
	} else {
		/* Otherwise do a reset */
		gve_reset_and_teardown(priv, true);
		return(gve_reset_recovery(priv, false));
	}
}

int gve_adjust_queues(struct gve_priv *priv,
		      struct gve_queue_config new_rx_config,
		      struct gve_queue_config new_tx_config)
{
	int err;

	if (netif_carrier_ok(priv->dev)) {
		/* To make this process as simple as possible we teardown the
		 * device, set the new configuration, and then bring the device
		 * up again.
		 */
		dev_deactivate(priv->dev);
		err = gve_close(priv->dev);
		if (err) {
			/* we have already tried to reset in close,
			 * just fail at this point
			 */
			 return err;
		}
		priv->tx_cfg = new_tx_config;
		priv->rx_cfg = new_rx_config;

		err = gve_open(priv->dev);
		if (err)
			goto err;
		dev_activate(priv->dev);
		return 0;
	} else {
		/* Set the config for the next up. */
		priv->tx_cfg = new_tx_config;
		priv->rx_cfg = new_rx_config;

		return 0;
	}
err:
	dev_err(&priv->pdev->dev,
		"Adjust queues failed! !!! DISABLING ALL QUEUES !!!\n");
	gve_turndown(priv);
	return err;
}

static void gve_turndown(struct gve_priv *priv)
{
	int idx;

	if (netif_carrier_ok(priv->dev)) {
		netif_carrier_off(priv->dev);
		dev_deactivate(priv->dev);
	}

	if (!test_bit(GVE_PRIV_FLAGS_NAPI_ENABLED, &priv->state_flags))
		return;

	/* Disable napi to prevent more work from coming in */
	for (idx = 0; idx < priv->tx_cfg.num_queues; idx++) {
		int ntfy_idx = gve_tx_ntfy_idx(priv, idx);
		struct gve_notify_block *block = &priv->ntfy_blocks[ntfy_idx];
		napi_disable(&block->napi);
	}
	for (idx = 0; idx < priv->rx_cfg.num_queues; idx++) {
		int ntfy_idx = gve_rx_ntfy_idx(priv, idx);
		struct gve_notify_block *block = &priv->ntfy_blocks[ntfy_idx];
		napi_disable(&block->napi);
	}

	clear_bit(GVE_PRIV_FLAGS_NAPI_ENABLED, &priv->state_flags);
}

static void gve_turnup(struct gve_priv *priv)
{
	int idx;

	/* Enable napi and unmask interupts for all queues */
	for (idx = 0; idx < priv->tx_cfg.num_queues; idx++) {
		int ntfy_idx = gve_tx_ntfy_idx(priv, idx);
		struct gve_notify_block *block = &priv->ntfy_blocks[ntfy_idx];
		napi_enable(&block->napi);
		writel(cpu_to_be32(0), gve_irq_doorbell(priv, block));
	}
	for (idx = 0; idx < priv->rx_cfg.num_queues; idx++) {
		int ntfy_idx = gve_rx_ntfy_idx(priv, idx);
		struct gve_notify_block *block = &priv->ntfy_blocks[ntfy_idx];
		napi_enable(&block->napi);
		writel(cpu_to_be32(0), gve_irq_doorbell(priv, block));
	}

	set_bit(GVE_PRIV_FLAGS_NAPI_ENABLED, &priv->state_flags);
}

static const struct net_device_ops gve_netdev_ops = {
	.ndo_start_xmit		=	gve_tx,
	.ndo_open		=	gve_open,
	.ndo_stop		=	gve_close,
	.ndo_get_stats64	=	gve_get_stats,
	.ndo_change_mtu		=	gve_change_mtu,
};

static void gve_write_version(void __iomem *reg_bar)
{
	const char *c = gve_version_str;

	while (*c) {
		writeb(*c, reg_bar + GVE_DRIVER_VERSION);
		c++;
	}
	writeb('\n', reg_bar + GVE_DRIVER_VERSION);
}

static void gve_handle_status(struct gve_priv *priv, u32 status)
{
	if (GVE_DEVICE_STATUS_RESET_MASK & status) {
		dev_info(&priv->pdev->dev, "Device requested reset.\n");
		set_bit(GVE_PRIV_FLAGS_DO_RESET,
			&priv->service_task_flags);
	}
}

static void gve_handle_reset(struct gve_priv *priv)
{
	/* A service task will be scheduled at the end of probe to catch any
	 * resets that need to happen, and we don't want to reset until
	 * probe is done.
	 */
	if (test_bit(GVE_PRIV_FLAGS_PROBE_IN_PROGRESS,
		     &priv->service_task_flags))
		return;

	if (test_bit(GVE_PRIV_FLAGS_DO_RESET, &priv->service_task_flags)) {
		gve_reset(priv, false);
	}
}

/* Handle NIC status register changes and reset requests */
static void gve_service_task(struct work_struct *work)
{
	struct gve_priv *priv = container_of(work, struct gve_priv,
					     service_task);

	rtnl_lock();
	gve_handle_status(priv, be32_to_cpu(readl(priv->reg_bar0 +
						  GVE_DEVICE_STATUS)));

	gve_handle_reset(priv);
	rtnl_unlock();
}

static int gve_init_priv(struct gve_priv *priv)
{
	int num_ntfy;
	int err;

	/* Set up the adminq */
	err = gve_alloc_adminq(&priv->pdev->dev, priv);
	if (err)
		return err;
	/* Get the initial information we need from the device */
	err = gve_adminq_describe_device(priv);
	if (err) {
		dev_err(&priv->pdev->dev,
			"Could not get device information: err=%d\n", err);
		goto abort_with_adminq;
	}
	num_ntfy = pci_msix_vec_count(priv->pdev);
	if (num_ntfy <= 0) {
		dev_err(&priv->pdev->dev,
			"could not count MSI-x vectors: err=%d\n", num_ntfy);
		err = num_ntfy;
		goto abort_with_adminq;
	} else if (num_ntfy < GVE_MIN_MSIX) {
		dev_err(&priv->pdev->dev, "gve needs at least %d MSI-x vectors, but only has %d\n",
			GVE_MIN_MSIX, num_ntfy);
		err = -EINVAL;
		goto abort_with_adminq;
	}

	priv->num_registered_pages = 0;
	priv->rx_copybreak = GVE_DEFAULT_RX_COPYBREAK;
	/* gvnic has one Notification Block per MSI-x vector, except for the
	 * management vector
	 */
	priv->num_ntfy_blks = num_ntfy - 1;
	priv->mgmt_msix_idx = num_ntfy - 1;
	priv->ntfy_blk_msix_base_idx = 0;

	priv->tx_cfg.max_queues =
		min_t(int, priv->tx_cfg.max_queues, priv->num_ntfy_blks / 2);
	priv->rx_cfg.max_queues =
		min_t(int, priv->rx_cfg.max_queues, priv->num_ntfy_blks / 2);

	priv->tx_cfg.num_queues =
		min_t(int, priv->default_num_queues, priv->tx_cfg.max_queues);
	priv->rx_cfg.num_queues =
		min_t(int, priv->default_num_queues, priv->rx_cfg.max_queues);

	netif_info(priv, drv, priv->dev, "TX queues %d, RX queues %d\n",
		   priv->tx_cfg.num_queues, priv->rx_cfg.num_queues);
	netif_info(priv, drv, priv->dev, "Max TX queues %d, Max RX queues %d\n",
		   priv->tx_cfg.max_queues, priv->rx_cfg.max_queues);

	err = gve_setup_device_resources(priv);
	if (err)
		goto abort_with_adminq;
	return 0;

abort_with_adminq:
	gve_free_adminq(&priv->pdev->dev, priv);
	return err;
}

static void gve_teardown_priv_resources(struct gve_priv *priv) {
	gve_teardown_device_resources(priv);
	gve_free_adminq(&priv->pdev->dev, priv);
}

static void gve_trigger_reset(struct gve_priv *priv) {
	/* Reset the device by releasing the AQ */
	gve_release_adminq(priv);
}

static void gve_reset_and_teardown(struct gve_priv *priv, bool was_up) {
	gve_trigger_reset(priv);
	if (was_up)
		/* With the reset having already happened, close cannot fail */
		gve_close(priv->dev);
	gve_teardown_priv_resources(priv);
}

static int gve_reset_recovery(struct gve_priv *priv, bool was_up) {
	int err;

	err = gve_init_priv(priv);
	if (err)
		goto err;
	if (was_up) {
		err = gve_open(priv->dev);
		if (err)
			goto err;
		dev_activate(priv->dev);
	}
	return 0;
err:
	dev_err(&priv->pdev->dev, "Reset failed! !!! DISABLING ALL QUEUES !!!\n");
	gve_turndown(priv);
	return err;
}

int gve_reset(struct gve_priv *priv, bool attempt_teardown)
{
	bool was_up = netif_carrier_ok(priv->dev);
	int err;

	dev_info(&priv->pdev->dev, "Performing reset\n");
	clear_bit(GVE_PRIV_FLAGS_DO_RESET, &priv->service_task_flags);
	set_bit(GVE_PRIV_FLAGS_RESET_IN_PROGRESS, &priv->service_task_flags);
	/* If we aren't attempting to teardown normally, just go turndown and
	 * reset right away.
	 */
	if (!attempt_teardown) {
		gve_turndown(priv);
		gve_reset_and_teardown(priv, was_up);
	} else {
		/* Otherwise attempt to close normally */
		if (was_up) {
			dev_deactivate(priv->dev);
			err = gve_close(priv->dev);
			/* If that fails, turndown and reset as we did above */
			if (err) {
				gve_turndown(priv);
				gve_reset_and_teardown(priv, was_up);
			}
		}
		/* Clean up any remaining resources */
		gve_teardown_priv_resources(priv);
	}

	/* Set it all back up */
	err = gve_reset_recovery(priv, was_up);
	clear_bit(GVE_PRIV_FLAGS_RESET_IN_PROGRESS, &priv->service_task_flags);
	return err;
}

static void gve_write_version(void __iomem *reg_bar)
{
	const char *c = gve_version_prefix;
	while (*c) {
		writeb(*c, reg_bar + GVE_DRIVER_VERSION);
		c++;
	}

	c = gve_version_str;
	while (*c) {
		writeb(*c, reg_bar + GVE_DRIVER_VERSION);
		c++;
	}
	writeb('\n', reg_bar + GVE_DRIVER_VERSION);
}

static int gve_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int max_tx_queues, max_rx_queues;
	struct net_device *dev;
	__be32 __iomem *db_bar;
	void __iomem *reg_bar;
	struct gve_priv *priv;
	int err;

	err = pci_enable_device(pdev);
	if (err)
		return -ENXIO;

	err = pci_request_regions(pdev, "gvnic-cfg");
	if (err)
		goto abort_with_enabled;

	pci_set_master(pdev);

	err = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (err) {
		dev_err(&pdev->dev, "Failed to set dma mask: err= %d\n", err);
		goto abort_with_pci_region;
	}

	err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
	if (err) {
		dev_err(&pdev->dev,
			"Failed to set consistent dma mask: err= %d\n", err);
		goto abort_with_pci_region;
	}

	reg_bar = pci_iomap(pdev, GVE_REGISTER_BAR, 0);
	if (!reg_bar) {
		err = -ENOMEM;
		goto abort_with_pci_region;
	}

	db_bar = pci_iomap(pdev, GVE_DOORBELL_BAR, 0);
	if (!db_bar) {
		dev_err(&pdev->dev, "Failed to map doorbell bar!\n");
		err = -ENOMEM;
		goto abort_with_reg_bar;
	}

	gve_write_version(reg_bar);
	/* Get max queues to alloc etherdev */
	max_rx_queues = min_t(u32,
			      be32_to_cpu(readl(reg_bar +
						GVE_DEVICE_MAX_RX_QUEUES)),
			      GVE_MAX_NUM_RX_QUEUES);
	max_tx_queues = min_t(u32,
			      be32_to_cpu(readl(reg_bar +
						GVE_DEVICE_MAX_TX_QUEUES)),
			      GVE_MAX_NUM_TX_QUEUES);
	/* Alloc and setup the netdev and priv */
	dev = alloc_etherdev_mqs(sizeof(*priv), max_tx_queues, max_rx_queues);
	if (!dev) {
		dev_err(&pdev->dev, "could not allocate netdev\n");
		goto abort_with_db_bar;
	}
	SET_NETDEV_DEV(dev, &pdev->dev);
	pci_set_drvdata(pdev, dev);
	dev->ethtool_ops = &gve_ethtool_ops;
	dev->netdev_ops = &gve_netdev_ops;
	/* advertise features */
	dev->hw_features = NETIF_F_HIGHDMA;
	dev->hw_features |= NETIF_F_SG;
	dev->hw_features |= NETIF_F_HW_CSUM;
	dev->hw_features |= NETIF_F_TSO;
	dev->hw_features |= NETIF_F_TSO6;
	dev->hw_features |= NETIF_F_TSO_ECN;
	dev->hw_features |= NETIF_F_RXCSUM;
	dev->hw_features |= NETIF_F_RXHASH;
	dev->features = dev->hw_features;

	err = register_netdev(dev);
	if (err)
		goto abort_with_netdev;
	netif_carrier_off(dev);
	priv = netdev_priv(dev);
	priv->dev = dev;
	priv->pdev = pdev;
	priv->msg_enable = DEFAULT_MSG_LEVEL;
	priv->reg_bar0 = reg_bar;
	priv->db_bar2 = db_bar;
	priv->service_task_flags = 0x0;
	priv->state_flags = 0x0;
	set_bit(GVE_PRIV_FLAGS_PROBE_IN_PROGRESS, &priv->service_task_flags);
	priv->gve_wq = alloc_ordered_workqueue("gve", 0);
	if (!priv->gve_wq) {
		dev_err(&pdev->dev, "could not allocate workqueue");
		err = -ENOMEM;
		goto abort_while_registered;
	}
	INIT_WORK(&priv->service_task, gve_service_task);
	priv->tx_cfg.max_queues = max_tx_queues;
	priv->rx_cfg.max_queues = max_rx_queues;

	err = gve_init_priv(priv);
	if (err)
		goto abort_with_wq;

	dev_info(&pdev->dev, "GVE version %s\n", gve_version_str);
	clear_bit(GVE_PRIV_FLAGS_PROBE_IN_PROGRESS, &priv->service_task_flags);
	queue_work(priv->gve_wq, &priv->service_task);
	return 0;

abort_with_wq:
	destroy_workqueue(priv->gve_wq);

abort_while_registered:
	unregister_netdev(dev);

abort_with_netdev:
	free_netdev(dev);

abort_with_db_bar:
	pci_iounmap(pdev, db_bar);

abort_with_reg_bar:
	pci_iounmap(pdev, reg_bar);

abort_with_pci_region:
	pci_release_regions(pdev);

abort_with_enabled:
	pci_disable_device(pdev);
	return -ENXIO;
}
EXPORT_SYMBOL(gve_probe);

static void gve_remove(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct gve_priv *priv = netdev_priv(netdev);
	__be32 __iomem *db_bar = priv->db_bar2;
	void __iomem *reg_bar = priv->reg_bar0;

	unregister_netdev(netdev);
	gve_teardown_priv_resources(priv);
	destroy_workqueue(priv->gve_wq);
	free_netdev(netdev);
	pci_iounmap(pdev, db_bar);
	pci_iounmap(pdev, reg_bar);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
}

static const struct pci_device_id gve_id_table[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_GOOGLE, PCI_DEV_ID_GVNIC) },
	{ }
};

static struct pci_driver gvnic_driver = {
	.name		= "gvnic",
	.id_table	= gve_id_table,
	.probe		= gve_probe,
	.remove		= gve_remove,
};

static int __init gvnic_init_module(void)
{
	return pci_register_driver(&gvnic_driver);
}

static void __exit gvnic_exit_module(void)
{
	pci_unregister_driver(&gvnic_driver);
}

module_init(gvnic_init_module);
module_exit(gvnic_exit_module);

MODULE_DEVICE_TABLE(pci, gve_id_table);
MODULE_AUTHOR("Google, Inc.");
MODULE_DESCRIPTION("gVNIC Driver");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION(GVE_VERSION);
