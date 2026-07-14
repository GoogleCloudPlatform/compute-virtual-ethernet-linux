@@
@@
struct gve_rx_slot_page_info {
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0))
	union {
		struct page *page;
		netmem_ref netmem;
	};
+#else
+	struct page *page;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0)) */
	void *page_address;
	...
};

@@
expression pp;
@@
void gve_skb_add_rx_frag(...)
{
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0))
if (pp) {
...
} else {
+#endif
...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0))
}
+#endif


}

@@
@@
static int gve_rx_dqo(...)
{
	...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0))
	if (rx->dqo.page_pool) {
		if (!netmem_is_net_iov(buf_state->page_info.netmem))
			prefetch(netmem_to_page(buf_state->page_info.netmem));
	} else {
		prefetch(buf_state->page_info.page);
	}
+#else
+	prefetch(buf_state->page_info.page);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
	...
}

@@
identifier buf_state;
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0))
buf_state->page_info.netmem = 0;
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
+buf_state->page_info.page = NULL;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */

@@
expression list args;
@@
static int gve_alloc_from_page_pool(...)
{
...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0))
	netmem_ref netmem;
+#else
+	struct page *page;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0))
	netmem = page_pool_alloc_netmem(args);

	if (!netmem)
		return -ENOMEM;

	buf_state->page_info.netmem = netmem;
	buf_state->page_info.page_address = netmem_address(netmem);
	buf_state->addr = page_pool_get_dma_addr_netmem(netmem);
+#else
+	page = page_pool_alloc(args);
+
+	if (!page)
+		return -ENOMEM;
+
+	buf_state->page_info.page = page;
+	buf_state->page_info.page_address = page_address(page);
+	buf_state->addr = page_pool_get_dma_addr(page);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
...
}

@@
@@
void gve_free_to_page_pool(...)
{
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0))
netmem_ref netmem = buf_state->page_info.netmem;
if (!netmem) {
	return;
}
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
+struct page *page = buf_state->page_info.page;
+if (!page) {
+	return;
+}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0))
	page_pool_put_full_netmem(...);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
+	page_pool_put_full_page(page->pp, page, allow_direct);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
...
}

@@
expression p;
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,16,0))
if (!gve_is_gqi(p) && !gve_is_qpl(p))
	dev->netmem_tx = true;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,16,0) */

@@
expression init, cond, inc;
@@
static void gve_unmap_packet(...)
{
...
for (init; cond; inc) {
...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,16,0))
	netmem_dma_unmap_page_attrs(...);
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(6,16,0) */
+dma_unmap_page(dev, dma_unmap_addr(pkt, dma[i]),
+		dma_unmap_len(pkt, len[i]), DMA_TO_DEVICE);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,16,0) */
...
}
...
}

@@
expression init, cond, inc;
@@
static int gve_tx_add_skb_no_copy_dqo(...)
{
...
for (init; cond; inc) {
...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,16,0))
	netmem_dma_unmap_addr_set(...);
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(6,16,0) */
+dma_unmap_addr_set(pkt, dma[pkt->num_bufs], addr);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,16,0) */
...
}
...
}

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0))
pp.flags |= PP_FLAG_ALLOW_UNREADABLE_NETMEM;
pp.queue_idx = rx->q_num;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0))
if (!rx->ctx.skb_head && rx->dqo.page_pool &&
    netmem_is_net_iov(buf_state->page_info.netmem))
{
	...
}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0))
if (rx->dqo.page_pool) {
	page_pool_dma_sync_netmem_for_cpu(rx->dqo.page_pool,
				buf_state->page_info.netmem,
				buf_state->page_info.page_offset,
				buf_len);
} else {
	dma_sync_single_range_for_cpu(&priv->pdev->dev, buf_state->addr,
				buf_state->page_info.page_offset +
				buf_state->page_info.pad,
				buf_len, DMA_FROM_DEVICE);
}
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
+	dma_sync_single_range_for_cpu(&priv->pdev->dev, buf_state->addr,
+				      buf_state->page_info.page_offset,
+				      buf_len, DMA_FROM_DEVICE);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */


@@
@@
if (eop && buf_len <= priv->rx_copybreak
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0))
    && !(rx->dqo.page_pool &&
	      netmem_is_net_iov(buf_state->page_info.netmem))
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
 )
{
	...
}
