// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/* Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2015-2018 Google, Inc.
 */

#include <linux/etherdevice.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include "gve.h"
#include "gve_adminq.h"
#include "gve_register.h"

int gve_alloc_adminq(struct device *dev, struct gve_priv *priv)
{
	priv->adminq = dma_zalloc_coherent(dev, PAGE_SIZE,
					   &priv->adminq_bus_addr, GFP_KERNEL);
	if (unlikely(!priv->adminq))
		return -ENOMEM;

	spin_lock_init(&priv->adminq_lock);
	priv->adminq_mask = (PAGE_SIZE / sizeof(union gve_adminq_command)) - 1;
	priv->adminq_prod_cnt = 0;

	/* Setup Admin queue with the device */
	writeq(cpu_to_be32(priv->adminq_bus_addr / PAGE_SIZE),
	       priv->reg_bar0 + GVE_ADMIN_QUEUE_PFN);

	return 0;
}

void gve_free_adminq(struct device *dev, struct gve_priv *priv)
{
	/* Tell the device the adminq is leaving */
	writeq(0x0, priv->reg_bar0 + GVE_ADMIN_QUEUE_PFN);

	dma_free_coherent(dev, PAGE_SIZE, priv->adminq, priv->adminq_bus_addr);
}

static int gve_adminq_kick_cmd(struct gve_priv *priv)
{
	u32 prod_cnt = priv->adminq_prod_cnt;
	int i;

	writel(cpu_to_be32(prod_cnt),
	       priv->reg_bar0 + GVE_ADMIN_QUEUE_DOORBELL);

	for (i = 0; i < GVE_MAX_ADMINQ_EVENT_COUNTER_CHECK; i++) {
		if (be32_to_cpu(readl(priv->reg_bar0 +
		    GVE_ADMIN_QUEUE_EVENT_COUNTER))
		     == prod_cnt)
			return 0;
		msleep(20);
	}

	return -ETIME;
}

static int gve_parse_aq_err(struct device *dev, int err, u32 status)
{
	if (err)
		return err;

	if (status != GVE_ADMINQ_COMMAND_PASSED &&
	    status != GVE_ADMINQ_COMMAND_UNSET)
		dev_err(dev, "AQ command failed with status %d\n", status);

	switch (status) {
	case GVE_ADMINQ_COMMAND_PASSED:
		return 0;
	case GVE_ADMINQ_COMMAND_UNSET:
		dev_err(dev, "parse_aq_err: err and status both unset, this should not be possible.\n");
		return -EINVAL;
	case GVE_ADMINQ_COMMAND_ABORTED_ERROR:
	case GVE_ADMINQ_COMMAND_CANCELLED_ERROR:
	case GVE_ADMINQ_COMMAND_DATALOSS_ERROR:
	case GVE_ADMINQ_COMMAND_FAILED_PRECONDITION_ERROR:
	case GVE_ADMINQ_COMMAND_UNAVAILABLE_ERROR:
		return -EAGAIN;
	case GVE_ADMINQ_COMMAND_ALREADY_EXISTS_ERROR:
	case GVE_ADMINQ_COMMAND_INTERNAL_ERROR:
	case GVE_ADMINQ_COMMAND_INVALID_ARGUMENT_ERROR:
	case GVE_ADMINQ_COMMAND_NOT_FOUND_ERROR:
	case GVE_ADMINQ_COMMAND_OUT_OF_RANGE_ERROR:
	case GVE_ADMINQ_COMMAND_UNKNOWN_ERROR:
		return -EINVAL;
	case GVE_ADMINQ_COMMAND_DEADLINE_EXCEEDED_ERROR:
		return -ETIME;
	case GVE_ADMINQ_COMMAND_PERMISSION_DENIED_ERROR:
	case GVE_ADMINQ_COMMAND_UNAUTHENTICATED_ERROR:
		return -EACCES;
	case GVE_ADMINQ_COMMAND_RESOURCE_EXHAUSTED_ERROR:
		return -ENOMEM;
	case GVE_ADMINQ_COMMAND_UNIMPLEMENTED_ERROR:
		return -ENOTSUPP;
	default:
		dev_err(dev, "parse_aq_err: unknown status code %d\n", status);
		return -EINVAL;
	}
}

int gve_execute_adminq_cmd(struct gve_priv *priv,
			   union gve_adminq_command *cmd_orig)
{
	union gve_adminq_command *cmd;
	u32 status = 0;
	int err;

	spin_lock(&priv->adminq_lock);
	cmd = &priv->adminq[priv->adminq_prod_cnt & priv->adminq_mask];
	priv->adminq_prod_cnt++;

	memcpy(cmd, cmd_orig, sizeof(*cmd_orig));

	err = gve_adminq_kick_cmd(priv);
	if (err == -ETIME) {
		dev_err(&priv->pdev->dev, "AQ command timed out, need to reset AQ\n");
		err = -ENOTRECOVERABLE;
	} else {
		memcpy(cmd_orig, cmd, sizeof(*cmd));
	}

	spin_unlock(&priv->adminq_lock);
	status = be32_to_cpu(READ_ONCE(cmd->status));
	return gve_parse_aq_err(&priv->pdev->dev, err, status);
}

int gve_adminq_configure_device_resources(struct gve_priv *priv,
					  dma_addr_t counter_array_bus_addr,
					  int num_counters,
					  dma_addr_t db_array_bus_addr,
					  int num_ntfy_blks)
{
	union gve_adminq_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_CONFIGURE_DEVICE_RESOURCES);
	cmd.configure_device_resources =
		(struct gve_adminq_configure_device_resources) {
		.counter_array = cpu_to_be64(counter_array_bus_addr),
		.num_counters = cpu_to_be32(num_counters),
		.irq_db_addr = cpu_to_be64(db_array_bus_addr),
		.num_irq_dbs = cpu_to_be32(num_ntfy_blks),
		.irq_db_stride = cpu_to_be32(
			L1_CACHE_ALIGN(sizeof(priv->ntfy_blocks[0]))),
		.ntfy_blk_msix_base_idx =
			cpu_to_be32(priv->ntfy_blk_msix_base_idx),
	};

	return gve_execute_adminq_cmd(priv, &cmd);
}

int gve_adminq_deconfigure_device_resources(struct gve_priv *priv)
{
	union gve_adminq_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_DECONFIGURE_DEVICE_RESOURCES);

	return gve_execute_adminq_cmd(priv, &cmd);
}

int gve_adminq_create_tx_queue(struct gve_priv *priv, u32 queue_index)
{
	struct gve_tx_ring *tx = &priv->tx[queue_index];
	union gve_adminq_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_CREATE_TX_QUEUE);
	cmd.create_tx_queue = (struct gve_adminq_create_tx_queue) {
		.queue_id = cpu_to_be32(queue_index),
		.reserved = 0,
		.queue_resources_addr = cpu_to_be64(tx->q_resources_bus),
		.tx_ring_addr = cpu_to_be64(tx->bus),
		.queue_page_list_id = cpu_to_be32(tx->tx_fifo.qpl->id),
		.ntfy_id = cpu_to_be32(tx->ntfy_id),
	};

	return gve_execute_adminq_cmd(priv, &cmd);
}

int gve_adminq_create_rx_queue(struct gve_priv *priv, u32 queue_index)
{
	struct gve_rx_ring *rx = &priv->rx[queue_index];
	union gve_adminq_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_CREATE_RX_QUEUE);
	cmd.create_rx_queue = (struct gve_adminq_create_rx_queue) {
		.queue_id = cpu_to_be32(queue_index),
		.index = cpu_to_be32(queue_index),
		.reserved = 0,
		.ntfy_id = cpu_to_be32(rx->ntfy_id),
		.queue_resources_addr = cpu_to_be64(rx->q_resources_bus),
		.rx_desc_ring_addr = cpu_to_be64(rx->desc.bus),
		.rx_data_ring_addr = cpu_to_be64(rx->data.data_bus),
		.queue_page_list_id = cpu_to_be32(rx->data.qpl->id),
	};

	return gve_execute_adminq_cmd(priv, &cmd);
}

int gve_adminq_destroy_tx_queue(struct gve_priv *priv, u32 queue_index)
{
	union gve_adminq_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_DESTROY_TX_QUEUE);
	cmd.destroy_tx_queue = (struct gve_adminq_destroy_tx_queue) {
		.queue_id = cpu_to_be32(queue_index),
	};

	return gve_execute_adminq_cmd(priv, &cmd);
}

int gve_adminq_destroy_rx_queue(struct gve_priv *priv, u32 queue_index)
{
	union gve_adminq_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_DESTROY_RX_QUEUE);
	cmd.destroy_rx_queue = (struct gve_adminq_destroy_rx_queue) {
		.queue_id = cpu_to_be32(queue_index),
	};

	return gve_execute_adminq_cmd(priv, &cmd);
}

int gve_adminq_describe_device(struct gve_priv *priv)
{
	struct gve_device_descriptor *descriptor;
	union gve_adminq_command cmd;
	dma_addr_t descriptor_bus;
	int err = 0;
	u8 *mac;
	u16 mtu;

	memset(&cmd, 0, sizeof(cmd));
	descriptor = dma_zalloc_coherent(&priv->pdev->dev, PAGE_SIZE,
					 &descriptor_bus, GFP_KERNEL);
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_DESCRIBE_DEVICE);
	cmd.describe_device.device_descriptor_addr =
						cpu_to_be64(descriptor_bus);
	cmd.describe_device.device_descriptor_version =
			cpu_to_be32(GVE_ADMINQ_DEVICE_DESCRIPTOR_VERSION);
	cmd.describe_device.available_length = cpu_to_be32(PAGE_SIZE);

	err = gve_execute_adminq_cmd(priv, &cmd);
	if (err)
		goto free_device_descriptor;

	priv->tx_desc_cnt = be16_to_cpu(descriptor->tx_queue_entries);
	if (priv->tx_desc_cnt * sizeof(priv->tx->desc[0]) < PAGE_SIZE) {
		dev_err(&priv->pdev->dev, "Rx desc count %d too low\n",
			priv->rx_desc_cnt);
		err = -EINVAL;
		goto free_device_descriptor;
	}
	priv->rx_desc_cnt = be16_to_cpu(descriptor->rx_queue_entries);
	if (priv->rx_desc_cnt * sizeof(priv->rx->desc.desc_ring[0])
	    < PAGE_SIZE ||
	    priv->rx_desc_cnt * sizeof(priv->rx->data.data_ring[0])
	    < PAGE_SIZE) {
		dev_err(&priv->pdev->dev, "Rx desc count %d too low\n",
			priv->rx_desc_cnt);
		err = -EINVAL;
		goto free_device_descriptor;
	}
	priv->max_registered_pages =
				be64_to_cpu(descriptor->max_registered_pages);
	mtu = be16_to_cpu(descriptor->mtu);
	if (mtu < GVE_MIN_MTU) {
		dev_err(&priv->pdev->dev, "MTU %d below minimum MTU\n", mtu);
		err = -EINVAL;
		goto free_device_descriptor;
	}
	priv->max_mtu = mtu;
	priv->dev->mtu = mtu;
	priv->num_event_counters = be16_to_cpu(descriptor->counters);
	ether_addr_copy(priv->dev->dev_addr, descriptor->mac);
	mac = descriptor->mac;
	dev_info(&priv->pdev->dev, "MAC addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
		 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	priv->tx_pages_per_qpl = be16_to_cpu(descriptor->tx_pages_per_qpl);
	if (priv->tx_pages_per_qpl > GVE_TX_QPL_MAX_PAGES) {
		dev_info(&priv->pdev->dev, "TX pages per qpl %d more than maximum %d, defaulting to the maximum instead.\n",
			 priv->tx_pages_per_qpl, GVE_TX_QPL_MAX_PAGES);
		priv->tx_pages_per_qpl = GVE_TX_QPL_MAX_PAGES;
	}
	priv->rx_pages_per_qpl = be16_to_cpu(descriptor->rx_pages_per_qpl);
	if (priv->rx_pages_per_qpl > GVE_RX_QPL_MAX_PAGES) {
		dev_info(&priv->pdev->dev, "RX pages per qpl %d more than maximum %d, defaulting to the maximum instead.\n",
			 priv->rx_pages_per_qpl, GVE_RX_QPL_MAX_PAGES);
		priv->rx_pages_per_qpl = GVE_RX_QPL_MAX_PAGES;
	}
	priv->default_num_queues = be16_to_cpu(descriptor->default_num_queues);

free_device_descriptor:
	dma_free_coherent(&priv->pdev->dev, sizeof(*descriptor), descriptor,
			  descriptor_bus);
	return err;
}

int gve_adminq_register_page_list(struct gve_priv *priv,
				  struct gve_queue_page_list *qpl)
{
	struct device *hdev = &priv->pdev->dev;
	int num_entries = qpl->num_entries;
	int size = num_entries * sizeof(qpl->page_buses[0]);
	union gve_adminq_command cmd;
	dma_addr_t page_list_bus;
	__be64 *page_list;
	int err;
	int i;

	memset(&cmd, 0, sizeof(cmd));
	page_list = dma_zalloc_coherent(hdev, size, &page_list_bus, GFP_KERNEL);
	if (!page_list)
		return -ENOMEM;

	for (i = 0; i < num_entries; i++)
		page_list[i] = cpu_to_be64(qpl->page_buses[i]);

	cmd.opcode = cpu_to_be32(GVE_ADMINQ_REGISTER_PAGE_LIST);
	cmd.reg_page_list = (struct gve_adminq_register_page_list) {
		.page_list_id = cpu_to_be32(qpl->id),
		.num_pages = cpu_to_be32(num_entries),
		.page_address_list_addr = cpu_to_be64(page_list_bus),
	};

	err = gve_execute_adminq_cmd(priv, &cmd);
	dma_free_coherent(hdev, size, page_list, page_list_bus);
	return err;
}

int gve_adminq_unregister_page_list(struct gve_priv *priv, u32 page_list_id)
{
	union gve_adminq_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_UNREGISTER_PAGE_LIST);
	cmd.unreg_page_list = (struct gve_adminq_unregister_page_list) {
		.page_list_id = cpu_to_be32(page_list_id),
	};

	return gve_execute_adminq_cmd(priv, &cmd);
}
