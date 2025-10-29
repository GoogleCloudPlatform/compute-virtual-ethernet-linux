@ptp_caps@
@@
static const struct ptp_clock_info gve_ptp_caps = {
	.owner          = THIS_MODULE,
	.name		= "gve clock",
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0))
	.gettimex64	= gve_ptp_gettimex64,
+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)) */
+	.gettime64	= gve_ptp_gettime64,
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0) */
	.settime64	= gve_ptp_settime64,
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0))
	.do_aux_work	= gve_ptp_do_aux_work,
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0) */
...
};

@gettimex64_swap@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0))
static int gve_ptp_gettimex64(...) {...}
+#else /*(LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)) */
+static int gve_ptp_gettime64(struct ptp_clock_info *ptp, struct timespec64 *ts)
+{
+	return -EOPNOTSUPP;
+}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0) */

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
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0) || RHEL_VERSION_GTE(9,6)
 , struct kernel_ethtool_ts_info *info
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0) || RHEL_VERSION_GTE(9,6) */
+ , struct ethtool_ts_info *info
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0) || RHEL_VERSION_GTE(9,6) */
 )
{
	...
}
