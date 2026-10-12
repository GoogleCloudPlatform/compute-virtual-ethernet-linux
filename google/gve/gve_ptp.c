// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/* Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2025 Google LLC
 */

#include "gve.h"
#include "gve_adminq.h"

/* Interval to schedule a nic timestamp calibration, 250ms. */
#define GVE_NIC_TS_SYNC_INTERVAL_MS 250
/* Time that a NIC clock reading remains valid after sampling
 * for the purposes of HW RX timestamp calculation.
 */
#define GVE_NIC_TS_VALID_MS 2000
#define GVE_CMD_SYNC_TRIGGER GENMASK(1, 0)
#define GVE_CMD_SYNC_SHTIME_EN BIT(2)

/*
 * Represents a sandwiched NIC clock read where pre/post_cycles
 * are in get_cycles() units.
 */
struct gve_nic_clock_sample {
	u64 nic_timestamp;
	u64 pre_cycles; /* Cycle counter from NIC before clock read */
	u64 post_cycles; /* Cycle counter from NIC after clock read */
};

static void gve_read_nic_ts_mmio(struct gve_ptp *ptp,
				u64 *dev_time_ns, struct ptp_system_timestamp *sts,
				u64 *sys_time_ns)
{
	u32 dev_high, dev_low, sys_high, sys_low;
	struct gve_priv *priv = ptp->priv;
	unsigned long flags;

	spin_lock_irqsave(&ptp->mmio_lock, flags);
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
	spin_unlock_irqrestore(&ptp->mmio_lock, flags);

	*dev_time_ns = (u64)dev_high << 32 | dev_low;
	if (sys_time_ns)
		*sys_time_ns = (u64)sys_high << 32 | sys_low;
}


bool gve_is_last_sync_nic_counter_valid(struct gve_priv *priv)
{
	return time_before(jiffies,
			   READ_ONCE(priv->last_sync_nic_jiffies) +
				   msecs_to_jiffies(GVE_NIC_TS_VALID_MS));
}

/*
 * Read NIC clock by issuing the AQ command. The command is subject to
 * rate limiting and may need to be retried. Requires nic_ts_read_lock
 * to be held.
 */
static int gve_ptp_read_timestamp(struct gve_ptp *ptp,
				  struct ptp_system_timestamp *sts)
{
	unsigned long deadline = jiffies + msecs_to_jiffies(100);
	struct gve_priv *priv = ptp->priv;
	unsigned long delay_us = 1000;
	int err;

	lockdep_assert_held(&ptp->nic_ts_read_lock);

	do {
		ptp_read_system_prets(sts);
		err = gve_adminq_report_nic_ts(priv,
					       ptp->nic_ts_report_bus);
		ptp_read_system_postts(sts);
		if (likely(err != -EAGAIN))
			return err;

		fsleep(delay_us);

		/* Exponential backoff */
		delay_us *= 2;
	} while (time_before(jiffies, deadline));

	return -ETIMEDOUT;
}

/* Read the nic timestamp from hardware via the admin queue. */
static int gve_read_nic_ts_adminq(struct gve_ptp *ptp,
				 struct gve_nic_clock_sample *sample,
				 struct ptp_system_timestamp *sts)
{
	struct gve_nic_ts_report *ts_report = ptp->nic_ts_report;
	int err;

	mutex_lock(&ptp->nic_ts_read_lock);
	err = gve_ptp_read_timestamp(ptp, sts);
	if (err)
		goto out;

	sample->nic_timestamp = be64_to_cpu(ts_report->nic_timestamp);
	sample->pre_cycles = be64_to_cpu(ts_report->pre_cycles);
	sample->post_cycles = be64_to_cpu(ts_report->post_cycles);

out:
	mutex_unlock(&ptp->nic_ts_read_lock);
	return err;
}

static int gve_read_nic_ts(struct gve_ptp *ptp,
				 struct gve_nic_clock_sample *sample,
				 struct ptp_system_timestamp *sts)
{
	switch (ptp->priv->clk_read_type) {
	case GVE_DEV_CLK_ADMINQ:
		return gve_read_nic_ts_adminq(ptp, sample, sts);
	case GVE_DEV_CLK_MMIO:
		gve_read_nic_ts_mmio(ptp, &sample->nic_timestamp, sts, NULL);
		return 0;
	default:
		return -ENOTSUPP;
	}
}

struct gve_cycles_to_clock_callback_ctx {
	u64 cycles;
};

static int gve_cycles_to_clock_fn(ktime_t *device_time,
				  struct system_counterval_t *system_counterval,
				  void *ctx)
{
	struct gve_cycles_to_clock_callback_ctx *context = ctx;

	*device_time = 0;

	system_counterval->cycles = context->cycles;
	system_counterval->use_nsecs = false;

	if (IS_ENABLED(CONFIG_X86))
		system_counterval->cs_id = CSID_X86_TSC;
	else if (IS_ENABLED(CONFIG_ARM64))
		system_counterval->cs_id = CSID_ARM_ARCH_COUNTER;
	else
		return -EOPNOTSUPP;

	return 0;
}

/*
 * Convert a raw cycle count (e.g. from get_cycles()) to CLOCK_REALTIME.
 * The system_time_snapshot must be taken before the cycle counter is sampled.
 */
static int gve_cycles_to_ktime(struct gve_priv *priv,
			       struct system_time_snapshot *snap, u64 cycles,
			       ktime_t *systime)
{
	struct gve_cycles_to_clock_callback_ctx ctx = {0};
	struct system_device_crosststamp xtstamp = {0};
	int err;

	ctx.cycles = cycles;
	xtstamp.clock_id = CLOCK_REALTIME;
	err = get_device_system_crosststamp(gve_cycles_to_clock_fn, &ctx, snap,
					    &xtstamp);
	if (err) {
		dev_err_ratelimited(&priv->pdev->dev,
				    "get_device_system_crosststamp() failed to convert %llu cycles to system time: %d\n",
				    cycles,
				    err);
		return err;
	}

	*systime = xtstamp.sys_systime;
	return 0;
}

static bool
gve_can_use_cycles_from_nic(enum clocksource_ids system_clock_source,
			    clockid_t clockid)
{
	if (clockid != CLOCK_REALTIME)
		return false;

	/* If the system clock source matches the system clock
	 * returned by the AdminQ command, we can use the system
	 * timestamps returned by the device, otherwise we have to
	 * fall back to sampling system time from the host which
	 * is less accurate.
	 */
	if (IS_ENABLED(CONFIG_X86))
		return system_clock_source == CSID_X86_TSC;
	else if (IS_ENABLED(CONFIG_ARM64))
		return system_clock_source == CSID_ARM_ARCH_COUNTER;

	return false;
}

static void gve_try_use_precise_sts(struct gve_priv *priv,
				    struct gve_nic_clock_sample *sample,
				    struct ptp_system_timestamp *sts)
{
	ktime_t sys_pre, sys_post;
	int err;

	if (!sts)
		return;

	if (priv->clk_read_type != GVE_DEV_CLK_ADMINQ)
		return;

	if (!gve_can_use_cycles_from_nic(sts->pre_sts.cs_id, sts->clockid))
		goto fallback_sts;

	/* Reject samples with out of order system clock values.
	 * Firmware must return valid non-zero cycle counts.
	 */
	if (!(sts->pre_sts.cycles <= sample->pre_cycles &&
	      sample->pre_cycles <= sample->post_cycles &&
	      sample->post_cycles <= sts->post_sts.cycles)) {
		dev_err_ratelimited(&priv->pdev->dev,
				    "AdminQ cycle counts out of order: %llu <= %llu <= %llu <= %llu\n",
				    sts->pre_sts.cycles, sample->pre_cycles,
				    sample->post_cycles, sts->post_sts.cycles);
		goto fallback_sts;
	}

	err = gve_cycles_to_ktime(priv, &sts->pre_sts, sample->pre_cycles,
				  &sys_pre);
	if (err)
		goto fallback_sts;

	err = gve_cycles_to_ktime(priv, &sts->pre_sts, sample->post_cycles,
				  &sys_post);
	if (err)
		goto fallback_sts;

	sts->pre_sts.systime = sys_pre;
	sts->post_sts.systime = sys_post;
	priv->ptp_precise_xtstamps++;
	return;

fallback_sts:
	priv->ptp_fallback_xtstamps++;
}

static int gve_ptp_adjfine(struct ptp_clock_info *ptp, long scaled_ppm)
{
	return -EOPNOTSUPP;
}

static int gve_ptp_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
	return -EOPNOTSUPP;
}

static int gve_ptp_gettimex64(struct ptp_clock_info *info,
			      struct timespec64 *ts,
			      struct ptp_system_timestamp *sts)
{
	struct gve_ptp *ptp = container_of(info, struct gve_ptp, info);
	struct gve_nic_clock_sample sample;
	struct gve_priv *priv = ptp->priv;
	int err;

	err = gve_read_nic_ts(ptp, &sample, sts);
	if (err)
		return err;

	gve_try_use_precise_sts(priv, &sample, sts);
	*ts = ns_to_timespec64(sample.nic_timestamp);
	return 0;
}

static int gve_ptp_settime64(struct ptp_clock_info *info,
			     const struct timespec64 *ts)
{
	return -EOPNOTSUPP;
}

static long gve_ptp_do_aux_work(struct ptp_clock_info *info)
{
	struct gve_ptp *ptp = container_of(info, struct gve_ptp, info);
	struct gve_nic_clock_sample sample;
	struct gve_priv *priv = ptp->priv;
	unsigned long before_jiffies;
	int err;

	before_jiffies = jiffies;
	err = gve_read_nic_ts(ptp, &sample, NULL);
	if (err) {
		dev_err_ratelimited(&priv->pdev->dev, "%s read err %d\n",
				    __func__, err);
		goto out;
	}
	WRITE_ONCE(priv->last_sync_nic_counter, sample.nic_timestamp);
	WRITE_ONCE(priv->last_sync_nic_jiffies, before_jiffies);

out:
	return msecs_to_jiffies(GVE_NIC_TS_SYNC_INTERVAL_MS);
}

static int gve_mmio_getcrosststamp_fn(ktime_t *device_time,
			       struct system_counterval_t *sys_counterval,
			       void *ctx)
{
	u64 dev_time_ns, sys_time_ns;
	struct gve_ptp *ptp = ctx;

	gve_read_nic_ts_mmio(ptp, &dev_time_ns, NULL, &sys_time_ns);

	*device_time = ns_to_ktime(dev_time_ns);
	sys_counterval->cs_id = CSID_X86_ART;
	sys_counterval->cycles = sys_time_ns;
	sys_counterval->use_nsecs = true;

	return 0;
}

static int gve_ptp_getcrosststamp(struct ptp_clock_info *info,
				  struct system_device_crosststamp *cts)
{
	struct gve_ptp *ptp = container_of_const(info, struct gve_ptp, info);

	return get_device_system_crosststamp(gve_mmio_getcrosststamp_fn, ptp, NULL,
					     cts);
}

static const struct ptp_clock_info gve_ptp_caps = {
	.owner          = THIS_MODULE,
	.name		= "gve clock",
	.adjfine	= gve_ptp_adjfine,
	.adjtime	= gve_ptp_adjtime,
	.gettimex64	= gve_ptp_gettimex64,
	.settime64	= gve_ptp_settime64,
	.do_aux_work	= gve_ptp_do_aux_work,
};

int gve_ptp_start_periodic(struct gve_priv *priv)
{
	struct gve_nic_clock_sample sample;
	int err;

	if (!priv->ptp)
		return -EOPNOTSUPP;

	err = gve_read_nic_ts(priv->ptp, &sample, NULL);
	if (err) {
		dev_err(&priv->pdev->dev, "Failed to start periodic clock reading: %d\n",
				    err);
		return err;
	}

	ptp_schedule_worker(priv->ptp->clock, 0);
	return 0;
}

void gve_ptp_stop_periodic(struct gve_priv *priv)
{
	if (!priv->ptp)
		return;

	ptp_cancel_worker_sync(priv->ptp->clock);
}

int gve_init_clock(struct gve_priv *priv)
{
	struct gve_ptp *ptp;
	int err;

	ptp = kzalloc_obj(*priv->ptp);
	if (!ptp)
		return -ENOMEM;

	ptp->info = gve_ptp_caps;
	ptp->priv = priv;
	spin_lock_init(&ptp->mmio_lock);
	mutex_init(&ptp->nic_ts_read_lock);
	ptp->nic_ts_report =
		dma_alloc_coherent(&priv->pdev->dev,
				   sizeof(struct gve_nic_ts_report),
				   &ptp->nic_ts_report_bus, GFP_KERNEL);
	if (!ptp->nic_ts_report) {
		dev_err(&priv->pdev->dev, "%s dma alloc error\n", __func__);
		err = -ENOMEM;
		goto free_ptp;
	}

#if IS_ENABLED(CONFIG_X86)
	if (priv->clk_read_type == GVE_DEV_CLK_MMIO &&
	    pcie_ptm_enabled(priv->pdev) && boot_cpu_has(X86_FEATURE_ART) &&
	    boot_cpu_has(X86_FEATURE_TSC_KNOWN_FREQ)) {
		ptp->info.getcrosststamp = gve_ptp_getcrosststamp;
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
		goto free_dma_mem;
	}

	priv->ptp = ptp;
	return 0;

free_dma_mem:
	dma_free_coherent(&priv->pdev->dev, sizeof(struct gve_nic_ts_report),
			  ptp->nic_ts_report, ptp->nic_ts_report_bus);
	ptp->nic_ts_report = NULL;
free_ptp:
	mutex_destroy(&ptp->nic_ts_read_lock);
	kfree(ptp);
	return err;
}

void gve_teardown_clock(struct gve_priv *priv)
{
	struct gve_ptp *ptp = priv->ptp;

	if (!ptp)
		return;

	priv->ptp = NULL;
	ptp_clock_unregister(ptp->clock);
	dma_free_coherent(&priv->pdev->dev, sizeof(struct gve_nic_ts_report),
			  ptp->nic_ts_report, ptp->nic_ts_report_bus);
	ptp->nic_ts_report = NULL;
	mutex_destroy(&ptp->nic_ts_read_lock);
	kfree(ptp);
}
