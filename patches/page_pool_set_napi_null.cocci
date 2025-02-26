@@
@@
void gve_rx_stop_ring_dqo(...)
{
	...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0))
	page_pool_disable_direct_recycling(rx->dqo.page_pool);
+#else
+	rx->dqo.page_pool->p.napi = NULL;
+#endif
	...
}
