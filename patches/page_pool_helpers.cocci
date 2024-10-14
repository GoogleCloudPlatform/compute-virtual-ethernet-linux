@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
#include <net/page_pool/helpers.h>
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
@@
struct gve_rx_ring {
	struct gve_priv *gve;
	union {
		/* GQI fields */
		struct {
			struct gve_rx_desc_queue desc;
			struct gve_rx_data_queue data;

			/* threshold for posting new buffs and descs */
			u32 db_threshold;
			u16 packet_buffer_size;

			u32 qpl_copy_pool_mask;
			u32 qpl_copy_pool_head;
			struct gve_rx_slot_page_info *qpl_copy_pool;
		};

		/* DQO fields. */
		struct {
			struct gve_rx_buf_queue_dqo bufq;
			struct gve_rx_compl_queue_dqo complq;

			struct gve_rx_buf_state_dqo *buf_states;
			u16 num_buf_states;

			/* Linked list of gve_rx_buf_state_dqo. Index into
			 * buf_states, or -1 if empty.
			 */
			s16 free_buf_states;

			/* Linked list of gve_rx_buf_state_dqo. Indexes into
			 * buf_states, or -1 if empty.
			 *
			 * This list contains buf_states which are pointing to
			 * valid buffers.
			 *
			 * We use a FIFO here in order to increase the
			 * probability that buffers can be reused by increasing
			 * the time between usages.
			 */
			struct gve_index_list recycled_buf_states;

			/* Linked list of gve_rx_buf_state_dqo. Indexes into
			 * buf_states, or -1 if empty.
			 *
			 * This list contains buf_states which have buffers
			 * which cannot be reused yet.
			 */
			struct gve_index_list used_buf_states;

			/* qpl assigned to this queue */
			struct gve_queue_page_list *qpl;

			/* index into queue page list */
			u32 next_qpl_page_idx;

			/* track number of used buffers */
			u16 used_buf_states_cnt;

			/* Address info of the buffers for header-split */
			struct gve_header_buf hdr_bufs;
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))

			struct page_pool *page_pool;
+#endif
		} dqo;
	};
...
};

@@
identifier gve_rx_slot_page_info;
@@
struct gve_rx_slot_page_info {
	...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
	unsigned int buf_size;
+#endif
	...
};

@@
@@
struct gve_rx_buf_state_dqo *gve_get_recycled_buf_state(...)
{
...
	for (i = 0; i < 5; i++) {
		buf_state = gve_dequeue_buf_state(rx, &rx->dqo.used_buf_states);
		if (gve_buf_ref_cnt(buf_state) == 0) {
			rx->dqo.used_buf_states_cnt--;
			return buf_state;
		}

		gve_enqueue_buf_state(rx, &rx->dqo.used_buf_states, buf_state);
	}

+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
	return NULL;

+#else /* (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0)) */

	/* For QPL, we cannot allocate any new buffers and must
	 * wait for the existing ones to be available.
	 */
+	if (rx->dqo.qpl)
+		return NULL;
+	/* If there are no free buf states discard an entry from
+	 * `used_buf_states` so it can be used.
+	 */
+	if (unlikely(rx->dqo.free_buf_states == -1)) {
+		buf_state = gve_dequeue_buf_state(rx, &rx->dqo.used_buf_states);
+		if (gve_buf_ref_cnt(buf_state) == 0)
+			return buf_state;
+
+		gve_free_page_dqo(rx->gve, buf_state, true);
+		gve_free_buf_state(rx, buf_state);
+	}
+
+	return NULL;
+#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0)) */
}

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
void gve_free_qpl_page_dqo(...)
{
...
}
+#endif

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
int gve_alloc_qpl_page_dqo(...)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
void gve_reuse_buffer(...)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
void gve_free_buffer(...)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
int gve_alloc_buffer(...)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
identifier gve_try_recycle_buf;
@@
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0))
+int gve_alloc_page_dqo(struct gve_rx_ring *rx, struct gve_rx_buf_state_dqo *buf_state);
+#endif
void gve_try_recycle_buf(struct gve_priv *priv, struct gve_rx_ring *rx,
			 struct gve_rx_buf_state_dqo *buf_state);

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
struct page_pool *gve_rx_create_page_pool(...)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
identifier pp;
@@
struct page_pool *gve_rx_create_page_pool(...)
{
	struct page_pool_params pp = {
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0))
		.netdev = priv->dev,
+#endif
	};
...
}


@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
void gve_free_to_page_pool(...)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
static int gve_alloc_from_page_pool(...)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@ headers @
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
int gve_alloc_qpl_page_dqo(...);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
void gve_free_to_page_pool(struct gve_rx_ring *rx, struct gve_rx_buf_state_dqo *buf_state,
			   bool allow_direct);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
void gve_free_qpl_page_dqo(struct gve_rx_buf_state_dqo *buf_state);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
void gve_free_buffer(...);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
void gve_reuse_buffer(...);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
int gve_alloc_buffer(...);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
struct page_pool *gve_rx_create_page_pool(...);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
@@
static void gve_rx_reset_ring_dqo(struct gve_priv *priv, int idx)
{
	...
	/* Reset buf states */
	if (rx->dqo.buf_states) {
		for (i = 0; i < rx->dqo.num_buf_states; i++) {
			struct gve_rx_buf_state_dqo *bs = &rx->dqo.buf_states[i];

+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
			if (rx->dqo.page_pool)
				gve_free_to_page_pool(rx, bs, false);
			else
				gve_free_qpl_page_dqo(bs);
+#else
+			if (bs->page_info.page)
+				gve_free_page_dqo(priv, bs, !rx->dqo.qpl);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */
		}
	}
	...
}

@@
identifier rx;
@@
void gve_rx_free_ring_dqo(struct gve_priv *priv, struct gve_rx_ring *rx,
			  struct gve_rx_alloc_rings_cfg *cfg)
{
...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
	if (rx->dqo.page_pool) {
		page_pool_destroy(rx->dqo.page_pool);
		rx->dqo.page_pool = NULL;
	}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */
...
}

@@
@@
int gve_rx_alloc_ring_dqo(...)
{
...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
	struct page_pool *pool;
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */
...
}

@@
@@
void gve_rx_post_buffers_dqo(struct gve_rx_ring *rx)
{
	struct gve_rx_compl_queue_dqo *complq = &rx->dqo.complq;
	struct gve_rx_buf_queue_dqo *bufq = &rx->dqo.bufq;
	struct gve_priv *priv = rx->gve;
	u32 num_avail_slots;
	u32 num_full_slots;
	u32 num_posted = 0;

	num_full_slots = (bufq->tail - bufq->head) & bufq->mask;
	num_avail_slots = bufq->mask - num_full_slots;

	num_avail_slots = min_t(u32, num_avail_slots, complq->num_free_slots);
	while (num_posted < num_avail_slots) {
		struct gve_rx_desc_dqo *desc = &bufq->desc_ring[bufq->tail];
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))

		if (unlikely(gve_alloc_buffer(rx, desc))) {
			u64_stats_update_begin(&rx->statss);
			rx->rx_buf_alloc_fail++;
			u64_stats_update_end(&rx->statss);
			break;
		}
+#else
+		struct gve_rx_buf_state_dqo *buf_state;
+
+		buf_state = gve_get_recycled_buf_state(rx);
+		if (unlikely(!buf_state)) {
+			buf_state = gve_alloc_buf_state(rx);
+			if (unlikely(!buf_state))
+				break;
+
+			if (unlikely(gve_alloc_page_dqo(rx, buf_state))) {
+				u64_stats_update_begin(&rx->statss);
+				rx->rx_buf_alloc_fail++;
+				u64_stats_update_end(&rx->statss);
+				gve_free_buf_state(rx, buf_state);
+				break;
+			}
+		}
+
+		desc->buf_id = cpu_to_le16(buf_state - rx->dqo.buf_states);
+		desc->buf_addr = cpu_to_le64(buf_state->addr +
+					     buf_state->page_info.page_offset);
+
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */
		...
	}
...
}

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
	if (rx->dqo.page_pool)
		skb_mark_for_recycle(rx->ctx.skb_head);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
	if (rx->dqo.page_pool)
		skb_mark_for_recycle(rx->ctx.skb_tail);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
expression list args;
@@
static int gve_rx_append_frags(...)
{
	...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
	skb_add_rx_frag(args, buf_state->page_info.buf_size);
+#else
+	skb_add_rx_frag(args, priv->data_buffer_size_dqo);
+#endif
	...
}
@@
expression list args;
@@
static int gve_rx_dqo(...)
{
	...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
	skb_add_rx_frag(args, buf_state->page_info.buf_size);
+#else
+	skb_add_rx_frag(args, priv->data_buffer_size_dqo);
+#endif
	...
}

@@
@@
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0))
+	gve_dec_pagecnt_bias(&buf_state->page_info);
+	gve_try_recycle_buf(priv, rx, buf_state);
+#else
	gve_reuse_buffer(rx, buf_state);
+#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0)) */

@@
identifier rx;
@@
void gve_rx_free_ring_dqo(...)
{
	...
	for (i = 0; i < rx->dqo.num_buf_states; i++) {
		struct gve_rx_buf_state_dqo *bs = &rx->dqo.buf_states[i];

+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
		if (rx->dqo.page_pool)
			gve_free_to_page_pool(rx, bs, false);
		else
			gve_free_qpl_page_dqo(bs);
+#else
+		if (bs->page_info.page)
+			gve_free_page_dqo(priv, bs, !rx->dqo.qpl);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */
	}
	...
}

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
	gve_free_buffer(rx, buf_state);
+#else
+	gve_enqueue_buf_state(rx, &rx->dqo.recycled_buf_states, buf_state);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
		if (rx->dqo.page_pool)
			skb_mark_for_recycle(skb);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0))
		rx->ctx.skb_head->truesize += buf_state->page_info.buf_size;
+#else
+		rx->ctx.skb_head->truesize += priv->data_buffer_size_dqo;
+#endif

@@
identifier gve_rx_append_frags;
@@
static int gve_rx_append_frags(...)
{
...
	skb_add_rx_frag(rx->ctx.skb_tail, num_frags,
			buf_state->page_info.page,
			buf_state->page_info.page_offset,
			buf_len, buf_state->page_info.buf_size);
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0))
+       gve_dec_pagecnt_bias(&buf_state->page_info);
+
+       /* Advances buffer page-offset if page is partially used.
+        * Marks buffer as used if page is full.
+        */
+       gve_try_recycle_buf(priv, rx, buf_state);
+#else
	gve_reuse_buffer(rx, buf_state);
+#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0)) */
	return 0;
}

@@
@@
static int gve_rx_dqo(...)
{
...
	if (unlikely(compl_desc->rx_error)) {
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0))
+		gve_enqueue_buf_state(rx, &rx->dqo.recycled_buf_states,
+				      buf_state);
+#else
		gve_free_buffer(rx, buf_state);
+#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0)) */
		return -EINVAL;
	}
...
}

@@
@@
static int gve_rx_dqo(...)
{
...
	if (eop && buf_len <= priv->rx_copybreak) {
		rx->ctx.skb_head = gve_rx_copy(priv->dev, napi,
					       &buf_state->page_info, buf_len);
		if (unlikely(!rx->ctx.skb_head))
			goto error;
		rx->ctx.skb_tail = rx->ctx.skb_head;

		u64_stats_update_begin(&rx->statss);
		rx->rx_copied_pkt++;
		rx->rx_copybreak_pkt++;
		u64_stats_update_end(&rx->statss);

+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0))
+		gve_enqueue_buf_state(rx, &rx->dqo.recycled_buf_states,
+				      buf_state);
+#else
		gve_free_buffer(rx, buf_state);
+#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0)) */
		return 0;
	}
...
}

@@
@@
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0))
+	rx->dqo.num_buf_states = cfg->raw_addressing ?
+		min_t(s16, S16_MAX, buffer_queue_slots * 4) :
+		gve_get_rx_pages_per_qpl_dqo(cfg->ring_size);
+#else
	rx->dqo.num_buf_states = cfg->raw_addressing ? buffer_queue_slots :
		gve_get_rx_pages_per_qpl_dqo(cfg->ring_size);
+#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0)) */

@@
identifier gve_rx_alloc_ring_dqo;
@@
int gve_rx_alloc_ring_dqo(...)
{
...
	/* Allocate RX buffer queue */
	size = sizeof(rx->dqo.bufq.desc_ring[0]) * buffer_queue_slots;
	rx->dqo.bufq.desc_ring =
		dma_alloc_coherent(hdev, size, &rx->dqo.bufq.bus, GFP_KERNEL);
	if (!rx->dqo.bufq.desc_ring)
		goto err;

+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0))
+       if (!cfg->raw_addressing) {
+		qpl_id = gve_get_rx_qpl_id(cfg->qcfg_tx, rx->q_num);
+		qpl_page_cnt = gve_get_rx_pages_per_qpl_dqo(cfg->ring_size);
+
+		rx->dqo.qpl = gve_alloc_queue_page_list(priv, qpl_id,
+							qpl_page_cnt);
+		if (!rx->dqo.qpl)
+			goto err;
+		rx->dqo.next_qpl_page_idx = 0;
+	}
+
+#else
	if (cfg->raw_addressing) {
		pool = gve_rx_create_page_pool(priv, rx);
		if (IS_ERR(pool))
			goto err;

		rx->dqo.page_pool = pool;
	} else {
		qpl_id = gve_get_rx_qpl_id(cfg->qcfg_tx, rx->q_num);
		qpl_page_cnt = gve_get_rx_pages_per_qpl_dqo(cfg->ring_size);

		rx->dqo.qpl = gve_alloc_queue_page_list(priv, qpl_id,
							qpl_page_cnt);
		if (!rx->dqo.qpl)
			goto err;
		rx->dqo.next_qpl_page_idx = 0;
	}

+#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0)) */
	rx->q_resources = dma_alloc_coherent(hdev, sizeof(*rx->q_resources),
					     &rx->q_resources_bus, GFP_KERNEL);
	if (!rx->q_resources)
		goto err;
...
}

@@
@@
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0))
+void gve_free_page_dqo(struct gve_priv *priv, struct gve_rx_buf_state_dqo *bs, bool free_page)
+{
+	page_ref_sub(bs->page_info.page, bs->page_info.pagecnt_bias - 1);
+	if (free_page)
+		gve_free_page(&priv->pdev->dev, bs->page_info.page, bs->addr,
+			      DMA_FROM_DEVICE);
+	bs->page_info.page = NULL;
+}
+
+#endif
struct gve_rx_buf_state_dqo *gve_alloc_buf_state(struct gve_rx_ring *rx)
{
	...
}

@@
identifier gve_try_recycle_buf;
@@
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0))
+int gve_alloc_page_dqo(struct gve_rx_ring *rx, struct gve_rx_buf_state_dqo *buf_state)
+{
+	struct gve_priv *priv = rx->gve;
+	u32 idx;
+
+	if (!rx->dqo.qpl) {
+		int err;
+
+		err = gve_alloc_page(priv, &priv->pdev->dev,
+				     &buf_state->page_info.page,
+				     &buf_state->addr,
+				     DMA_FROM_DEVICE, GFP_ATOMIC);
+		if (err)
+			return err;
+	} else {
+		idx = rx->dqo.next_qpl_page_idx;
+		if (idx >= gve_get_rx_pages_per_qpl_dqo(priv->rx_desc_cnt)) {
+			net_err_ratelimited("%s: Out of QPL pages\n",
+					    priv->dev->name);
+			return -ENOMEM;
+		}
+		buf_state->page_info.page = rx->dqo.qpl->pages[idx];
+		buf_state->addr = rx->dqo.qpl->page_buses[idx];
+		rx->dqo.next_qpl_page_idx++;
+	}
+	buf_state->page_info.page_offset = 0;
+	buf_state->page_info.page_address =
+		page_address(buf_state->page_info.page);
+	buf_state->last_single_ref_offset = 0;
+
+	/* The page already has 1 ref. */
+	page_ref_add(buf_state->page_info.page, INT_MAX - 1);
+	buf_state->page_info.pagecnt_bias = INT_MAX;
+
+	return 0;
+}
+
+#endif
void gve_try_recycle_buf(struct gve_priv *priv, struct gve_rx_ring *rx,
			 struct gve_rx_buf_state_dqo *buf_state)
{
	...
}
