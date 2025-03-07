@@
@@
struct gve_rx_slot_page_info {
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0))
	union {
		struct page *page;
		netmem_ref netmem;
	};
+#else
+	struct page *page;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0)) */
	void *page_address;
	...
};

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0))
static void gve_skb_add_rx_frag(...)
{
	...
}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0)) */

@@
@@
static int gve_rx_append_frags(...)
{
	...
	if (gve_rx_should_trigger_copy_ondemand(rx))
		return gve_rx_copy_ondemand(rx, buf_state, buf_len);

+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0))
	gve_skb_add_rx_frag(rx, buf_state, num_frags, buf_len);
+#else
+	skb_add_rx_frag(rx->ctx.skb_tail, num_frags,
+			buf_state->page_info.page,
+			buf_state->page_info.page_offset,
+			buf_len, buf_state->page_info.buf_size);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0)) */
	...
}

@@
@@
static int gve_rx_dqo(...)
{
	...
	/* Page might have not been used for awhile and was likely last written
	 * by a different thread.
	 */

+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0))
	if (rx->dqo.page_pool) {
		if (!netmem_is_net_iov(buf_state->page_info.netmem))
			prefetch(netmem_to_page(buf_state->page_info.netmem));
	} else {
		prefetch(buf_state->page_info.page);
	}
+#else
+	prefetch(buf_state->page_info.page);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0) */
	...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0))
	gve_skb_add_rx_frag(rx, buf_state, 0, buf_len);
+#else
+	skb_add_rx_frag(rx->ctx.skb_head, 0, buf_state->page_info.page,
+			buf_state->page_info.page_offset, buf_len,
+			buf_state->page_info.buf_size);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0) */
	gve_reuse_buffer(rx, buf_state);
	...
}

@@
@@
void gve_reuse_buffer(...)
{
	if (rx->dqo.page_pool) {
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0))
		buf_state->page_info.netmem = 0;
+#else
+		buf_state->page_info.page = NULL;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0) */
		gve_free_buf_state(rx, buf_state);
	} else {
		gve_dec_pagecnt_bias(&buf_state->page_info);
		gve_try_recycle_buf(rx->gve, rx, buf_state);
	}
}

@@
@@
static int gve_alloc_from_page_pool(...)
{
	struct gve_priv *priv = rx->gve;
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0))
	netmem_ref netmem;
+#else
+	struct page *page;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0) */

	buf_state->page_info.buf_size = priv->data_buffer_size_dqo;
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0))
	netmem = page_pool_alloc_netmem(rx->dqo.page_pool,
					&buf_state->page_info.page_offset,
					&buf_state->page_info.buf_size,
					GFP_ATOMIC);

	if (!netmem)
		return -ENOMEM;

	buf_state->page_info.netmem = netmem;
	buf_state->page_info.page_address = netmem_address(netmem);
	buf_state->addr = page_pool_get_dma_addr_netmem(netmem);
+#else
+	page = page_pool_alloc(rx->dqo.page_pool,
+			       &buf_state->page_info.page_offset,
+			       &buf_state->page_info.buf_size, GFP_ATOMIC);
+
+	if (!page)
+		return -ENOMEM;
+
+	buf_state->page_info.page = page;
+	buf_state->page_info.page_address = page_address(page);
+	buf_state->addr = page_pool_get_dma_addr(page);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0) */

	return 0;
}

@@
@@
void gve_free_to_page_pool(...)
{
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0))
	netmem_ref netmem = buf_state->page_info.netmem;

	if (!netmem)
		return;

	page_pool_put_full_netmem(...);
	buf_state->page_info.netmem = 0;
+#else
+	struct page *page = buf_state->page_info.page;
+
+	if (!page)
+		return;
+
+	page_pool_put_full_page(page->pp, page, allow_direct);
+	buf_state->page_info.page = NULL;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0) */
}
