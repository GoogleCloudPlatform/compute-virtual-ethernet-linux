@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0))
int gve_xdp_rx_timestamp(...);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0))
int gve_xdp_rx_timestamp(...)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0))
static const struct xdp_metadata_ops gve_xdp_metadata_ops = {
...
};
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0))
if (!gve_is_gqi(priv))
	priv->dev->xdp_metadata_ops = &gve_xdp_metadata_ops;
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0))
XSK_CHECK_PRIV_TYPE(...);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)) */
