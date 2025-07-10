@@
@@
static const struct ptp_clock_info gve_ptp_caps = {
	.owner          = THIS_MODULE,
	.name		= "gve clock",
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0))
	.do_aux_work	= gve_ptp_do_aux_work,
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0) */
...
};

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0))
ptp_schedule_worker(...);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0))
ptp_cancel_worker_sync(...);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0) */

@@
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,9,0)
skb_hwtstamps(skb)->hwtstamp = ...;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,9,0) */

@@
identifier netdev, info, priv;
@@

static int gve_get_ts_info(struct net_device *netdev,
			   struct kernel_ethtool_ts_info *info)
{
	struct gve_priv *priv = netdev_priv(netdev);

	ethtool_op_get_ts_info(netdev, info);

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,9,0)
	if (priv->nic_timestamp_supported) {
		...
	}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,9,0) */
	...
}

@@
@@
static int gve_get_ts_info(struct net_device *netdev
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0)
 , struct kernel_ethtool_ts_info *info
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0) */
+ , struct ethtool_ts_info *info
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0) */
 )
{
	...
}
