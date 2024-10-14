@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
static const struct netdev_stat_ops gve_stat_ops = {
...
};
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)) */

@@
identifier dev;
@@
static int gve_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
	dev->stat_ops = &gve_stat_ops;
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)) */
	...
}

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
static void gve_get_rx_queue_stats(...)
{
	...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
static void gve_get_tx_queue_stats(...)
{
	...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
static void gve_get_base_stats(...)
{
	...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)) */
