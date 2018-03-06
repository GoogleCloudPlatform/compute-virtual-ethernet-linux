/*
 * Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2015-2018 Google, Inc.
 *
 * This software is available to you under a choice of one of two licenses. You
 * may choose to be licensed under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation, and may be copied,
 * distributed, and modified under those terms. See the GNU General Public
 * License for more details. Otherwise you may choose to be licensed under the
 * terms of the MIT license below.
 *
 * --------------------------------------------------------------------------
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
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
#define GVE_VERSION		"0.0.1"

const char gve_version_str[] = GVE_VERSION;

static void gve_get_stats(struct net_device *dev, struct rtnl_link_stats64 *s)
{
	struct gve_priv *priv = netdev_priv(dev);

	if (priv->rx) {
		s->rx_packets += priv->rx->rpackets;
		s->rx_bytes += priv->rx->rbytes;
	}
	if (priv->tx) {
		s->tx_packets += priv->tx->pkt_done;
		s->tx_bytes += priv->tx->bytes_done;
	}
	/* TODO(csully): Start counting mcast, bcast, drops, etc? */
}

static int gve_alloc_counter_array(struct gve_priv *priv)
{
	/* TODO(csully): Figure out behavior of this on ARM/Power */
	priv->counter_array = dma_zalloc_coherent(&priv->pdev->dev,
		priv->num_event_counters * sizeof(__be32),
		&priv->counter_array_bus, GFP_KERNEL);
	if (!priv->counter_array)
		return -ENOMEM;

	return 0;
}

static void gve_free_counter_array(struct gve_priv *priv)
{
	dma_free_coherent(&priv->pdev->dev,
			  priv->num_event_counters * sizeof(__be32),
			  priv->counter_array, priv->counter_array_bus);
	priv->counter_array = NULL;
}

static irqreturn_t gve_mgmnt_intr(int irq, void *arg)
{
	struct gve_priv *priv = arg;

	queue_work(priv->gve_wq, &priv->service_task);
	/* TODO(venkateshs): We need to ack the interrupt here; can't do so
	 * until vector 0's interrupt doorbell irq_state setup correctly in the
	 * virtual device.
	 */
	return IRQ_HANDLED;
}

static irqreturn_t gve_intr(int irq, void *arg)
{
	struct gve_notify_block *block = arg;

	if (!block->napi_enabled)
		return IRQ_HANDLED;

	napi_schedule_irqoff(&block->napi);
	return IRQ_HANDLED;
}

int gve_napi_poll(struct napi_struct *napi, int budget)
{
	struct gve_notify_block *block;
	__be32 __iomem *irq_doorbell;
	int reschedule = false;
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
		__raw_writel(cpu_to_be32(GVE_IRQ_MASK), irq_doorbell);

	return 0;
}

static int gve_alloc_notify_blocks(struct gve_priv *priv)
{
	struct device *hdev = &priv->pdev->dev;
	struct gve_notify_block *block;
	char *name = priv->dev->name;
	int vecs_enabled;
	int msix_idx;
	int err;
	int i;

	priv->msix_vectors = kcalloc(GVE_NUM_MSIX,
				  sizeof(*priv->msix_vectors), GFP_KERNEL);
	if (!priv->msix_vectors)
		return -ENOMEM;
	for (i = 0; i < GVE_NUM_MSIX; i++)
		priv->msix_vectors[i].entry = i;
	vecs_enabled = pci_enable_msix_range(priv->pdev, priv->msix_vectors,
					     GVE_NUM_MSIX, GVE_NUM_MSIX);
	if (vecs_enabled < 0) {
		dev_err(hdev, "Could not enable min msix %d/%d\n",
			GVE_NUM_MSIX, vecs_enabled);
		err = vecs_enabled;
		goto abort_with_msix_vectors;
	}

	/* Setup Management Vector */
	snprintf(priv->mgmt_msix_name, sizeof(priv->mgmt_msix_name), "%s-mgmnt",
		 name);
	err = request_irq(priv->msix_vectors[priv->mgmt_msix_idx].vector,
			  gve_mgmnt_intr, 0, priv->mgmt_msix_name, priv);
	if (err) {
		dev_err(hdev, "Did not receive managment vector.\n");
		goto abort_with_msix_enabled;
	}

	/* Setup the notification block */
	priv->ntfy_block = dma_zalloc_coherent(&priv->pdev->dev,
					       sizeof(*priv->ntfy_block),
					       &priv->ntfy_block_bus,
					       GFP_KERNEL);
	if (!priv->ntfy_block) {
		dev_err(hdev, "Failed to allocate notification blocks\n");
		err = -ENOMEM;
		goto abort_with_mgmt_vector;
	}
	msix_idx = priv->ntfy_blk_msix_base_idx;
	block = priv->ntfy_block;
	snprintf(block->name, sizeof(block->name), "%s-ntfy-block",
		 name);
	block->priv = priv;
	err = request_irq(priv->msix_vectors[msix_idx].vector,
			  gve_intr, 0, block->name, block);
	if (err) {
		dev_err(hdev, "Failed to receive notify block msix vector\n");
		goto abort_with_ntfy_block;
	}
	irq_set_affinity_hint(priv->msix_vectors[msix_idx].vector,
			      get_cpu_mask(0));
	block->tx = NULL;
	block->rx = NULL;
	block->napi_enabled = 0;
	return 0;

abort_with_ntfy_block:
	dma_free_coherent(&priv->pdev->dev,
			  sizeof(*priv->ntfy_block),
			  priv->ntfy_block, priv->ntfy_block_bus);
	priv->ntfy_block = NULL;
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
	/* Free the irqs */
	struct gve_notify_block *block = priv->ntfy_block;
	int msix_idx = priv->ntfy_blk_msix_base_idx;

	irq_set_affinity_hint(priv->msix_vectors[msix_idx].vector, NULL);
	free_irq(priv->msix_vectors[msix_idx].vector, block);
	dma_free_coherent(&priv->pdev->dev,
			  sizeof(*priv->ntfy_block),
			  priv->ntfy_block, priv->ntfy_block_bus);
	priv->ntfy_block = NULL;
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
						    priv->ntfy_block_bus);
	if (unlikely(err)) {
		dev_err(&priv->pdev->dev,
			"could not setup device_resources: err=%d\n", err);
		err = -ENXIO;
		goto abort_with_ntfy_block;
	}
	return 0;
abort_with_ntfy_block:
	gve_free_notify_blocks(priv);
abort_with_counter:
	gve_free_counter_array(priv);
	return err;
}

static void gve_teardown_device_resources(struct gve_priv *priv)
{
	int err;

	/* Tell device its resources are being freed */
	err = gve_adminq_deconfigure_device_resources(priv);
	WARN(err, "GVE device resources not released\n");
	gve_free_counter_array(priv);
	gve_free_notify_blocks(priv);
}

void gve_add_napi(struct gve_priv *priv, struct gve_notify_block *block)
{
	netif_napi_add(priv->dev, &block->napi, gve_napi_poll,
		       NAPI_POLL_WEIGHT);
	napi_enable(&block->napi);
	block->napi_enabled = 1;
}

void gve_remove_napi(struct gve_notify_block *block)
{
	if (block->napi_enabled) {
		napi_disable(&block->napi);
		block->napi_enabled = 0;
		netif_napi_del(&block->napi);
	}
}

static int gve_register_qpls(struct gve_priv *priv)
{
	int err;
	int i;

	for (i = 0; i < priv->num_qpls; i++) {
		err = gve_adminq_register_page_list(priv, &priv->qpls[i]);
		if (err) {
			netdev_err(priv->dev, "failed to register queue page list %d\n",
				   priv->qpls[i].id);
			return err;
		}
	}
	return 0;
}

static int gve_unregister_qpls(struct gve_priv *priv)
{
	int err;
	int i;

	for (i = 0; i < priv->num_qpls; i++) {
		err = gve_adminq_unregister_page_list(priv, priv->qpls[i].id);
		if (err)
			return err;
	}
	return 0;
}

static int gve_create_rings(struct gve_priv *priv)
{
	int err = 0;

	err = gve_adminq_create_tx_queue(priv);
	if (err) {
		netdev_err(priv->dev, "failed to create tx queue\n");
		return err;
	}
	netdev_dbg(priv->dev, "created tx queue\n");
	err = gve_adminq_create_rx_queue(priv);
	if (err) {
		netdev_err(priv->dev, "failed to create rx queue\n");
		return err;
	}
	/* Rx data ring has been prefilled with packet buffers at
	 * queue allocation time.
	 * Write the doorbell to provide descriptor slots and packet
	 * buffers to the NIC.
	 */
	gve_rx_write_doorbell(priv, priv->rx);
	netdev_dbg(priv->dev, "created rx queue\n");

	return 0;

}

static int gve_alloc_rings(struct gve_priv *priv)
{
	int err;

	priv->tx = kcalloc(1, sizeof(*priv->tx), GFP_KERNEL);
	if (!priv->tx)
		return -ENOMEM;
	err = gve_tx_alloc_ring(priv);
	if (err)
		goto free_tx;
	/* Setup rx rings */
	priv->rx = kcalloc(1, sizeof(*priv->rx), GFP_KERNEL);
	if (!priv->rx) {
		err = -ENOMEM;
		goto free_tx_queue;
	}
	err = gve_rx_alloc_ring(priv);
	if (err)
		goto free_rx;

	return 0;

free_rx:
	kfree(priv->rx);
free_tx_queue:
	gve_tx_free_ring(priv);
free_tx:
	kfree(priv->tx);
	return err;
}

static int gve_destroy_rings(struct gve_priv *priv)
{
	int err;

	err = gve_adminq_destroy_tx_queue(priv);
	if (err) {
		netdev_err(priv->dev, "failed to destroy tx queue.\n");
		return err;
	}
	err = gve_adminq_destroy_rx_queue(priv);
	if (err) {
		netdev_err(priv->dev, "failed to destroy rx queue.\n");
		return err;
	}
	return 0;
}

static void gve_free_rings(struct gve_priv *priv)
{
	if (priv->tx) {
		gve_tx_free_ring(priv);
		kfree(priv->tx);
	}
	if (priv->rx) {
		gve_rx_free_ring(priv);
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
	int err;

	priv->qpls = kcalloc(2, sizeof(*priv->qpls), GFP_KERNEL);
	if (!priv->qpls)
		return -ENOMEM;
	err = gve_alloc_queue_page_list(priv, GVE_TX_QPL_ID,
					priv->tx_pages_per_qpl);
	if (err)
		goto free_qpls;
	err = gve_alloc_queue_page_list(priv, GVE_RX_QPL_ID,
					priv->rx_pages_per_qpl);
	if (err)
		goto free_tx_qpl;

	return 0;

free_tx_qpl:
	gve_free_queue_page_list(priv, GVE_TX_QPL_ID);
free_qpls:
	kfree(priv->qpls);
	return err;
}

static void gve_free_qpls(struct gve_priv *priv)
{
	gve_free_queue_page_list(priv, GVE_TX_QPL_ID);
	gve_free_queue_page_list(priv, GVE_RX_QPL_ID);
	kfree(priv->qpls);
}

void gve_schedule_aq_reset(struct gve_priv *priv)
{
	set_bit(GVE_PRIV_FLAGS_DO_AQ_RESET, &priv->flags);
	queue_work(priv->gve_wq, &priv->service_task);
}

void gve_schedule_pci_reset(struct gve_priv *priv)
{
	set_bit(GVE_PRIV_FLAGS_DO_PCI_RESET, &priv->flags);
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

static int gve_open(struct net_device *dev)
{
	struct gve_priv *priv = netdev_priv(dev);
	int err = 0;

	err = gve_alloc_qpls(priv);
	if (err)
		return err;
	err = gve_alloc_rings(priv);
	if (err) {
		gve_free_qpls(priv);
		return err;
	}
	netif_set_real_num_tx_queues(dev, 1);
	netif_set_real_num_rx_queues(dev, 1);

	err = gve_register_qpls(priv);
	if (err) {
		gve_schedule_aq_reset(priv);
		return err;
	}
	err = gve_create_rings(priv);
	if (err) {
		gve_schedule_aq_reset(priv);
		return err;
	}

	netif_carrier_on(dev);
	priv->is_up = true;

	return 0;
}

static int gve_close(struct net_device *dev)
{
	struct gve_priv *priv = netdev_priv(dev);
	int err;

	netif_carrier_off(dev);
	priv->is_up = false;
	err = gve_destroy_rings(priv);
	if (err)
		gve_schedule_aq_reset(priv);
	err = gve_unregister_qpls(priv);
	if (err)
		gve_schedule_aq_reset(priv);
	gve_free_rings(priv);
	gve_free_qpls(priv);

	return err;
}

static void gve_turndown_queues(struct gve_priv *priv)
{
	struct gve_tx_ring *tx_ring = priv->tx;

	/* Turn off napi so no new work comes in */
	gve_remove_napi(priv->ntfy_block);

	gve_clean_tx_done(priv, tx_ring, tx_ring->req);
	tx_ring->done = 0;
	tx_ring->req = 0;
	netdev_tx_reset_queue(tx_ring->netdev_txq);

	gve_clean_rx_done(priv->rx, 0, priv->ntfy_block->napi.dev->features);
}

static void gve_turnup_queues(struct gve_priv *priv)
{
	/* Turn napi back on if the block has queues */
	struct gve_notify_block *block = priv->ntfy_block;

	if (block->rx || block->tx)
		gve_add_napi(priv, block);
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
		dev_err(&priv->pdev->dev, "could not count MSI-x vectors\n");
		err = num_ntfy;
		goto abort_with_adminq;
	} else if (num_ntfy < GVE_NUM_MSIX) {
		dev_err(&priv->pdev->dev, "gve needs at least 2 MSI-x vectors, but only has %d\n",
			num_ntfy);
		err = -EINVAL;
		goto abort_with_adminq;
	}

	priv->mgmt_msix_idx = 1;
	priv->ntfy_blk_msix_base_idx = 0;
	priv->num_registered_pages = 0;
	priv->num_qpls = 2;
	priv->rx_copybreak = GVE_DEFAULT_RX_COPYBREAK;

	err = gve_setup_device_resources(priv);
	if (err)
		goto abort_with_adminq;
	return 0;

abort_with_adminq:
	gve_free_adminq(&priv->pdev->dev, priv);
	return err;
}

static void gve_reset_pci(struct gve_priv *priv)
{
	bool was_up = priv->is_up;
	int err;

	dev_info(&priv->pdev->dev, "Performing pci reset\n");
	clear_bit(GVE_PRIV_FLAGS_DO_PCI_RESET, &priv->flags);
	if (was_up) {
		gve_turndown_queues(priv);
		priv->is_up = false;
	}

	/* Reset the device */
	pci_reset_function(priv->pdev);

	/* Teardown all our device resources */
	gve_free_rings(priv);
	gve_free_notify_blocks(priv);
	gve_free_counter_array(priv);
	gve_free_adminq(&priv->pdev->dev, priv);

	err = gve_init_priv(priv);
	if (err)
		goto err;
	if (was_up) {
		err = gve_open(priv->dev);
		if (err)
			goto err;
	}
	return;
err:
	dev_err(&priv->pdev->dev, "PCI reset failed! !!! DISABLING ALL QUEUES !!!\n");
	if (was_up) {
		gve_turndown_queues(priv);
		priv->is_up = false;
	}
}

static void gve_reset_aq(struct gve_priv *priv)
{
	bool was_up = priv->is_up;
	int err;

	dev_info(&priv->pdev->dev, "Perfmorming AQ reset\n");
	clear_bit(GVE_PRIV_FLAGS_DO_AQ_RESET, &priv->flags);
	if (was_up) {
		gve_turndown_queues(priv);
		priv->is_up = false;
	}

	/* Reset the device by deallocating the AQ */
	gve_free_adminq(&priv->pdev->dev, priv);

	/* Tell the AQ where everything is again */
	err = gve_alloc_adminq(&priv->pdev->dev, priv);
	if (err)
		goto pci_reset;
	err = gve_adminq_configure_device_resources(priv,
						    priv->counter_array_bus,
						    priv->num_event_counters,
						    priv->ntfy_block_bus);
	if (err)
		goto pci_reset;
	if (was_up) {
		err = gve_create_rings(priv);
		if (err)
			goto pci_reset;
		gve_turnup_queues(priv);
		priv->is_up = true;
	}
	return;

pci_reset:
	dev_err(&priv->pdev->dev, "AQ reset failed, trying PCI reset\n");
	set_bit(GVE_PRIV_FLAGS_DO_PCI_RESET, &priv->flags);
	queue_work(priv->gve_wq, &priv->service_task);
}

static void gve_handle_status(struct gve_priv *priv, u32 status)
{
	if (GVE_DEVICE_STATUS_RESET_MASK & status) {
		dev_info(&priv->pdev->dev, "Device requested request.\n");
		set_bit(GVE_PRIV_FLAGS_DO_PCI_RESET, &priv->flags);
	}
}

static void gve_handle_reset(struct gve_priv *priv)
{
	/* A service task will be scheduled at the end of probe to catch any
	 * resets that need to happen, and we don't want to reset until
	 * probe is done.
	 */
	if (test_bit(GVE_PRIV_FLAGS_PROBE_IN_PROGRESS, &priv->flags))
		return;

	if (test_bit(GVE_PRIV_FLAGS_DO_PCI_RESET, &priv->flags)) {
		/* PCI reset supercedes AQ reset */
		clear_bit(GVE_PRIV_FLAGS_DO_AQ_RESET, &priv->flags);
		rtnl_lock();
		gve_reset_pci(priv);
		rtnl_unlock();
	}

	if (test_bit(GVE_PRIV_FLAGS_DO_AQ_RESET, &priv->flags)) {
		rtnl_lock();
		gve_reset_aq(priv);
		rtnl_unlock();
	}
}

/* Handle NIC status register changes and reset requests */
static void gve_service_task(struct work_struct *work)
{
	struct gve_priv *priv = container_of(work, struct gve_priv,
					     service_task);

	gve_handle_status(priv, be32_to_cpu(readl(priv->reg_bar0 +
						  GVE_DEVICE_STATUS)));

	gve_handle_reset(priv);
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

static int gve_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
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

	reg_bar = pci_iomap(pdev, GVE_REGISTER_BAR, 0);
	if (!reg_bar)
		goto abort_with_pci_region;

	db_bar = pci_iomap(pdev, GVE_DOORBELL_BAR, 0);
	if (!db_bar) {
		dev_err(&pdev->dev, "Failed to map doorbell bar!\n");
		goto abort_with_reg_bar;
	}
	pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));

	gve_write_version(reg_bar);

	/* Alloc and setup the netdev and priv */
	dev = alloc_etherdev_mqs(sizeof(*priv), 1, 1);
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
	priv->flags = 0x0;
	priv->is_up = false;
	set_bit(GVE_PRIV_FLAGS_PROBE_IN_PROGRESS, &priv->flags);
	priv->gve_wq = alloc_ordered_workqueue("gve", 0);
	if (!priv->gve_wq) {
		dev_err(&pdev->dev, "could not allocate workqueue");
		goto abort_while_registered;
	}
	INIT_WORK(&priv->service_task, gve_service_task);
	err = gve_init_priv(priv);
	if (err)
		goto abort_with_wq;

	dev_info(&pdev->dev, "GVE version %s\n", gve_version_str);
	clear_bit(GVE_PRIV_FLAGS_PROBE_IN_PROGRESS, &priv->flags);
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
	struct net_device *dev = pci_get_drvdata(pdev);
	struct gve_priv *priv = netdev_priv(dev);
	__be32 __iomem *db_bar = priv->db_bar2;
	void __iomem *reg_bar = priv->reg_bar0;

	unregister_netdev(dev);
	gve_teardown_device_resources(priv);
	gve_free_adminq(&pdev->dev, priv);
	destroy_workqueue(priv->gve_wq);
	free_netdev(dev);
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
