// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/* Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2015-2018 Google, Inc.
 */

#include "gve.h"
#include "gve_adminq.h"
#include <linux/etherdevice.h>

void gve_rx_remove_from_block(struct gve_priv *priv, int queue_idx)
{
	struct gve_notify_block *block =
			&priv->ntfy_blocks[gve_rx_ntfy_idx(priv, queue_idx)];

	block->rx = NULL;

	/* If there are no more rings in this block disable napi */
	if (!block->tx)
		gve_remove_napi(block);
}

static void gve_rx_free_ring(struct gve_priv *priv, int idx)
{
	struct gve_rx_ring *rx = &priv->rx[idx];
	struct device *hdev = &priv->pdev->dev;
	size_t bytes;
	int slots;

	gve_rx_remove_from_block(priv, idx);

	bytes = sizeof(struct gve_rx_desc) * priv->rx_desc_cnt;
	dma_free_coherent(hdev, bytes, rx->desc.desc_ring, rx->desc.bus);
	rx->desc.desc_ring = NULL;

	dma_free_coherent(hdev, sizeof(*rx->q_resources),
			  rx->q_resources, rx->q_resources_bus);
	rx->q_resources = NULL;

	gve_unassign_qpl(priv, rx->data.qpl->id);
	rx->data.qpl = NULL;
	kfree(rx->data.page_info);

	slots = rx->data.mask + 1;
	bytes = sizeof(*rx->data.data_ring) * slots;
	dma_free_coherent(hdev, bytes, rx->data.data_ring,
			  rx->data.data_bus);
	rx->data.data_ring = NULL;
	netif_dbg(priv, drv, priv->dev, "freed rx ring %d\n", idx);
}

static int gve_prefill_rx_pages(struct gve_rx_ring *rx)
{
	struct gve_rx_slot_page_info *page_info;
	struct gve_priv *priv = rx->gve;
	int slots, size;
	int i;

	/* Allocate one page per Rx queue slot. Each page is split into two
	 * packet buffers, when possible we "page flip" between the two.
	 */
	slots = rx->data.mask + 1;
	size = slots * PAGE_SIZE;

	rx->data.page_info = kcalloc(slots,
				     sizeof(*rx->data.page_info),
				     GFP_KERNEL);
	if (!rx->data.page_info)
		return -ENOMEM;

	rx->data.qpl = gve_assign_rx_qpl(priv);

	for (i = 0; i < slots; i++) {
		page_info = &rx->data.page_info[i];
		page_info->page = rx->data.qpl->pages[i];
		page_info->page_offset = 0;
		page_info->page_address = page_address(rx->data.qpl->pages[i]);
		rx->data.data_ring[i].qpl_offset = cpu_to_be64(i * PAGE_SIZE);
	}

	return slots;
}

static void gve_rx_add_to_block(struct gve_priv *priv, int queue_idx)
{
	u32 ntfy_idx = gve_rx_ntfy_idx(priv, queue_idx);
	struct gve_notify_block *block = &priv->ntfy_blocks[ntfy_idx];
	struct gve_rx_ring *rx = &priv->rx[queue_idx];

	block->rx = rx;
	rx->ntfy_id = ntfy_idx;

	if (!block->napi_enabled)
		gve_add_napi(priv, block);
}

static int gve_rx_alloc_ring(struct gve_priv *priv, int idx)
{
	struct gve_rx_ring *rx = &priv->rx[idx];
	struct device *hdev = &priv->pdev->dev;
	int slots, npages, gve_desc_per_page;
	size_t bytes;
	int err;

	netif_dbg(priv, drv, priv->dev, "allocating rx ring\n");
	/* Make sure everything is zeroed to start with */
	memset(rx, 0, sizeof(*rx));

	rx->gve = priv;
	rx->q_num = idx;

	slots = priv->rx_pages_per_qpl;
	rx->data.mask = slots - 1;

	/* alloc rx data ring */
	bytes = sizeof(*rx->data.data_ring) * slots;
	rx->data.data_ring = dma_zalloc_coherent(hdev, bytes,
						 &rx->data.data_bus,
						 GFP_KERNEL);
	if (!rx->data.data_ring)
		return -ENOMEM;
	rx->desc.fill_cnt = gve_prefill_rx_pages(rx);
	if (rx->desc.fill_cnt < 0) {
		rx->desc.fill_cnt = 0;
		err = -ENOMEM;
		goto abort_with_slots;
	}
	/* Ensure data ring slots (packet buffers) are visible. */
	dma_wmb();

	/* Alloc gve_queue_resources */
	rx->q_resources =
		dma_zalloc_coherent(hdev,
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
	gve_desc_per_page = PAGE_SIZE / sizeof(struct gve_rx_desc);
	bytes = sizeof(struct gve_rx_desc) * priv->rx_desc_cnt;
	npages = bytes / PAGE_SIZE;
	if (npages * PAGE_SIZE != bytes) {
		netif_err(priv, drv, priv->dev,
			  "rx[%d]->desc.desc_ring size must be a multiple of PAGE_SIZE. Actual size: %lu\n",
			  idx, bytes);
		err = -EIO;
		goto abort_with_q_resources;
	}

	rx->desc.desc_ring = dma_zalloc_coherent(hdev, bytes, &rx->desc.bus,
						 GFP_KERNEL);
	if (!rx->desc.desc_ring) {
		netif_err(priv, drv, priv->dev,
			  "alloc failed for rx[%d]->desc.desc_ring\n", idx);
		err = -ENOMEM;
		goto abort_with_q_resources;
	}
	rx->desc.mask = slots - 1;
	rx->desc.cnt = 0;
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
			netdev_err(priv->dev, "alloc failed for rx ring=%d\n",
				   i);
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

	writel(cpu_to_be32(rx->desc.fill_cnt), &priv->db_bar2[db_idx]);
}

static enum pkt_hash_types gve_rss_type(__be16 pkt_flags)
{
	if (likely(pkt_flags & (GVE_RXF_TCP | GVE_RXF_UDP)))
		return PKT_HASH_TYPE_L4;
	if (pkt_flags & (GVE_RXF_IPV4 | GVE_RXF_IPV6))
		return PKT_HASH_TYPE_L3;
	return PKT_HASH_TYPE_L2;
}

static int gve_rx(struct gve_rx_ring *rx, struct gve_rx_desc *rx_desc,
		  netdev_features_t feat)
{
	struct gve_rx_slot_page_info *page_info;
	struct gve_priv *priv = rx->gve;
	struct napi_struct *napi = &priv->ntfy_blocks[rx->ntfy_id].napi;
	struct net_device *dev = priv->dev;
	bool can_page_flip = true;
	struct sk_buff *skb;
	int pagecount;
	u64 qpl_offset;
	void *va;
	u16 len;
	int idx;

	len = be16_to_cpu(rx_desc->len) - GVE_RX_PAD;

	idx = rx->data.cnt & rx->data.mask;
	page_info = &rx->data.page_info[idx];

	netif_info(priv, rx_status, priv->dev,
		   "[%d] %s: len=0x%x flags=0x%x data.cnt=%d\n",
		   rx->q_num, __func__, len, rx_desc->flags_seq, rx->data.cnt);

#if PAGE_SIZE == 4096
	/* just copy small packets. */
	if (len <= priv->rx_copybreak)
		goto copy;

	pagecount = page_count(page_info->page);
	if (pagecount == 1) {
		/* No part of this page is used by any SKBs; we attach the page
		 * fragment to a new SKB and pass it up the stack.
		 */
		skb = napi_get_frags(napi);
		if (unlikely(!skb)) {
			can_page_flip = false;
			goto copy;
		}
		get_page(page_info->page);

		skb_add_rx_frag(skb, 0, page_info->page,
				page_info->page_offset + GVE_RX_PAD, len,
				PAGE_SIZE / 2);

		/* "flip" to other packet buffer on this page */
		qpl_offset = be64_to_cpu(rx->data.data_ring[idx].qpl_offset);
		page_info->page_offset ^= PAGE_SIZE / 2;
		qpl_offset ^= PAGE_SIZE / 2;
		rx->data.data_ring[idx].qpl_offset = cpu_to_be64(qpl_offset);
	} else if (pagecount >= 2) {
		/* We have previously passed the other half of this page up the
		 * stack, but it has not yet been freed.
		 *
		 * NOTYET NOTYET NOTYET
		 * 0) Scan rx::recycling list for a page w/ refcount==1.
		 *    If found:
		 * 0.1) get_page(page_info->page)
		 * 0.2) Link page to rx::recycle list
		 * 0.3) Unload page from page_info (clear ->page, page_address)
		 * 0.4) Load page w/ refcount==1 from rx::recycling into
		 *	page_info
		 *    Otherwise:
		 * 1.0) Fallback to copying.
		 *
		 * Optimizations:
		 * - Second list of pages to reuse quickly; don't just scan for
		 *   first pc == 1 page, scan more. (or scan on rx::recycling
		 *   insert). Use reuse list first, before recycling).
		 *   Remember: Reduce, Reuse, Recycle!
		 * NOTYET NOTYET NOTYET
		 */

		/* Just fallback to copying for now */
		can_page_flip = false;
	} else {
		WARN(pagecount < 1, "Pagecount should never be < 1");
		return 0;
	}
#else
	can_page_flip = false;
#endif

copy:
	if (len <= priv->rx_copybreak || !can_page_flip) {
		skb = napi_alloc_skb(napi, len);
		if (unlikely(!skb))
			return 0;

		va = page_info->page_address + page_info->page_offset +
		     GVE_RX_PAD;

		__skb_put(skb, len);

		skb_copy_to_linear_data(skb, va, len);

		skb->protocol = eth_type_trans(skb, dev);
	}

	rx->data.cnt++;

	if (likely(feat & NETIF_F_RXCSUM)) {
		/* NIC passes up the partial sum */
		if (rx_desc->csum)
			skb->ip_summed = CHECKSUM_COMPLETE;
		else
			skb->ip_summed = CHECKSUM_NONE;
		skb->csum = rx_desc->csum;
	}

	/* parse flags & pass relevant info up */
	if (likely(feat & NETIF_F_RXHASH) &&
	    gve_rss_valid(rx_desc->flags_seq))
		skb_set_hash(skb, be32_to_cpu(rx_desc->rss_hash),
			     gve_rss_type(rx_desc->flags_seq));

	if (skb_is_nonlinear(skb))
		napi_gro_frags(napi);
	else
		napi_gro_receive(napi, skb);
	return 1;
}

static bool gve_rx_work_pending(struct gve_rx_ring *rx)
{
	struct gve_rx_desc *desc;
	int next_idx;
	u16 flags_seq;

	next_idx = rx->desc.cnt & rx->desc.mask;
	desc = rx->desc.desc_ring + next_idx;

	flags_seq = desc->flags_seq;
	/* Make sure we have synchronized the seq no with the device */
	smp_rmb();

	return (GVE_SEQNO(flags_seq) == rx->desc.seqno);
}

bool gve_clean_rx_done(struct gve_rx_ring *rx, int budget,
		       netdev_features_t feat)
{
	struct gve_priv *priv = rx->gve;
	struct gve_rx_desc *desc;
	int cnt = rx->desc.cnt;
	int idx = cnt & rx->desc.mask;
	int work_done = 0;
	u64 bytes = 0;

	desc = rx->desc.desc_ring + idx;
	while ((GVE_SEQNO(desc->flags_seq) == rx->desc.seqno) &&
	       work_done < budget) {
		netif_info(priv, rx_status, priv->dev,
			   "[%d] idx=%d desc=%p desc->flags_seq=0x%x\n",
			   rx->q_num, idx, desc, desc->flags_seq);
		netif_info(priv, rx_status, priv->dev,
			   "[%d] seqno=%d rx->desc.seqno=%d\n",
			   rx->q_num, GVE_SEQNO(desc->flags_seq),
			   rx->desc.seqno);
		bytes += be16_to_cpu(desc->len) - GVE_RX_PAD;
		gve_rx(rx, desc, feat);
		cnt++;
		idx = cnt & rx->desc.mask;
		desc = rx->desc.desc_ring + idx;
		rx->desc.seqno = gve_next_seqno(rx->desc.seqno);
		work_done++;
	}

	if (!work_done)
		return false;

	rx->rpackets += work_done;
	rx->rbytes += bytes;
	rx->desc.cnt = cnt;
	rx->desc.fill_cnt += work_done;

	/* restock desc ring slots */
	dma_wmb();	/* Ensure descs are visible before ringing doorbell */
	gve_rx_write_doorbell(priv, rx);
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
