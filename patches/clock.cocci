@@
@@
static const struct ptp_clock_info gve_ptp_caps = {
	.owner          = THIS_MODULE,
	.name		= "gve clock",
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
	.gettimex64	= gve_ptp_gettimex64,
+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0)) */
+	.gettime64	= gve_ptp_gettime64,
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0)) */
	.settime64	= gve_ptp_settime64,
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0))
	.do_aux_work	= gve_ptp_do_aux_work,
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0) */
...
};

@wrap_ctx@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
struct gve_tsc_to_clock_callback_ctx { ... };
+#endif

@wrap_fn1@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
static int gve_tsc_to_clock_callback(...)
{
...
}
+#endif

@wrap_fn2@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
static int gve_tsc_to_clock_ts64(...)
{
...
}
+#endif

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
static int gve_ptp_gettimex64_adminq(...)
{
...
}
+#endif

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
static int gve_ptp_gettimex64_mmio(...)
{
...
}
+#endif

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
static int gve_mmio_getcrosststamp_fn(...)
{
...
}
+#endif

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
static int gve_ptp_getcrosststamp(...)
{
...
}
+#endif

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
	ptp->info.getcrosststamp = gve_ptp_getcrosststamp;
+#endif

@gettimex64_swap@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
static int gve_ptp_gettimex64(...) {...}
+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0)) */
+static int gve_ptp_gettime64(struct ptp_clock_info *info, struct timespec64 *ts)
+{
+	struct gve_ptp *ptp = container_of(info, struct gve_ptp, info);
+	struct gve_priv *priv = ptp->priv;
+	u64 nic_ts;
+	int err;
+
+	err = gve_clock_nic_ts_read(priv, &nic_ts, NULL, NULL);
+	if (err)
+		return err;
+
+	*ts = ns_to_timespec64(nic_ts);
+	return 0;
+}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0)) */

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

@@
expression dev, gran, res;
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(7,1,0) || RHEL_VERSION_GTE(10,3))
+	res = pci_enable_ptm(dev);
+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(7,1,0) || RHEL_VERSION_GTE(10,3)) */
	res = pci_enable_ptm(dev, gran);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(7,1,0) || RHEL_VERSION_GTE(10,3)) */
