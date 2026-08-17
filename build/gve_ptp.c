// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/* Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2025 Google LLC
 */

#include "gve.h"
#include "gve_adminq.h"

/* Interval to schedule a nic timestamp calibration, 250ms. */
#define GVE_NIC_TS_SYNC_INTERVAL_MS 250
#define GVE_CMD_SYNC_TRIGGER GENMASK(1, 0)
#define GVE_CMD_SYNC_SHTIME_EN BIT(2)

static void gve_mmio_clock_read(struct gve_priv *priv,
				struct ptp_system_timestamp *sts,
				u64 *dev_time_ns, u64 *sys_time_ns)
{
	u32 dev_high, dev_low, sys_high, sys_low;
	unsigned long flags;

	BUG_ON(priv->clk_read_type != GVE_DEV_CLK_MMIO);

	spin_lock_irqsave(&priv->clk_lock, flags);
	ptp_read_system_prets(sts);
	iowrite32(GVE_CMD_SYNC_SHTIME_EN | GVE_CMD_SYNC_TRIGGER,
		  priv->dev_clk_cmd_sync);

	dev_high = ioread32(priv->dev_clk_ns_h);
	dev_low = ioread32(priv->dev_clk_ns_l);
	if (sys_time_ns) {
		sys_high = ioread32(priv->dev_art_ns_h);
		sys_low = ioread32(priv->dev_art_ns_l);
	}
	ptp_read_system_postts(sts);
	spin_unlock_irqrestore(&priv->clk_lock, flags);

	*dev_time_ns = (u64)dev_high << 32 | dev_low;
	if (sys_time_ns)
		*sys_time_ns = (u64)sys_high << 32 | sys_low;
}

// Requires priv->nic_ts_read_lock held
static int gve_read_adminq_ts_with_retry(struct gve_priv *priv, u64 *pre_tsc,
					 u64 *post_tsc)
{
	int err;
	int retry_count = 0;
	unsigned long delay_us = 1000; // Start with your 1ms limit

	do {
		*pre_tsc = rdtsc_ordered();
		err = gve_adminq_report_nic_ts(priv, priv->nic_ts_report_bus);
		*post_tsc = rdtsc_ordered();
		if (err != -EAGAIN) {
			return err;
		}

		// Exponential backoff: Double the delay
		fsleep(delay_us);

		if (delay_us < 100000) // Cap backoff at 100ms
			delay_us *= 2;

		retry_count++;
	} while (retry_count < 10);

	return -ETIMEDOUT;
}

/* Read the nic timestamp from hardware via the admin queue. */
static int gve_clock_nic_ts_read_adminq(struct gve_priv *priv, u64 *nic_raw,
				 u64 *pre_tsc, u64 *post_tsc)
{
	struct gve_nic_ts_report *ts_report;
	int err;
	u64 inner_pre_tsc, inner_post_tsc, outer_pre_tsc, outer_post_tsc;

	mutex_lock(&priv->nic_ts_read_lock);
	err = gve_read_adminq_ts_with_retry(priv, &outer_pre_tsc,
					    &outer_post_tsc);
	if (err) {
		dev_err_ratelimited(&priv->pdev->dev,
				    "AdminQ timestamp read failed: %d\n", err);
		goto out;
	}

	ts_report = priv->nic_ts_report;
	*nic_raw = be64_to_cpu(ts_report->nic_timestamp);
	inner_pre_tsc = be64_to_cpu(ts_report->pre_tsc);
	inner_post_tsc = be64_to_cpu(ts_report->post_tsc);

	if (!(outer_pre_tsc <= inner_pre_tsc &&
	      inner_post_tsc <= outer_post_tsc)) {
		dev_err_ratelimited(&priv->pdev->dev,
				    "AdminQ timestamp TSCs out of order: %lld %lld %lld %lld\n",
				    outer_pre_tsc, inner_pre_tsc,
				    inner_post_tsc, outer_post_tsc);
		err = -EBADMSG;
		goto out;
	}

	if (pre_tsc)
		*pre_tsc = inner_pre_tsc;
	if (post_tsc)
		*post_tsc = inner_post_tsc;

out:
	mutex_unlock(&priv->nic_ts_read_lock);
	return err;
}

static int gve_clock_nic_ts_read(struct gve_priv *priv, u64 *nic_raw,
				 u64 *pre_tsc, u64 *post_tsc)
{
	if (priv->clk_read_type == GVE_DEV_CLK_ADMINQ) {
		return gve_clock_nic_ts_read_adminq(priv, nic_raw, pre_tsc,
						    post_tsc);
	} else if (priv->clk_read_type == GVE_DEV_CLK_MMIO) {
		gve_mmio_clock_read(priv, NULL, nic_raw, NULL);
		return 0;
	} else
		return -ENOTSUPP;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
struct gve_tsc_to_clock_callback_ctx {
	u64 tsc;
};
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
static int
gve_tsc_to_clock_callback(ktime_t *device_time,
			  struct system_counterval_t *system_counterval,
			  void *ctx)
{
	struct gve_tsc_to_clock_callback_ctx *cb_ctx = ctx;

	*device_time = 0;

	system_counterval->cycles = cb_ctx->tsc;
	system_counterval->cs_id =
		IS_ENABLED(CONFIG_X86) ? CSID_X86_TSC : CSID_ARM_ARCH_COUNTER;
	system_counterval->use_nsecs = false;

	return 0;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
static int gve_tsc_to_clock_ts64(struct gve_priv *priv, clockid_t clockid,
				 struct system_time_snapshot *snap, u64 tsc,
				 struct timespec64 *ts)
{
	int err;
	struct system_device_crosststamp xtstamp;
	struct gve_tsc_to_clock_callback_ctx ctx = {
		.tsc = tsc,
	};

	err = get_device_system_crosststamp(gve_tsc_to_clock_callback, &ctx,
					    snap, &xtstamp);
	if (err) {
		dev_err_ratelimited(&priv->pdev->dev,
				    "get_device_system_crosststamp failed: %d\n",
				    err);
		return err;
	}

	switch (clockid) {
	case CLOCK_REALTIME:
		*ts = ktime_to_timespec64(xtstamp.sys_realtime);
		break;
	case CLOCK_MONOTONIC_RAW:
		*ts = ktime_to_timespec64(xtstamp.sys_monoraw);
		break;
	default:
		dev_err_ratelimited(&priv->pdev->dev,
				    "TSC conversion to clockid %d not supported\n",
				    clockid);
		return -ENOTSUPP;
	}

	return 0;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
static int gve_ptp_gettimex64_adminq(struct gve_priv *priv,
			      struct timespec64 *ts,
			      struct ptp_system_timestamp *sts)
{
	u64 nic_raw, pre_tsc, post_tsc;
	struct system_time_snapshot snap;
	struct timespec64 pre_ts, post_ts;
	int err;

	if (priv->clk_read_type != GVE_DEV_CLK_ADMINQ) {
		return -EOPNOTSUPP;
	}

	// 1. get history snapshot
	if (sts) {
		ktime_get_snapshot(&snap);
	}

	// 2. issue AQ command
	ptp_read_system_prets(sts);
	err = gve_clock_nic_ts_read_adminq(priv, &nic_raw, &pre_tsc, &post_tsc);
	if (err) {
		dev_err_ratelimited(&priv->pdev->dev,
				    "Failed to read NIC timestamp from AdminQ: %d\n",
				    err);
		return err;
	}
	ptp_read_system_postts(sts);

	if (sts) {
		// 3. convert pre_tsc to sys
		err = gve_tsc_to_clock_ts64(priv, sts->clockid, &snap, pre_tsc,
					    &pre_ts);
		if (err) {
			dev_err_ratelimited(&priv->pdev->dev,
					    "Failed to convert pre_tsc to clockid: %d\n",
					    sts->clockid);
			return err;
		}

		// 4. convert post_tsc to sys
		err = gve_tsc_to_clock_ts64(priv, sts->clockid, &snap, post_tsc,
					    &post_ts);
		if (err) {
			dev_err_ratelimited(&priv->pdev->dev,
					    "Failed to convert post_tsc to clockid: %d\n",
					    sts->clockid);
			return err;
		}

		// Validate sanity of sts
		if (timespec64_compare(&sts->pre_ts, &pre_ts) > 0 ||
		    timespec64_compare(&post_ts, &sts->post_ts) > 0) {
			dev_err_ratelimited(&priv->pdev->dev,
					    "TSC ordering not correct! clockid=%d timestamps=%lld %lld %lld %lld\n",
					    sts->clockid,
					    timespec64_to_ns(&sts->pre_ts),
					    timespec64_to_ns(&pre_ts),
					    timespec64_to_ns(&post_ts),
					    timespec64_to_ns(&sts->post_ts));
			return err;
		}
		sts->pre_ts = pre_ts;
		sts->post_ts = post_ts;
	}

	*ts = ns_to_timespec64(nic_raw);

	return 0;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
static int gve_ptp_gettimex64_mmio(struct gve_priv *priv,
			      struct timespec64 *ts,
			      struct ptp_system_timestamp *sts)
{
	u64 time_ns;
	gve_mmio_clock_read(priv, sts, &time_ns, NULL);
	*ts = ns_to_timespec64(time_ns);
	return 0;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
static int gve_ptp_gettimex64(struct ptp_clock_info *info,
			      struct timespec64 *ts,
			      struct ptp_system_timestamp *sts)
{
	struct gve_ptp *ptp = container_of(info, struct gve_ptp, info);
	struct gve_priv *priv = ptp->priv;

	if (!ptp->started)
		return -EBUSY;

	if (priv->clk_read_type == GVE_DEV_CLK_ADMINQ) {
		return gve_ptp_gettimex64_adminq(priv, ts, sts);
	} else if (priv->clk_read_type == GVE_DEV_CLK_MMIO) {
		return gve_ptp_gettimex64_mmio(priv, ts, sts);
	} else
		return -ENOTSUPP;
}
#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0)) */
static int gve_ptp_gettime64(struct ptp_clock_info *info,
		             struct timespec64 *ts) {
	struct gve_ptp *ptp = container_of(info, struct gve_ptp, info);
	struct gve_priv *priv = ptp->priv;
	u64 nic_ts;
	int err;

	err = gve_clock_nic_ts_read(priv, &nic_ts, NULL, NULL);
	if (err)
		return err;

	*ts = ns_to_timespec64(nic_ts);
	return 0;
}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0)) */

static int gve_ptp_settime64(struct ptp_clock_info *info,
			     const struct timespec64 *ts)
{
	return -EOPNOTSUPP;
}

static long gve_ptp_do_aux_work(struct ptp_clock_info *info)
{
	const struct gve_ptp *ptp = container_of(info, struct gve_ptp, info);
	struct gve_priv *priv = ptp->priv;
	u64 nic_raw;
	int err;

	if (!ptp->started)
		return -1;

	err = gve_clock_nic_ts_read(priv, &nic_raw, NULL, NULL);
	if (!err)
		WRITE_ONCE(priv->last_sync_nic_counter, nic_raw);
	else if (net_ratelimit())
		dev_err(&priv->pdev->dev, "%s read err %d\n", __func__, err);

	return msecs_to_jiffies(GVE_NIC_TS_SYNC_INTERVAL_MS);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
static int gve_mmio_getcrosststamp_fn(ktime_t *device_time,
			       struct system_counterval_t *sys_counterval,
			       void *ctx)
{
	u64 dev_time_ns, sys_time_ns;
	struct gve_priv *priv = ctx;

	gve_mmio_clock_read(priv, NULL, &dev_time_ns, &sys_time_ns);

	*device_time = ns_to_ktime(dev_time_ns);
	sys_counterval->cs_id = CSID_X86_ART;
	sys_counterval->cycles = sys_time_ns;
	sys_counterval->use_nsecs = true;

	return 0;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
static int gve_ptp_getcrosststamp(struct ptp_clock_info *info,
				  struct system_device_crosststamp *cts)
{
	struct gve_ptp *ptp = container_of_const(info, struct gve_ptp, info);
	struct gve_priv *priv = ptp->priv;

	if (!ptp->started)
		return -EBUSY;

	BUG_ON(priv->clk_read_type != GVE_DEV_CLK_MMIO);
	return get_device_system_crosststamp(gve_mmio_getcrosststamp_fn, priv, NULL,
					     cts);
}
#endif

static const struct ptp_clock_info gve_ptp_caps = {
	.owner = THIS_MODULE,
	.name = "gve clock",
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
	.gettimex64 = gve_ptp_gettimex64,
#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0)) */
	.gettime64 = gve_ptp_gettime64,
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0)) */
	
	.settime64 = gve_ptp_settime64,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0))
	.do_aux_work = gve_ptp_do_aux_work,
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0) */
};

int gve_ptp_start(struct gve_priv* priv)
{
	u64 nic_raw;
	int err;

	err = gve_clock_nic_ts_read(priv, &nic_raw, NULL, NULL);
	if (err) {
		dev_err(&priv->pdev->dev, "failed to read NIC clock %d\n", err);
		return err;
	}

	WRITE_ONCE(priv->last_sync_nic_counter, nic_raw);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0))
	ptp_schedule_worker(priv->ptp->clock,
			    msecs_to_jiffies(GVE_NIC_TS_SYNC_INTERVAL_MS));
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0) */

	priv->ptp->started = true;
	return 0;
}

void gve_ptp_stop(struct gve_priv* priv)
{
	if (!priv->ptp)
		return;
	priv->ptp->started = false;
	if (priv->ptp->clock) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0))
		ptp_cancel_worker_sync(priv->ptp->clock);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0) */
	}
}

int gve_ptp_register(struct gve_priv *priv)
{
	struct gve_ptp *ptp;
	int err;

	spin_lock_init(&priv->clk_lock);
	mutex_init(&priv->nic_ts_read_lock);
	priv->nic_ts_report = dma_alloc_coherent(
		&priv->pdev->dev, sizeof(struct gve_nic_ts_report),
		&priv->nic_ts_report_bus, GFP_KERNEL);
	if (!priv->nic_ts_report) {
		dev_err(&priv->pdev->dev, "%s dma alloc error\n", __func__);
		err = -ENOMEM;
		goto free_mutex;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
	priv->ptp = kzalloc_obj(*priv->ptp);
#else
	priv->ptp = kzalloc(sizeof(*priv->ptp), GFP_KERNEL);
#endif
	if (!priv->ptp) {
		err = -ENOMEM;
		goto free_dma_mem;
	}

	ptp = priv->ptp;
	ptp->priv = priv;
	ptp->info = gve_ptp_caps;

#if IS_ENABLED(CONFIG_X86)
	if (priv->clk_read_type == GVE_DEV_CLK_MMIO &&
	    pcie_ptm_enabled(priv->pdev) && boot_cpu_has(X86_FEATURE_ART) &&
	    boot_cpu_has(X86_FEATURE_TSC_KNOWN_FREQ)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(7,1,0))
		ptp->info.getcrosststamp = gve_ptp_getcrosststamp;
#endif
		dev_info(&priv->pdev->dev, "Cross timestamp is supported\n");
	} else {
		dev_info(&priv->pdev->dev,
			 "Cross timestamp is not supported. clk_read_type: %d, ptm: %d, art: %d, tsc: %d\n",
			priv->clk_read_type, pcie_ptm_enabled(priv->pdev),
		        boot_cpu_has(X86_FEATURE_ART), boot_cpu_has(X86_FEATURE_TSC_KNOWN_FREQ));
	}
#endif /* CONFIG_X86 */

	ptp->clock = ptp_clock_register(&ptp->info, &priv->pdev->dev);
	if (IS_ERR(ptp->clock)) {
		dev_err(&priv->pdev->dev, "PTP clock registration failed\n");
		err = PTR_ERR(ptp->clock);
		ptp->clock = NULL;
		goto free_ptp;
	}


	return 0;

free_ptp:
	priv->ptp = NULL;
	kfree(ptp);

free_dma_mem:
	dma_free_coherent(&priv->pdev->dev, sizeof(struct gve_nic_ts_report),
			  priv->nic_ts_report, priv->nic_ts_report_bus);
	priv->nic_ts_report = NULL;

free_mutex:
	mutex_destroy(&priv->nic_ts_read_lock);
	return err;
}

void gve_ptp_unregister(struct gve_priv *priv)
{
	struct gve_ptp *ptp = priv->ptp;

	if (!ptp)
		return;

	if (ptp->clock) {
		ptp_clock_unregister(ptp->clock);
		ptp->clock = NULL;
	}
	priv->ptp = NULL;

	kfree(ptp);

	if (priv->nic_ts_report) {
		dma_free_coherent(&priv->pdev->dev,
				  sizeof(struct gve_nic_ts_report),
				  priv->nic_ts_report, priv->nic_ts_report_bus);
		priv->nic_ts_report = NULL;
	}

	mutex_destroy(&priv->nic_ts_read_lock);
}
