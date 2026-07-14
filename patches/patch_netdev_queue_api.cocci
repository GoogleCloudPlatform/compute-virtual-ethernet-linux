@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
#include <net/netdev_queues.h>
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
static const struct netdev_queue_mgmt_ops gve_queue_mgmt_ops = {
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
	dev->queue_mgmt_ops = &gve_queue_mgmt_ops;
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)) */
	...
}

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
static void gve_turnup_and_check_status(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
static int gve_rx_queue_mem_alloc(struct net_device *dev, void *per_q_mem,
				  int idx)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
static void gve_rx_queue_mem_free(struct net_device *dev, void *per_q_mem)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
static int gve_rx_queue_start(struct net_device *dev, void *per_q_mem, int idx)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
static int gve_rx_queue_stop(struct net_device *dev, void *per_q_mem, int idx)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)) */
