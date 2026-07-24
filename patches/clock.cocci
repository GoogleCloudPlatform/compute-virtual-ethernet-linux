// We only support gettimex64 on kernels >= 6.10 due to
// significant changes to the API in this version, otherwise
// we fall back to gettime64
@ptp_caps@
@@
static const struct ptp_clock_info gve_ptp_caps = {
	.owner          = THIS_MODULE,
	.name		= "gve clock",
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
	.gettimex64	= gve_ptp_gettimex64,
+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)) */
+	.gettime64	= gve_ptp_gettime64,
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)) */
	.settime64	= gve_ptp_settime64,
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0))
	.do_aux_work	= gve_ptp_do_aux_work,
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0) */
...
};

@wrap_ctx@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
struct gve_cycles_to_clock_callback_ctx { ... };
+#endif

@wrap_fn1@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
static int gve_cycles_to_clock_fn(...)
{
...
}
+#endif

@wrap_fn2@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
static int gve_cycles_to_timespec64(...)
{
...
}
+#endif

@gettimex64_swap@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0))
static int gve_ptp_gettimex64(...) {...}
+#else /*(LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)) */
+ static int gve_ptp_gettime64(struct ptp_clock_info *info, struct timespec64 *ts)
+ {
+   struct gve_ptp *ptp = container_of(info, struct gve_ptp, info);
+   struct gve_priv *priv = ptp->priv;
+   u64 nic_ts;
+   int err;
+
+   err = gve_clock_nic_ts_read(priv);
+   if (err)
+			return err;
+
+		*ts = ns_to_timespec64(nic_ts);
+		return 0;
+ }
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) */

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

// ptp_system_timestamp::clockid was added in c259aca (kernel 6.12)
@@
expression E1, E2, E3, E4, err;
struct ptp_system_timestamp *sts;
@@
- err = gve_cycles_to_timespec64(E1, sts->clockid, E2, E3, E4);
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,12,0)
+    err = gve_cycles_to_timespec64(E1, sts->clockid, E2, E3, E4);
+#else
+    err = gve_cycles_to_timespec64(E1, CLOCK_REALTIME, E2, E3, E4);
+#endif

@fsleep_replace@
expression E;
@@
-fsleep(E);
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,8,0)
+fsleep(E);
+#else
+usleep_range(E, E + 10);
+#endif
