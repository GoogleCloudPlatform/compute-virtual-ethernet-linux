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

#ifndef _GVE_H_
#define _GVE_H_

#include <linux/atomic.h>
#include <linux/netdevice.h>
#include <linux/dma-mapping.h>
#include <linux/pci.h>
#include <linux/rtnetlink.h>
#include <net/tcp.h>
#include "gve_desc.h"

#ifndef PCI_VENDOR_ID_GOOGLE
#define PCI_VENDOR_ID_GOOGLE	0x1ae0
#endif

#define PCI_DEV_ID_GVNIC	0x0042

#define GVE_REGISTER_BAR	0
#define GVE_DOORBELL_BAR	2

#define GVE_MAX_NUM_TX_QUEUES	1024
#define GVE_MAX_NUM_RX_QUEUES	256
#define GVE_TX_QPL_MAX_PAGES	512
#define GVE_RX_QPL_MAX_PAGES	1024

struct gve_rx_desc_queue {
	struct gve_rx_desc *desc_ring;
	dma_addr_t bus;
	int cnt;
	u32 fill_cnt;
	int mask;
	u8 seqno;
};

struct gve_rx_slot_page_info {
	struct page *page;
	void *page_address;
	unsigned int page_offset;
};

struct gve_queue_page_list {
	u32 id;
	u32 num_entries;
	struct page **pages;
	dma_addr_t *page_buses;
	void **page_ptrs;
};

struct gve_rx_data_queue {
	struct gve_rx_data_slot *data_ring;
	dma_addr_t data_bus; /* dma mapping of the slots */
	struct gve_rx_slot_page_info *page_info;
	/* Recycling is picked up on Fridays. */
	/*void *recycle_pages; */
	struct gve_queue_page_list *qpl;
	int mask;
	int cnt;
};

struct gve_priv;

struct gve_rx_ring {
	struct gve_priv *gve;
	struct gve_rx_desc_queue desc;
	struct gve_rx_data_queue data;
	unsigned long rbytes;
	unsigned long rpackets;
	int q_num;
	u32 ntfy_id;
	struct gve_queue_resources *q_resources;
	dma_addr_t q_resources_bus;
};

union gve_tx_desc {
	struct gve_tx_pkt_desc pkt;
	struct gve_tx_seg_desc seg;
};

struct gve_tx_iovec {
	/* References coherent DMA memory */
	void *iov_base;
	/* Offset into this segment */
	u32 iov_offset;
	u32 iov_len;
	u32 iov_padding;
};

/* TODO: Shrink this structure to <= 64b (cacheline) */
struct gve_tx_buffer_state {
	struct sk_buff *skb;
	struct gve_tx_iovec iov[4];
};

struct gve_tx_fifo {
	void *base;
	u32 size;
	atomic_t available;
	u32 head;
	struct gve_queue_page_list *qpl;
};

struct gve_tx_ring {
	/* Cacheline 0 -- Accessed & dirtied during transmit */
	struct gve_tx_fifo tx_fifo;
	u32 req;
	u32 done;

	/* Cacheline 1 -- Accessed & dirtied during gve_clean_tx_done */
	__be32 last_nic_done ____cacheline_aligned;
	unsigned long pkt_done;
	unsigned long bytes_done;

	/* Cacheline 2 -- Read-mostly fields */
	union gve_tx_desc *desc ____cacheline_aligned;
	struct gve_tx_buffer_state *info;
	struct netdev_queue *netdev_txq;
	struct gve_queue_resources *q_resources;
	u32 mask;

	/* Slow-path fields */
	int q_num;
	int stop_queue;
	int wake_queue;
	u32 ntfy_id;
	dma_addr_t bus;
	dma_addr_t q_resources_bus;
} ____cacheline_aligned;

/* 1 for management, 1 for rx, 1 for tx */
#define GVE_MIN_MSIX 3

struct gve_notify_block {
	volatile __be32 irq_db_index; /* Set by device - must be first field */
	char name[IFNAMSIZ + 16];
	struct napi_struct napi;
	int napi_enabled;
	struct gve_priv *priv;
	struct gve_tx_ring *tx;
	struct gve_rx_ring *rx;
} ____cacheline_aligned;

#define GVE_MIN_MTU			(68)

struct gve_queue_config {
	int max_queues;
	int num_queues;
};

struct gve_qpl_config {
	int qpl_map_size;
	unsigned long *qpl_id_map;
};

struct gve_priv {
	struct net_device *dev;
	struct gve_tx_ring *tx; /* array of tx_cfg.num_queues */
	struct gve_rx_ring *rx; /* array of rx_cfg.num_queues */
	struct gve_queue_page_list *qpls; /* array of num qpls */
	struct gve_notify_block *ntfy_blocks; /* array of num_ntfy_blks */
	dma_addr_t ntfy_block_bus;
	struct msix_entry *msix_vectors; /* array of num_ntfy_blks + 1 */
	char mgmt_msix_name[IFNAMSIZ + 16];
	int mgmt_msix_idx;
	int ntfy_blk_msix_base_idx;
	__be32 *counter_array; /* array of num_event_counters */
	dma_addr_t counter_array_bus;

	bool is_up;
	int num_event_counters;
	int tx_desc_cnt;
	int rx_desc_cnt;
	int tx_pages_per_qpl;
	int rx_pages_per_qpl;
	u64 max_registered_pages;
	u64 num_registered_pages;
	int rx_copybreak;
	int max_mtu;
	int max_num_slices;

	struct gve_queue_config tx_cfg;
	struct gve_queue_config rx_cfg;
	struct gve_qpl_config qpl_cfg;
	int num_ntfy_blks;

	void __iomem *reg_bar0;
	__be32 __iomem *db_bar2; /* "array" of doorbells */
	u32 msg_enable;	/* level for netif* netdev print macros	*/
	struct pci_dev *pdev;

	/* Admin queue */
	spinlock_t adminq_lock;
	union gve_adminq_command *adminq;
	dma_addr_t adminq_bus_addr;
	int adminq_mask;
	u32 adminq_prod_cnt;

	struct workqueue_struct *gve_wq;
	struct work_struct service_task;
	unsigned long flags;
};

#define GVE_PRIV_FLAGS_IGNORE_FLOW_TABLE	BIT(0)
#define GVE_PRIV_FLAGS_DO_AQ_RESET		BIT(1)
#define GVE_PRIV_FLAGS_DO_PCI_RESET		BIT(2)
#define GVE_PRIV_FLAGS_PROBE_IN_PROGRESS	BIT(5)

static inline __be32 __iomem *gve_irq_doorbell(struct gve_priv *priv,
					   struct gve_notify_block *block)
{
	u32 irq_db_index = be32_to_cpu(smp_load_acquire(&block->irq_db_index));

	return &priv->db_bar2[irq_db_index];
}

/**
  * Returns the index into ntfy_blocks of the given rx ring's block
 **/
static inline u32 gve_tx_ntfy_idx(struct gve_priv *priv, u32 queue_idx)
{
	return queue_idx;
}

/**
  * Returns the index into ntfy_blocks of the given rx ring's block
 **/
static inline u32 gve_rx_ntfy_idx(struct gve_priv *priv, u32 queue_idx)
{
	return priv->tx_cfg.max_queues + queue_idx;
}

/**
  * Returns the number of tx queue page lists
 **/
static inline u32 gve_num_tx_qpls(struct gve_priv *priv)
{
	return priv->tx_cfg.num_queues;
}

/**
  * Returns the number of rx queue page lists
 **/
static inline u32 gve_num_rx_qpls(struct gve_priv *priv)
{
	return priv->rx_cfg.num_queues;
}

/**
 * Returns a pointer to the next available tx qpl in the list of qpls
 **/
static inline struct gve_queue_page_list *gve_assign_tx_qpl(
							struct gve_priv *priv)
{
	int id = find_first_zero_bit(priv->qpl_cfg.qpl_id_map,
				     priv->qpl_cfg.qpl_map_size);

	/* we are out of tx qpls */
	if (id >= gve_num_tx_qpls(priv))
		return NULL;

	set_bit(id, priv->qpl_cfg.qpl_id_map);
	return &priv->qpls[id];
}

/**
 * Returns a pointer to the next available rx qpl in the list of qpls
 **/
static inline struct gve_queue_page_list *gve_assign_rx_qpl(
							struct gve_priv *priv)
{
	int id = find_next_zero_bit(priv->qpl_cfg.qpl_id_map,
				    priv->qpl_cfg.qpl_map_size,
				    gve_num_tx_qpls(priv));

	/* we are out of rx qpls */
	if (id == priv->qpl_cfg.qpl_map_size)
		return NULL;

	set_bit(id, priv->qpl_cfg.qpl_id_map);
	return &priv->qpls[id];
}

/**
 * Unassigns the qpl with the given id
 **/
static inline void gve_unassign_qpl(struct gve_priv *priv, int id)
{
	clear_bit(id, priv->qpl_cfg.qpl_id_map);
}

void gve_add_napi(struct gve_priv *priv, struct gve_notify_block *block);
void gve_remove_napi(struct gve_notify_block *block);
/* tx handling */
netdev_tx_t gve_tx(struct sk_buff *skb, struct net_device *dev);
int gve_clean_tx_done(struct gve_priv *priv, struct gve_tx_ring *tx,
		      u32 nic_done);
bool gve_tx_poll(struct gve_notify_block *block, int budget);
int gve_tx_alloc_rings(struct gve_priv *priv);
void gve_tx_free_rings(struct gve_priv *priv);
__be32 gve_tx_load_event_counter(struct gve_priv *priv,
				 struct gve_tx_ring *tx);
/* rx handling */
void gve_rx_write_doorbell(struct gve_priv *priv, struct gve_rx_ring *rx);
bool gve_rx_poll(struct gve_notify_block *block, int budget);
int gve_rx_alloc_rings(struct gve_priv *priv);
void gve_rx_free_rings(struct gve_priv *priv);
bool gve_clean_rx_done(struct gve_rx_ring *rx, int budget,
			      netdev_features_t feat);
/* Resets */
void gve_schedule_aq_reset(struct gve_priv *priv);
void gve_schedule_pci_reset(struct gve_priv *priv);
int gve_adjust_queues(struct gve_priv *priv,
		      struct gve_queue_config new_rx_config,
		      struct gve_queue_config new_tx_config);
/* exported by ethtool.c */
extern const struct ethtool_ops gve_ethtool_ops;
/* needed by ethtool */
extern const char gve_version_str[];
#endif /* _GVE_H_ */
