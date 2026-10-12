@@
@@
static const struct ptp_clock_info gve_ptp_caps = {
	.owner          = THIS_MODULE,
	.name		= "gve clock",
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0))
	.gettimex64	= gve_ptp_gettimex64,
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0)) */
	.settime64	= gve_ptp_settime64,
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0))
	.do_aux_work	= gve_ptp_do_aux_work,
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0) */
...
};

// Swap in alternate implementation of gve_ptp_gettimex64 on kernels before
// "ptp: Switch to ktime_get_snapshot_id() for pre/post timestamps"
@gettimex64_swap@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(7,2,0))
static int gve_ptp_gettimex64(...) {...}
+#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0))
+static int gve_ptp_gettimex64(struct ptp_clock_info *info,
+			      struct timespec64 *ts,
+			      struct ptp_system_timestamp *sts)
+{
+	struct gve_ptp *ptp = container_of(info, struct gve_ptp, info);
+	struct gve_nic_clock_sample sample;
+	struct gve_priv *priv = ptp->priv;
+	int err;
+
+	struct system_time_snapshot snap;
+	if (sts)
+		ktime_get_snapshot(&snap);
+
+	err = gve_read_nic_ts(ptp, &sample, sts);
+	if (err)
+		return err;
+
+	gve_try_use_precise_sts(priv, &sample, sts, &snap);
+	*ts = ns_to_timespec64(sample.nic_timestamp);
+	return 0;
+}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0)) */

// Swap in alternate implementation of gve_try_use_precise_sts() on kernels before
// "ptp: Switch to ktime_get_snapshot_id() for pre/post timestamps"
@gve_try_use_precise_sts_swap@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(7,2,0))
static void gve_try_use_precise_sts(...) {...}
+#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0))
+static void gve_try_use_precise_sts(struct gve_priv *priv,
+				    struct gve_nic_clock_sample *sample,
+				    struct ptp_system_timestamp *sts,
+					struct system_time_snapshot *snap)
+{
+	ktime_t sys_pre, sys_post;
+	int err;
+
+	if (!sts)
+		return;
+
+	if (priv->clk_read_type != GVE_DEV_CLK_ADMINQ)
+		return;
+
+	if (!gve_can_use_cycles_from_nic(snap->cs_id, sts->clockid))
+		goto fallback_sts;
+
+	err = gve_cycles_to_ktime(priv, snap, sample->pre_cycles,
+				  &sys_pre);
+	if (err)
+		goto fallback_sts;
+
+	err = gve_cycles_to_ktime(priv, snap, sample->post_cycles,
+				  &sys_post);
+	if (err)
+		goto fallback_sts;
+
+	sts->pre_ts = ktime_to_timespec64(sys_pre);
+	sts->post_ts = ktime_to_timespec64(sys_post);
+	priv->ptp_precise_xtstamps++;
+	return;
+
+fallback_sts:
+	priv->ptp_fallback_xtstamps++;
+}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0)) */

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

// Delete all accesses to system_device_crosststamp::clock_id in kernels
// before commit "e3b8d03f timekeeping: Add CLOCK ID to system_device_crosststamp"
@@
struct system_device_crosststamp xtstamp;
struct system_device_crosststamp *xtstamp_ptr;
expression E;
@@
(
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(7,2,0))
xtstamp.clock_id = E;
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(7,2,0)) */
|
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(7,2,0))
xtstamp_ptr->clock_id = E;
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(7,2,0)) */
)

// Transform accesses to system_device_crosststamp::sys_systime to
// system_device_crosststamp::sys_realtime in kernels before commit
// "5ccaf1e5 timekeeping: Remove system_device_crosststamp::sys_realtime"
@@
struct system_device_crosststamp xtstamp;
struct system_device_crosststamp *xtstamp_ptr;
expression E;
@@
(
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(7,2,0))
E = xtstamp.sys_systime;
+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(7,2,0)) */
+E = xtstamp.sys_realtime;
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(7,2,0)) */
|
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(7,2,0))
E = xtstamp_ptr->sys_systime;
+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(7,2,0)) */
+E = xtstamp_ptr->sys_realtime;
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(7,2,0)) */
)

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

// Use the old form of pci_enable_ptm() on kernels before
// aa8671af "PCI/PTM: Drop pci_enable_ptm() granularity parameter"
@@
expression dev, res;
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(7,1,0) || RHEL_VERSION_GTE(10,3))
	res = pci_enable_ptm(dev);
+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(7,1,0) || RHEL_VERSION_GTE(10,3)) */
+	res = pci_enable_ptm(dev, NULL);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(7,1,0) || RHEL_VERSION_GTE(10,3)) */

// Replace ptp_system_timestamp::clockid with CLOCK_REALTIME on kernels
// before c259acab839e "ptp/ioctl: support MONOTONIC{,_RAW} timestamps for PTP_SYS_OFFSET_EXTENDED"
@@
struct ptp_system_timestamp *sts_ptr;
struct ptp_system_timestamp sts;
@@
(
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,12,0))
sts_ptr->clockid
+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,12,0)) */
+(CLOCK_REALTIME)
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,12,0)) */
|
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,12,0))
sts.clockid
+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,12,0)) */
+(CLOCK_REALTIME)
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,12,0)) */
)
