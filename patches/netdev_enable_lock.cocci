@@
expression netdev;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2)
	netdev_lock(netdev);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */

@@
expression netdev;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0)|| RHEL_VERSION_GTE(10,2)
	netdev_unlock(netdev);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */


@@
identifier block;
@@
void gve_add_napi(...)
{
	...
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0)|| RHEL_VERSION_GTE(10,2)
	if (block->rx)
		netif_napi_add_config_locked(priv->dev, &block->napi, gve_poll, q_idx);
	else
		netif_napi_add_locked(priv->dev, &block->napi, gve_poll);
+#elif LINUX_VERSION_CODE >= KERNEL_VERSION(6,13,0) || RHEL_VERSION_GTE(10,1)
+	if (block->rx)
+		netif_napi_add_config(priv->dev, &block->napi, gve_poll, q_idx);
+	else
+		netif_napi_add(priv->dev, &block->napi, gve_poll);
+#elif LINUX_VERSION_CODE >= KERNEL_VERSION(6,0,0) || RHEL_VERSION_GTE(9,2) || (RHEL_VERSION_GTE(8,8) && RHEL_VERSION_LT(9,0))
+	netif_napi_add(priv->dev, &block->napi, gve_poll);
+#else
+	netif_napi_add(priv->dev, &block->napi, gve_poll, NAPI_POLL_WEIGHT);
+#endif
	...
}

@@
expression napi, irq;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2)
netif_napi_set_irq_locked(napi, irq);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
+netif_napi_set_irq(napi, irq);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */

@@
expression napi;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2)
netif_napi_del_locked(napi);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
+netif_napi_del(napi);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */

@@
expression napi;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2)
		napi_disable_locked(napi);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
+		napi_disable(napi);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */

@@
expression napi;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2)
		napi_enable_locked(napi);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
+		napi_enable(napi);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */

