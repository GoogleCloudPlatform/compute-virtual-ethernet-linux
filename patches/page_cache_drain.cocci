@@
identifier priv;
@@

static void gve_drain_page_cache(struct gve_priv *priv)
{
+#if LINUX_VERSION_CODE < KERNEL_VERSION(6,9,0) || RHEL_VERSION_CODE < RHEL_RELEASE_VERSION(10,0)
+    struct page_frag_cache *nc;
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6,9,0) || RHEL_VERSION_CODE < RHEL_RELEASE_VERSION(10,0) */
    int i;
+#if LINUX_VERSION_CODE < KERNEL_VERSION(6,9,0) || RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(10,0)
+    for (i = 0; i < priv->rx_cfg.num_queues; i++) {
+        nc = &priv->rx[i].page_cache;
+        if (nc->va) {
+            __page_frag_cache_drain(virt_to_page(nc->va),
+                        nc->pagecnt_bias);
+            nc->va = NULL;
+        }
+    }
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(6,9,0) || RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(10,0) */
    for (i = 0; i < priv->rx_cfg.num_queues; i++)
        page_frag_cache_drain(&priv->rx[i].page_cache);
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6,9,0) || RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(10,0) */
}
