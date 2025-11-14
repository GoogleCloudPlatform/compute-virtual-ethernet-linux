@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
int gve_xdp_rx_timestamp(...);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
int gve_xdp_rx_timestamp(...)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */
