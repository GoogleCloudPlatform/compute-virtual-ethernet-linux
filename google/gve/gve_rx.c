// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/* Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2015-2019 Google, Inc.
 */

#include "gve.h"
#include "gve_adminq.h"
#include <linux/etherdevice.h>

static void gve_rx_remove_from_block(struct gve_priv *priv, int queue_idx)
{
	struct gve_notify_block *block =
			&priv->ntfy_blocks[gve_rx_idx_to_ntfy(priv, queue_idx)];

	block->rx = NULL;
}

static void gve_rx_free_ring(struct gve_priv *priv, int idx)
{
	struct gve_rx_ring *rx = &priv->rx[idx];
	struct device *dev = &priv->pdev->dev;
	size_t bytes;
	u32 slots = rx->mask + 1;

	gve_rx_remove_from_block(priv, idx);

	bytes = sizeof(struct gve_rx_desc) * priv->rx_desc_cnt;
	dma_free_coherent(dev, bytes, rx->desc.desc_ring, rx->desc.bus);
	rx->desc.desc_ring = NULL;

	dma_free_coherent(dev, sizeof(*rx->q_resources),
			  rx->q_resources, rx->q_resources_bus);
	rx->q_resources = NULL;

	if (rx->data.raw_addressing) {
		int i;

		for (i = 0; i < slots; i++) {
			struct gve_rx_slot_page_info *page_info =
							&rx->data.page_info[i];
			struct gve_rx_data_slot *data_slot =
							&rx->data.data_ring[i];
			dma_addr_t dma =
				(dma_addr_t)(be64_to_cpu(data_slot->addr) -
				page_info->page_offset);

			gve_free_page(dev, rx->data.page_info[i].page, dma,
				      DMA_FROM_DEVICE);
		}
	} else {
		gve_unassign_qpl(priv, rx->data.qpl->id);
		rx->data.qpl = NULL;
	}
	kfree(rx->data.page_info);

	bytes = sizeof(*rx->data.data_ring) * slots;
	dma_free_coherent(dev, bytes, rx->data.data_ring,
			  rx->data.data_bus);
	rx->data.data_ring = NULL;
	netif_dbg(priv, drv, priv->dev, "freed rx ring %d\n", idx);
}

static void gve_setup_rx_buffer(struct gve_rx_slot_page_info *page_info,
				struct gve_rx_data_slot *slot,
				dma_addr_t addr, struct page *page)
{
	page_info->page = page;
	page_info->page_offset = 0;
	page_info->page_address = page_address(page);
	slot->addr = cpu_to_be64(addr);
}

static int gve_prefill_rx_pages(struct gve_rx_ring *rx)
{
	struct gve_priv *priv = rx->gve;
	u32 slots;
	int err;
	int i;

	/* Allocate one page per Rx queue slot. Each page is split into two
	 * packet buffers, when possible we "page flip" between the two.
	 */
	slots = rx->mask + 1;

	rx->data.page_info = kvzalloc(slots *
				      sizeof(*rx->data.page_info), GFP_KERNEL);
	if (!rx->data.page_info)
		return -ENOMEM;

	if (!rx->data.raw_addressing)
		rx->data.qpl = gve_assign_rx_qpl(priv);
	for (i = 0; i < slots; i++) {
		struct page *page;
		dma_addr_t addr;

		if (rx->data.raw_addressing) {
			err = gve_alloc_page(priv, &priv->pdev->dev, &page,
					     &addr, DMA_FROM_DEVICE,
					     GFP_KERNEL);
			if (err) {
				int j;

				u64_stats_update_begin(&rx->statss);
				rx->rx_buf_alloc_fail++;
				u64_stats_update_end(&rx->statss);
				for (j = 0; j < i; j++)
					gve_free_page(&priv->pdev->dev, page,
						      addr, DMA_FROM_DEVICE);
				return err;
			}
		} else {
			page = rx->data.qpl->pages[i];
			addr = i * PAGE_SIZE;
		}
		gve_setup_rx_buffer(&rx->data.page_info[i],
				    &rx->data.data_ring[i], addr, page);
	}

	return slots;
}

static void gve_rx_add_to_block(struct gve_priv *priv, int queue_idx)
{
	u32 ntfy_idx = gve_rx_idx_to_ntfy(priv, queue_idx);
	struct gve_notify_block *block = &priv->ntfy_blocks[ntfy_idx];
	struct gve_rx_ring *rx = &priv->rx[queue_idx];

	block->rx = rx;
	rx->ntfy_id = ntfy_idx;
}

static int gve_rx_alloc_ring(struct gve_priv *priv, int idx)
{
	struct gve_rx_ring *rx = &priv->rx[idx];
	struct device *hdev = &priv->pdev->dev;
	u32 slots, npages;
	int filled_pages;
	size_t bytes;
	int err;

	netif_dbg(priv, drv, priv->dev, "allocating rx ring\n");
	/* Make sure everything is zeroed to start with */
	memset(rx, 0, sizeof(*rx));

	rx->gve = priv;
	rx->q_num = idx;

	slots = priv->rx_data_slot_cnt;
	rx->mask = slots - 1;
	rx->data.raw_addressing = priv->raw_addressing;

	/* alloc rx data ring */
	bytes = sizeof(*rx->data.data_ring) * slots;
	rx->data.data_ring = dma_alloc_coherent(hdev, bytes,
						&rx->data.data_bus,
						GFP_KERNEL);
	if (!rx->data.data_ring)
		return -ENOMEM;
	filled_pages = gve_prefill_rx_pages(rx);
	if (filled_pages < 0) {
		err = -ENOMEM;
		goto abort_with_slots;
	}
	rx->fill_cnt = filled_pages;
	/* Ensure data ring slots (packet buffers) are visible. */
	dma_wmb();

	/* Alloc gve_queue_resources */
	rx->q_resources =
		dma_alloc_coherent(hdev,
				   sizeof(*rx->q_resources),
				   &rx->q_resources_bus,
				   GFP_KERNEL);
	if (!rx->q_resources) {
		err = -ENOMEM;
		goto abort_filled;
	}
	netif_dbg(priv, drv, priv->dev, "rx[%d]->data.data_bus=%lx\n", idx,
		  (unsigned long)rx->data.data_bus);

	/* alloc rx desc ring */
	bytes = sizeof(struct gve_rx_desc) * priv->rx_desc_cnt;
	npages = bytes / PAGE_SIZE;
	if (npages * PAGE_SIZE != bytes) {
		err = -EIO;
		goto abort_with_q_resources;
	}

	rx->desc.desc_ring = dma_alloc_coherent(hdev, bytes, &rx->desc.bus,
						GFP_KERNEL);
	if (!rx->desc.desc_ring) {
		err = -ENOMEM;
		goto abort_with_q_resources;
	}
	rx->cnt = 0;
	rx->db_threshold = priv->rx_desc_cnt / 2;
	rx->desc.seqno = 1;
	gve_rx_add_to_block(priv, idx);

	return 0;

abort_with_q_resources:
	dma_free_coherent(hdev, sizeof(*rx->q_resources),
			  rx->q_resources, rx->q_resources_bus);
	rx->q_resources = NULL;
abort_filled:
	kfree(rx->data.page_info);
abort_with_slots:
	bytes = sizeof(*rx->data.data_ring) * slots;
	dma_free_coherent(hdev, bytes, rx->data.data_ring, rx->data.data_bus);
	rx->data.data_ring = NULL;

	return err;
}

int gve_rx_alloc_rings(struct gve_priv *priv)
{
	int err = 0;
	int i;

	for (i = 0; i < priv->rx_cfg.num_queues; i++) {
		err = gve_rx_alloc_ring(priv, i);
		if (err) {
			netif_err(priv, drv, priv->dev,
				  "Failed to alloc rx ring=%d: err=%d\n",
				  i, err);
			break;
		}
	}
	/* Unallocate if there was an error */
	if (err) {
		int j;

		for (j = 0; j < i; j++)
			gve_rx_free_ring(priv, j);
	}
	return err;
}

void gve_rx_free_rings(struct gve_priv *priv)
{
	int i;

	for (i = 0; i < priv->rx_cfg.num_queues; i++)
		gve_rx_free_ring(priv, i);
}

void gve_rx_write_doorbell(struct gve_priv *priv, struct gve_rx_ring *rx)
{
	u32 db_idx = be32_to_cpu(rx->q_resources->db_index);

	iowrite32be(rx->fill_cnt, &priv->db_bar2[db_idx]);
}

static enum pkt_hash_types gve_rss_type(__be16 pkt_flags)
{
	if (likely(pkt_flags & (GVE_RXF_TCP | GVE_RXF_UDP)))
		return PKT_HASH_TYPE_L4;
	if (pkt_flags & (GVE_RXF_IPV4 | GVE_RXF_IPV6))
		return PKT_HASH_TYPE_L3;
	return PKT_HASH_TYPE_L2;
}

static struct sk_buff *gve_rx_copy(struct net_device *dev,
				   struct napi_struct *napi,
				   struct gve_rx_slot_page_info *page_info,
				   u16 len)
{
	struct sk_buff *skb = napi_alloc_skb(napi, len);
	void *va = page_info->page_address + GVE_RX_PAD +
		   page_info->page_offset;

	if (unlikely(!skb))
		return NULL;

	__skb_put(skb, len);

	skb_copy_to_linear_data(skb, va, len);

	skb->protocol = eth_type_trans(skb, dev);

	return skb;
}

static struct sk_buff *gve_rx_add_frags(struct napi_struct *napi,
					struct gve_rx_slot_page_info *page_info,
					u16 len)
{
	struct sk_buff *skb = napi_get_frags(napi);

	if (unlikely(!skb))
		return NULL;

	skb_add_rx_frag(skb, 0, page_info->page,
			page_info->page_offset +
			GVE_RX_PAD, len, PAGE_SIZE / 2);

	return skb;
}

static int gve_rx_alloc_buffer(struct gve_priv *priv, struct device *dev,
			       struct gve_rx_slot_page_info *page_info,
			       struct gve_rx_data_slot *data_slot,
			       struct gve_rx_ring *rx)
{
	dma_addr_t new_dma;
	struct page *page;
	int err;

	err = gve_alloc_page(priv, dev, &page, &new_dma, DMA_FROM_DEVICE,
			     GFP_ATOMIC);
	if (err) {
		u64_stats_update_begin(&rx->statss);
		rx->rx_buf_alloc_fail++;
		u64_stats_update_end(&rx->statss);
		return err;
	}

	gve_setup_rx_buffer(page_info, data_slot, new_dma, page);
	return 0;
}

static void gve_rx_recycle_buffer(struct gve_rx_slot_page_info *page_info,
				  struct gve_rx_data_slot *data_slot)
{
	u64 addr = be64_to_cpu(data_slot->addr);

	/* "flip" to other packet buffer on this page */
	page_info->page_offset ^= PAGE_SIZE / 2;
	addr ^= PAGE_SIZE / 2;
	data_slot->addr = cpu_to_be64(addr);

	/* Make sure the kernel stack can't release the page */
	get_page(page_info->page);
}

static int gve_rx_can_recycle_buffer(struct net_device *netdev,
				      struct page *page) {
#if PAGE_SIZE == 4096
	/* We can't recycle the pages if we can't fit a packet
	 * into half a page.
	 */
	if (netdev->max_mtu + GVE_RX_PAD + ETH_HLEN  > PAGE_SIZE / 2) {
		return 0;
	} else {
		int pagecount = page_count(page);
		/* This page is not being used by any SKBs - reuse */
		if (pagecount == 1) {
			return 1;
		/* This page is still being used by an SKB - we can't reuse */
		} else if (pagecount >= 2) {
			return 0;
		} else {
			WARN(pagecount < 1, "Pagecount should never be < 1");
			return -1;
		}
	}
#else
	/* PAGE_SIZE != 4096 - don't try to reuse */
	return 0;
#endif
}

static struct sk_buff *
gve_rx_raw_addressing(struct device *dev, struct net_device *netdev,
		      struct gve_rx_slot_page_info *page_info, u16 len,
		      struct napi_struct *napi,
		      struct gve_rx_data_slot *data_slot, bool recycle)
{
	struct sk_buff *skb = gve_rx_add_frags(napi, page_info, len);

	if (!skb)
		return NULL;

	if (recycle) {
		gve_rx_recycle_buffer(page_info, data_slot);
	} else {
		dma_addr_t dma = (dma_addr_t)(be64_to_cpu(data_slot->addr) -
					      page_info->page_offset);

		/* Unmap the page and null our reference to it. The networking
		 * stack will free it.
		 */
		dma_unmap_page(dev, dma, PAGE_SIZE, DMA_FROM_DEVICE);
		page_info->page = NULL;
	}
	return skb;
}

static struct sk_buff *
gve_rx_qpl(struct device *dev, struct net_device *netdev,
	   struct gve_rx_ring *rx, struct gve_rx_slot_page_info *page_info,
	   u16 len, struct napi_struct *napi,
	   struct gve_rx_data_slot *data_slot, bool recycle)
{
	struct sk_buff *skb;
	/* if raw_addressing mode is not enabled gvnic can only receive into
	 * registered segments. If the buffer can't be recycled, our only
	 * choice is to copy the data out of it so that we can return it to the
	 * device.
	 */
	if (recycle) {
		skb = gve_rx_add_frags(napi, page_info, len);
		/* No point in recycling if we didn't get the skb */
		if (skb)
			gve_rx_recycle_buffer(page_info, data_slot);
	} else {
		skb = gve_rx_copy(netdev, napi, page_info, len);
		if (skb) {
			u64_stats_update_begin(&rx->statss);
			rx->rx_copied_pkt++;
			u64_stats_update_end(&rx->statss);
		}
	}
	return skb;
}

static bool gve_rx(struct gve_rx_ring *rx, struct gve_rx_desc *rx_desc,
		   netdev_features_t feat, u32 idx)
{
	struct gve_rx_slot_page_info *page_info;
	struct gve_priv *priv = rx->gve;
	struct napi_struct *napi = &priv->ntfy_blocks[rx->ntfy_id].napi;
	struct net_device *netdev = priv->dev;
	struct gve_rx_data_slot *data_slot;
	struct sk_buff *skb = NULL;
	dma_addr_t page_bus;
	int can_recycle;
	u16 len;

	/* drop this packet */
	if (unlikely(rx_desc->flags_seq & GVE_RXF_ERR)) {
		u64_stats_update_begin(&rx->statss);
		rx->rx_desc_err_dropped_pkt++;
		u64_stats_update_end(&rx->statss);
		return false;
	}

	len = be16_to_cpu(rx_desc->len) - GVE_RX_PAD;
	page_info = &rx->data.page_info[idx];
	data_slot = &rx->data.data_ring[idx];
	page_bus = (rx->data.raw_addressing) ?
                   be64_to_cpu(data_slot->addr) - page_info->page_offset:
		   rx->data.qpl->page_buses[idx];
	dma_sync_single_for_cpu(&priv->pdev->dev, page_bus,
				PAGE_SIZE, DMA_FROM_DEVICE);

	if (len <= priv->rx_copybreak) {
		/* Just copy small packets */
		skb = gve_rx_copy(netdev, napi, page_info, len);
		if (skb) {
			u64_stats_update_begin(&rx->statss);
			rx->rx_copied_pkt++;
			rx->rx_copybreak_pkt++;
			u64_stats_update_end(&rx->statss);
		}
	} else {
		can_recycle = gve_rx_can_recycle_buffer(netdev,
							page_info->page);
		if (can_recycle < 0) {
			gve_schedule_reset(priv);
			return false;
		} else if (rx->data.raw_addressing) {
			skb = gve_rx_raw_addressing(&priv->pdev->dev, netdev,
						    page_info, len, napi,
						    data_slot, can_recycle);
		} else {
			skb = gve_rx_qpl(&priv->pdev->dev, netdev, rx,
					 page_info, len, napi, data_slot,
					 can_recycle);
		}
	}

	if (!skb) {
		u64_stats_update_begin(&rx->statss);
		rx->rx_skb_alloc_fail++;
		u64_stats_update_end(&rx->statss);
		return false;
	}

	if (likely(feat & NETIF_F_RXCSUM)) {
		/* NIC passes up the partial sum */
		if (rx_desc->csum)
			skb->ip_summed = CHECKSUM_COMPLETE;
		else
			skb->ip_summed = CHECKSUM_NONE;
		skb->csum = csum_unfold(rx_desc->csum);
	}

	/* parse flags & pass relevant info up */
	if (likely(feat & NETIF_F_RXHASH) &&
	    gve_needs_rss(rx_desc->flags_seq))
		skb_set_hash(skb, be32_to_cpu(rx_desc->rss_hash),
			     gve_rss_type(rx_desc->flags_seq));

	if (skb_is_nonlinear(skb))
		napi_gro_frags(napi);
	else
		napi_gro_receive(napi, skb);

	return true;
}

static bool gve_rx_work_pending(struct gve_rx_ring *rx)
{
	struct gve_rx_desc *desc;
	__be16 flags_seq;
	u32 next_idx;

	next_idx = rx->cnt & rx->mask;
	desc = rx->desc.desc_ring + next_idx;

	/* make sure we have synchronized the seq no with the device */
	smp_mb();
	flags_seq = desc->flags_seq;

	return (GVE_SEQNO(flags_seq) == rx->desc.seqno);
}

static void gve_rx_refill_buffers(struct gve_priv *priv, struct gve_rx_ring *rx)
{
	u32 fill_cnt = rx->fill_cnt;

	while ((fill_cnt & rx->mask) != (rx->cnt & rx->mask)) {
		u32 idx = fill_cnt & rx->mask;

		/* If page is not null, that slot is already filled */
		if (!rx->data.page_info[idx].page) {
			if (gve_rx_alloc_buffer(priv, &priv->pdev->dev,
						&rx->data.page_info[idx],
						&rx->data.data_ring[idx], rx)) {
				break;
			}
		}
		fill_cnt++;
	}
	rx->fill_cnt = fill_cnt;
}

bool gve_clean_rx_done(struct gve_rx_ring *rx, int budget,
		       netdev_features_t feat)
{
	struct gve_priv *priv = rx->gve;
	u32 work_done = 0, packets = 0;
	struct gve_rx_desc *desc;
	u32 cnt = rx->cnt;
	u32 idx = cnt & rx->mask;
	u64 bytes = 0;

	desc = rx->desc.desc_ring + idx;
	while ((GVE_SEQNO(desc->flags_seq) == rx->desc.seqno) &&
	       work_done < budget) {
		bool dropped;
		netif_info(priv, rx_status, priv->dev,
			   "[%d] idx=%d desc=%p desc->flags_seq=0x%x\n",
			   rx->q_num, idx, desc, desc->flags_seq);
		netif_info(priv, rx_status, priv->dev,
			   "[%d] seqno=%d rx->desc.seqno=%d\n",
			   rx->q_num, GVE_SEQNO(desc->flags_seq),
			   rx->desc.seqno);
		dropped = !gve_rx(rx, desc, feat, idx);
		if (!dropped) {
			bytes += be16_to_cpu(desc->len) - GVE_RX_PAD;
			packets++;
		}
		cnt++;
		idx = cnt & rx->mask;
		desc = rx->desc.desc_ring + idx;
		rx->desc.seqno = gve_next_seqno(rx->desc.seqno);
		work_done++;
	}

	if (!work_done)
		return false;

	u64_stats_update_begin(&rx->statss);
	rx->rpackets += packets;
	rx->rbytes += bytes;
	u64_stats_update_end(&rx->statss);
	rx->cnt = cnt;
	/* restock ring slots */
	if (!rx->data.raw_addressing) {
		/* In QPL mode buffs are refilled as the desc are processed */
		rx->fill_cnt += work_done;
		dma_wmb();/* Ensure descs are visible before ringing doorbell */
		gve_rx_write_doorbell(priv, rx);
	} else if (rx->fill_cnt - cnt <= rx->db_threshold) {
		/* In raw addressing mode buffs are only refilled if the avail
		 * falls below a threshold.
		 */
		gve_rx_refill_buffers(priv, rx);
		/* restock desc ring slots */
		dma_wmb();/* Ensure descs are visible before ringing doorbell */
		gve_rx_write_doorbell(priv, rx);
	}

	return gve_rx_work_pending(rx);
}

bool gve_rx_poll(struct gve_notify_block *block, int budget)
{
	struct gve_rx_ring *rx = block->rx;
	netdev_features_t feat;
	bool repoll = false;

	feat = block->napi.dev->features;

	/* If budget is 0, do all the work */
	if (budget == 0)
		budget = INT_MAX;

	if (budget > 0)
		repoll |= gve_clean_rx_done(rx, budget, feat);
	else
		repoll |= gve_rx_work_pending(rx);
	return repoll;
}
