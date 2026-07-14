// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/* Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2015-2021 Google, Inc.
 */

#include <linux/etherdevice.h>
#include <linux/pci.h>
#include "gve.h"
#include "gve_adminq.h"
#include "gve_register.h"

#define GVE_MAX_ADMINQ_RELEASE_CHECK 500
#define GVE_ADMINQ_SLEEP_LEN 20
#define GVE_MAX_ADMINQ_EVENT_COUNTER_CHECK 100

#define GVE_DEVICE_OPTION_ERROR_FMT               \
	"%s option error:\n"                      \
	"Expected: length=%d, feature_mask=%x.\n" \
	"Actual: length=%d, feature_mask=%x.\n"

#define GVE_DEVICE_OPTION_TOO_BIG_FMT \
	"Length of %s option larger than expected. Possible older version of guest driver.\n"

static struct gve_device_option *
gve_get_next_option(struct gve_device_descriptor *descriptor,
		    struct gve_device_option *option)
{
	void *option_end, *descriptor_end;

	option_end = (void *)(option + 1) + be16_to_cpu(option->option_length);
	descriptor_end =
		(void *)descriptor + be16_to_cpu(descriptor->total_length);

	return option_end > descriptor_end ?
		       NULL :
		       (struct gve_device_option *)option_end;
}

#define GVE_DEVICE_OPTION_NO_MIN_RING_SIZE 8

static void gve_parse_device_option(
	struct gve_adapter *adapter,
	struct gve_device_descriptor *device_descriptor,
	struct gve_device_option *option,
	struct gve_device_option_gqi_rda **dev_op_gqi_rda,
	struct gve_device_option_gqi_qpl **dev_op_gqi_qpl,
	struct gve_device_option_dqo_rda **dev_op_dqo_rda,
	struct gve_device_option_jumbo_frames **dev_op_jumbo_frames,
	struct gve_device_option_dqo_qpl **dev_op_dqo_qpl,
	struct gve_device_option_buffer_sizes **dev_op_buffer_sizes,
	struct gve_device_option_flow_steering **dev_op_flow_steering,
	struct gve_device_option_rss_config **dev_op_rss_config,
	struct gve_device_option_nic_timestamp **dev_op_nic_timestamp,
	struct gve_device_option_modify_ring **dev_op_modify_ring)
{
	u32 req_feat_mask = be32_to_cpu(option->required_features_mask);
	u16 option_length = be16_to_cpu(option->option_length);
	u16 option_id = be16_to_cpu(option->option_id);
	struct device *dev = &adapter->pdev->dev;

	/* If the length or feature mask doesn't match, continue without
	 * enabling the feature.
	 */
	switch (option_id) {
	case GVE_DEV_OPT_ID_GQI_RAW_ADDRESSING:
		if (option_length != GVE_DEV_OPT_LEN_GQI_RAW_ADDRESSING ||
		    req_feat_mask !=
			    GVE_DEV_OPT_REQ_FEAT_MASK_GQI_RAW_ADDRESSING) {
			dev_warn(dev, GVE_DEVICE_OPTION_ERROR_FMT,
				 "Raw Addressing",
				 GVE_DEV_OPT_LEN_GQI_RAW_ADDRESSING,
				 GVE_DEV_OPT_REQ_FEAT_MASK_GQI_RAW_ADDRESSING,
				 option_length, req_feat_mask);
			break;
		}

		dev_info(dev, "Gqi raw addressing device option enabled.\n");
		adapter->device_info->queue_format = GVE_GQI_RDA_FORMAT;
		break;
	case GVE_DEV_OPT_ID_GQI_RDA:
		if (option_length < sizeof(**dev_op_gqi_rda) ||
		    req_feat_mask != GVE_DEV_OPT_REQ_FEAT_MASK_GQI_RDA) {
			dev_warn(dev, GVE_DEVICE_OPTION_ERROR_FMT, "GQI RDA",
				 (int)sizeof(**dev_op_gqi_rda),
				 GVE_DEV_OPT_REQ_FEAT_MASK_GQI_RDA,
				 option_length, req_feat_mask);
			break;
		}

		if (option_length > sizeof(**dev_op_gqi_rda)) {
			dev_warn(dev, GVE_DEVICE_OPTION_TOO_BIG_FMT, "GQI RDA");
		}
		*dev_op_gqi_rda = (void *)(option + 1);
		break;
	case GVE_DEV_OPT_ID_GQI_QPL:
		if (option_length < sizeof(**dev_op_gqi_qpl) ||
		    req_feat_mask != GVE_DEV_OPT_REQ_FEAT_MASK_GQI_QPL) {
			dev_warn(dev, GVE_DEVICE_OPTION_ERROR_FMT, "GQI QPL",
				 (int)sizeof(**dev_op_gqi_qpl),
				 GVE_DEV_OPT_REQ_FEAT_MASK_GQI_QPL,
				 option_length, req_feat_mask);
			break;
		}

		if (option_length > sizeof(**dev_op_gqi_qpl)) {
			dev_warn(dev, GVE_DEVICE_OPTION_TOO_BIG_FMT, "GQI QPL");
		}
		*dev_op_gqi_qpl = (void *)(option + 1);
		break;
	case GVE_DEV_OPT_ID_DQO_RDA:
		if (option_length < sizeof(**dev_op_dqo_rda) ||
		    req_feat_mask != GVE_DEV_OPT_REQ_FEAT_MASK_DQO_RDA) {
			dev_warn(dev, GVE_DEVICE_OPTION_ERROR_FMT, "DQO RDA",
				 (int)sizeof(**dev_op_dqo_rda),
				 GVE_DEV_OPT_REQ_FEAT_MASK_DQO_RDA,
				 option_length, req_feat_mask);
			break;
		}

		if (option_length > sizeof(**dev_op_dqo_rda)) {
			dev_warn(dev, GVE_DEVICE_OPTION_TOO_BIG_FMT, "DQO RDA");
		}
		*dev_op_dqo_rda = (void *)(option + 1);
		break;
	case GVE_DEV_OPT_ID_DQO_QPL:
		if (option_length < sizeof(**dev_op_dqo_qpl) ||
		    req_feat_mask != GVE_DEV_OPT_REQ_FEAT_MASK_DQO_QPL) {
			dev_warn(dev, GVE_DEVICE_OPTION_ERROR_FMT, "DQO QPL",
				 (int)sizeof(**dev_op_dqo_qpl),
				 GVE_DEV_OPT_REQ_FEAT_MASK_DQO_QPL,
				 option_length, req_feat_mask);
			break;
		}

		if (option_length > sizeof(**dev_op_dqo_qpl)) {
			dev_warn(dev, GVE_DEVICE_OPTION_TOO_BIG_FMT, "DQO QPL");
		}
		*dev_op_dqo_qpl = (void *)(option + 1);
		break;
	case GVE_DEV_OPT_ID_JUMBO_FRAMES:
		if (option_length < sizeof(**dev_op_jumbo_frames) ||
		    req_feat_mask != GVE_DEV_OPT_REQ_FEAT_MASK_JUMBO_FRAMES) {
			dev_warn(dev, GVE_DEVICE_OPTION_ERROR_FMT,
				 "Jumbo Frames",
				 (int)sizeof(**dev_op_jumbo_frames),
				 GVE_DEV_OPT_REQ_FEAT_MASK_JUMBO_FRAMES,
				 option_length, req_feat_mask);
			break;
		}

		if (option_length > sizeof(**dev_op_jumbo_frames)) {
			dev_warn(dev, GVE_DEVICE_OPTION_TOO_BIG_FMT,
				 "Jumbo Frames");
		}
		*dev_op_jumbo_frames = (void *)(option + 1);
		break;
	case GVE_DEV_OPT_ID_BUFFER_SIZES:
		if (option_length < sizeof(**dev_op_buffer_sizes) ||
		    req_feat_mask != GVE_DEV_OPT_REQ_FEAT_MASK_BUFFER_SIZES) {
			dev_warn(dev, GVE_DEVICE_OPTION_ERROR_FMT,
				 "Buffer Sizes",
				 (int)sizeof(**dev_op_buffer_sizes),
				 GVE_DEV_OPT_REQ_FEAT_MASK_BUFFER_SIZES,
				 option_length, req_feat_mask);
			break;
		}

		if (option_length > sizeof(**dev_op_buffer_sizes))
			dev_warn(dev, GVE_DEVICE_OPTION_TOO_BIG_FMT,
				 "Buffer Sizes");
		*dev_op_buffer_sizes = (void *)(option + 1);
		break;
	case GVE_DEV_OPT_ID_MODIFY_RING:
		if (option_length < GVE_DEVICE_OPTION_NO_MIN_RING_SIZE ||
		    req_feat_mask != GVE_DEV_OPT_REQ_FEAT_MASK_MODIFY_RING) {
			dev_warn(dev, GVE_DEVICE_OPTION_ERROR_FMT,
				 "Modify Ring",
				 (int)sizeof(**dev_op_modify_ring),
				 GVE_DEV_OPT_REQ_FEAT_MASK_MODIFY_RING,
				 option_length, req_feat_mask);
			break;
		}

		if (option_length > sizeof(**dev_op_modify_ring)) {
			dev_warn(dev, GVE_DEVICE_OPTION_TOO_BIG_FMT,
				 "Modify Ring");
		}

		*dev_op_modify_ring = (void *)(option + 1);

		/* device has not provided min ring size */
		if (option_length == GVE_DEVICE_OPTION_NO_MIN_RING_SIZE)
			adapter->device_info->default_min_ring_size = true;
		break;
	case GVE_DEV_OPT_ID_FLOW_STEERING:
		if (option_length < sizeof(**dev_op_flow_steering) ||
		    req_feat_mask != GVE_DEV_OPT_REQ_FEAT_MASK_FLOW_STEERING) {
			dev_warn(dev, GVE_DEVICE_OPTION_ERROR_FMT,
				 "Flow Steering",
				 (int)sizeof(**dev_op_flow_steering),
				 GVE_DEV_OPT_REQ_FEAT_MASK_FLOW_STEERING,
				 option_length, req_feat_mask);
			break;
		}

		if (option_length > sizeof(**dev_op_flow_steering))
			dev_warn(dev, GVE_DEVICE_OPTION_TOO_BIG_FMT,
				 "Flow Steering");
		*dev_op_flow_steering = (void *)(option + 1);
		break;
	case GVE_DEV_OPT_ID_RSS_CONFIG:
		if (option_length < sizeof(**dev_op_rss_config) ||
		    req_feat_mask != GVE_DEV_OPT_REQ_FEAT_MASK_RSS_CONFIG) {
			dev_warn(dev, GVE_DEVICE_OPTION_ERROR_FMT, "RSS config",
				 (int)sizeof(**dev_op_rss_config),
				 GVE_DEV_OPT_REQ_FEAT_MASK_RSS_CONFIG,
				 option_length, req_feat_mask);
			break;
		}

		if (option_length > sizeof(**dev_op_rss_config))
			dev_warn(dev, GVE_DEVICE_OPTION_TOO_BIG_FMT,
				 "RSS config");
		*dev_op_rss_config = (void *)(option + 1);
		break;
	case GVE_DEV_OPT_ID_NIC_TIMESTAMP:
		if (option_length < sizeof(**dev_op_nic_timestamp) ||
		    req_feat_mask != GVE_DEV_OPT_REQ_FEAT_MASK_NIC_TIMESTAMP) {
			dev_warn(dev, GVE_DEVICE_OPTION_ERROR_FMT,
				 "Nic Timestamp",
				 (int)sizeof(**dev_op_nic_timestamp),
				 GVE_DEV_OPT_REQ_FEAT_MASK_NIC_TIMESTAMP,
				 option_length, req_feat_mask);
			break;
		}

		if (option_length > sizeof(**dev_op_nic_timestamp))
			dev_warn(dev, GVE_DEVICE_OPTION_TOO_BIG_FMT,
				 "Nic Timestamp");
		*dev_op_nic_timestamp = (void *)(option + 1);
		break;
	default:
		/* If we don't recognize the option just continue
		 * without doing anything.
		 */
		dev_dbg(dev, "Unrecognized device option 0x%hx not enabled.\n",
			option_id);
	}
}

/* Process all device options for a given describe device call. */
static int gve_process_device_options(
	struct gve_adapter *adapter, struct gve_device_descriptor *descriptor,
	struct gve_device_option_gqi_rda **dev_op_gqi_rda,
	struct gve_device_option_gqi_qpl **dev_op_gqi_qpl,
	struct gve_device_option_dqo_rda **dev_op_dqo_rda,
	struct gve_device_option_jumbo_frames **dev_op_jumbo_frames,
	struct gve_device_option_dqo_qpl **dev_op_dqo_qpl,
	struct gve_device_option_buffer_sizes **dev_op_buffer_sizes,
	struct gve_device_option_flow_steering **dev_op_flow_steering,
	struct gve_device_option_rss_config **dev_op_rss_config,
	struct gve_device_option_nic_timestamp **dev_op_nic_timestamp,
	struct gve_device_option_modify_ring **dev_op_modify_ring)
{
	const int num_options = be16_to_cpu(descriptor->num_device_options);
	struct gve_device_option *dev_opt;
	int i;

	/* The options struct directly follows the device descriptor. */
	dev_opt = (void *)(descriptor + 1);
	for (i = 0; i < num_options; i++) {
		struct gve_device_option *next_opt;

		next_opt = gve_get_next_option(descriptor, dev_opt);
		if (!next_opt) {
			dev_err(&adapter->pdev->dev,
				"options exceed device_descriptor's total length.\n");
			return -EINVAL;
		}

		gve_parse_device_option(adapter, descriptor, dev_opt,
					dev_op_gqi_rda, dev_op_gqi_qpl,
					dev_op_dqo_rda, dev_op_jumbo_frames,
					dev_op_dqo_qpl, dev_op_buffer_sizes,
					dev_op_flow_steering, dev_op_rss_config,
					dev_op_nic_timestamp,
					dev_op_modify_ring);
		dev_opt = next_opt;
	}

	return 0;
}

int gve_adminq_alloc(struct gve_adapter *adapter)
{
	struct gve_adminq_registers __iomem *reg_bar0 = adapter->reg_bar0;
	struct device *dev;
	int err;

	dev = &adapter->pdev->dev;

	adapter->adminq_pool = dma_pool_create("adminq_pool", dev,
					       GVE_ADMINQ_BUFFER_SIZE, 0, 0);
	if (unlikely(!adapter->adminq_pool)) {
		err = -ENOMEM;
		goto err;
	}
	adapter->adminq = dma_pool_alloc(adapter->adminq_pool, GFP_KERNEL,
					 &adapter->adminq_bus_addr);
	if (unlikely(!adapter->adminq)) {
		dma_pool_destroy(adapter->adminq_pool);
		err = -ENOMEM;
		goto err;
	}

	adapter->adminq_mask =
		(GVE_ADMINQ_BUFFER_SIZE / sizeof(union gve_adminq_command)) - 1;
	adapter->adminq_prod_cnt = 0;
	adapter->adminq_cmd_fail = 0;
	adapter->adminq_timeouts = 0;
	adapter->adminq_describe_device_cnt = 0;
	adapter->adminq_cfg_device_resources_cnt = 0;
	adapter->adminq_register_page_list_cnt = 0;
	adapter->adminq_unregister_page_list_cnt = 0;
	adapter->adminq_create_tx_queue_cnt = 0;
	adapter->adminq_create_rx_queue_cnt = 0;
	adapter->adminq_destroy_tx_queue_cnt = 0;
	adapter->adminq_destroy_rx_queue_cnt = 0;
	adapter->adminq_dcfg_device_resources_cnt = 0;
	adapter->adminq_set_driver_parameter_cnt = 0;
	adapter->adminq_report_stats_cnt = 0;
	adapter->adminq_report_link_speed_cnt = 0;
	adapter->adminq_report_nic_timestamp_cnt = 0;
	adapter->adminq_get_ptype_map_cnt = 0;
	adapter->adminq_query_flow_rules_cnt = 0;
	adapter->adminq_cfg_flow_rule_cnt = 0;
	adapter->adminq_cfg_rss_cnt = 0;
	adapter->adminq_query_rss_cnt = 0;

	/* Setup Admin queue with the device */
	if (adapter->pdev->revision < 0x1) {
		iowrite32be(adapter->adminq_bus_addr / PAGE_SIZE,
			    &reg_bar0->adminq_pfn);
	} else {
		iowrite16be(GVE_ADMINQ_BUFFER_SIZE, &reg_bar0->adminq_length);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		iowrite32be(adapter->adminq_bus_addr >> 32,
			    &reg_bar0->adminq_base_address_hi);
#endif
		iowrite32be(adapter->adminq_bus_addr,
			    &reg_bar0->adminq_base_address_lo);
		iowrite32be(GVE_DRIVER_STATUS_RUN_MASK,
			    &reg_bar0->driver_status);
	}
	gve_set_admin_queue_ok(adapter);
	return 0;
err:
	dev_err(&adapter->pdev->dev, "Failed to alloc admin queue: err=%d\n",
		err);
	return err;
}

int gve_adminq_init(struct gve_adapter *adapter)
{
	struct gve_adminq_registers __iomem *reg_bar = adapter->reg_bar0;

	gve_adminq_write_version(&reg_bar->driver_version);
	return gve_adminq_alloc(adapter);
}

static void gve_adminq_release(struct gve_adapter *adapter)
{
	struct gve_adminq_registers __iomem *reg_bar0 = adapter->reg_bar0;
	int i = 0;

	/* Tell the device the adminq is leaving */
	if (adapter->pdev->revision < 0x1) {
		iowrite32be(0x0, &reg_bar0->adminq_pfn);
		while (ioread32be(&reg_bar0->adminq_pfn)) {
			/* If this is reached the device is unrecoverable and still
			 * holding memory. Continue looping to avoid memory corruption,
			 * but WARN so it is visible what is going on.
			 */
			if (i == GVE_MAX_ADMINQ_RELEASE_CHECK)
				WARN(1, "Unrecoverable platform error!");
			i++;
			msleep(GVE_ADMINQ_SLEEP_LEN);
		}
	} else {
		iowrite32be(GVE_DRIVER_STATUS_RESET_MASK,
			    &reg_bar0->driver_status);
		while (!(ioread32be(&reg_bar0->device_status) &
			 GVE_DEVICE_STATUS_DEVICE_IS_RESET)) {
			if (i == GVE_MAX_ADMINQ_RELEASE_CHECK)
				WARN(1, "Unrecoverable platform error!");
			i++;
			msleep(GVE_ADMINQ_SLEEP_LEN);
		}
	}
	gve_clear_device_rings_ok(adapter);
	gve_clear_device_resources_ok(adapter);
}

void gve_adminq_free(struct gve_adapter *adapter)
{
	/* Wait for any in-progress command to complete */
	mutex_lock(&adapter->adminq_lock);
	if (!gve_get_admin_queue_ok(adapter))
		return;
	gve_clear_admin_queue_ok(adapter);
	mutex_unlock(&adapter->adminq_lock);

	gve_adminq_release(adapter);
	dma_pool_free(adapter->adminq_pool, adapter->adminq,
		      adapter->adminq_bus_addr);
	dma_pool_destroy(adapter->adminq_pool);
}

static void gve_adminq_kick_cmd(struct gve_adapter *adapter, u32 prod_cnt)
{
	struct gve_adminq_registers __iomem *reg_bar0 = adapter->reg_bar0;

	iowrite32be(prod_cnt, &reg_bar0->adminq_doorbell);
}

static bool gve_adminq_wait_for_cmd(struct gve_adapter *adapter, u32 prod_cnt)
{
	struct gve_adminq_registers __iomem *reg_bar0 = adapter->reg_bar0;
	int i;

	for (i = 0; i < GVE_MAX_ADMINQ_EVENT_COUNTER_CHECK; i++) {
		if (ioread32be(&reg_bar0->adminq_event_counter) == prod_cnt)
			return true;
		msleep(GVE_ADMINQ_SLEEP_LEN);
	}

	return false;
}

static int adminq_status_to_err(struct gve_adapter *adapter, u32 status)
{
	switch (status) {
	case GVE_ADMINQ_COMMAND_PASSED:
		return 0;
	case GVE_ADMINQ_COMMAND_UNSET:
		dev_err(&adapter->pdev->dev,
			"parse_aq_err: err and status both unset, this should not be possible.\n");
		return -EINVAL;
	case GVE_ADMINQ_COMMAND_ERROR_ABORTED:
	case GVE_ADMINQ_COMMAND_ERROR_CANCELLED:
	case GVE_ADMINQ_COMMAND_ERROR_DATALOSS:
	case GVE_ADMINQ_COMMAND_ERROR_FAILED_PRECONDITION:
	case GVE_ADMINQ_COMMAND_ERROR_UNAVAILABLE:
		return -EAGAIN;
	case GVE_ADMINQ_COMMAND_ERROR_ALREADY_EXISTS:
	case GVE_ADMINQ_COMMAND_ERROR_INTERNAL_ERROR:
	case GVE_ADMINQ_COMMAND_ERROR_INVALID_ARGUMENT:
	case GVE_ADMINQ_COMMAND_ERROR_NOT_FOUND:
	case GVE_ADMINQ_COMMAND_ERROR_OUT_OF_RANGE:
	case GVE_ADMINQ_COMMAND_ERROR_UNKNOWN_ERROR:
		return -EINVAL;
	case GVE_ADMINQ_COMMAND_ERROR_DEADLINE_EXCEEDED:
		return -ETIME;
	case GVE_ADMINQ_COMMAND_ERROR_PERMISSION_DENIED:
	case GVE_ADMINQ_COMMAND_ERROR_UNAUTHENTICATED:
		return -EACCES;
	case GVE_ADMINQ_COMMAND_ERROR_RESOURCE_EXHAUSTED:
		return -ENOMEM;
	case GVE_ADMINQ_COMMAND_ERROR_UNIMPLEMENTED:
		return -EOPNOTSUPP;
	default:
		dev_err(&adapter->pdev->dev,
			"parse_aq_err: unknown status code %d\n", status);
		return -EINVAL;
	}
}

static int gve_adminq_parse_err(struct gve_adapter *adapter, u32 status)
{
	int err = adminq_status_to_err(adapter, status);
	if (err != 0 && err != -EAGAIN) {
		dev_err_ratelimited(&adapter->pdev->dev, "AQ command failed with status %d\n", status);
		adapter->adminq_cmd_fail++;
	}

	return err;
}

/* Flushes all AQ commands currently queued and waits for them to complete.
 * If there are failures, it will return the first error.
 */
static int gve_adminq_kick_and_wait(struct gve_adapter *adapter)
{
	struct gve_adminq_registers __iomem *reg_bar0 = adapter->reg_bar0;
	int tail, head;
	int i;

	lockdep_assert_held(&adapter->adminq_lock);

	tail = ioread32be(&reg_bar0->adminq_event_counter);
	head = adapter->adminq_prod_cnt;

	gve_adminq_kick_cmd(adapter, head);
	if (!gve_adminq_wait_for_cmd(adapter, head)) {
		dev_err(&adapter->pdev->dev,
			"AQ commands timed out, need to reset AQ\n");
		adapter->adminq_timeouts++;
		return -ENOTRECOVERABLE;
	}

	for (i = tail; i < head; i++) {
		union gve_adminq_command *cmd;
		u32 status, err;

		cmd = &adapter->adminq[i & adapter->adminq_mask];
		status = be32_to_cpu(READ_ONCE(cmd->status));
		err = gve_adminq_parse_err(adapter, status);
		if (err)
			// Return the first error if we failed.
			return err;
	}

	return 0;
}

static int gve_adminq_issue_cmd(struct gve_adapter *adapter,
				union gve_adminq_command *cmd_orig)
{
	struct gve_adminq_registers __iomem *reg_bar0 = adapter->reg_bar0;
	union gve_adminq_command *cmd;
	u32 opcode;
	u32 tail;

	lockdep_assert_held(&adapter->adminq_lock);
	if (!gve_get_admin_queue_ok(adapter))
		return -EBUSY;

	tail = ioread32be(&reg_bar0->adminq_event_counter);

	// Check if next command will overflow the buffer.
	if (((adapter->adminq_prod_cnt + 1) & adapter->adminq_mask) ==
	    (tail & adapter->adminq_mask)) {
		int err;

		// Flush existing commands to make room.
		err = gve_adminq_kick_and_wait(adapter);
		if (err)
			return err;

		// Retry.
		tail = ioread32be(&reg_bar0->adminq_event_counter);
		if (((adapter->adminq_prod_cnt + 1) & adapter->adminq_mask) ==
		    (tail & adapter->adminq_mask)) {
			// This should never happen. We just flushed the
			// command queue so there should be enough space.
			return -ENOMEM;
		}
	}

	cmd = &adapter->adminq[adapter->adminq_prod_cnt & adapter->adminq_mask];
	adapter->adminq_prod_cnt++;

	memcpy(cmd, cmd_orig, sizeof(*cmd_orig));
	opcode = be32_to_cpu(READ_ONCE(cmd->opcode));
	if (opcode == GVE_ADMINQ_EXTENDED_COMMAND)
		opcode = be32_to_cpu(cmd->extended_command.inner_opcode);

	switch (opcode) {
	case GVE_ADMINQ_DESCRIBE_DEVICE:
		adapter->adminq_describe_device_cnt++;
		break;
	case GVE_ADMINQ_CONFIGURE_DEVICE_RESOURCES:
		adapter->adminq_cfg_device_resources_cnt++;
		break;
	case GVE_ADMINQ_REGISTER_PAGE_LIST:
		adapter->adminq_register_page_list_cnt++;
		break;
	case GVE_ADMINQ_UNREGISTER_PAGE_LIST:
		adapter->adminq_unregister_page_list_cnt++;
		break;
	case GVE_ADMINQ_CREATE_TX_QUEUE:
		adapter->adminq_create_tx_queue_cnt++;
		break;
	case GVE_ADMINQ_CREATE_RX_QUEUE:
		adapter->adminq_create_rx_queue_cnt++;
		break;
	case GVE_ADMINQ_DESTROY_TX_QUEUE:
		adapter->adminq_destroy_tx_queue_cnt++;
		break;
	case GVE_ADMINQ_DESTROY_RX_QUEUE:
		adapter->adminq_destroy_rx_queue_cnt++;
		break;
	case GVE_ADMINQ_DECONFIGURE_DEVICE_RESOURCES:
		adapter->adminq_dcfg_device_resources_cnt++;
		break;
	case GVE_ADMINQ_SET_DRIVER_PARAMETER:
		adapter->adminq_set_driver_parameter_cnt++;
		break;
	case GVE_ADMINQ_REPORT_STATS:
		adapter->adminq_report_stats_cnt++;
		break;
	case GVE_ADMINQ_REPORT_LINK_SPEED:
		adapter->adminq_report_link_speed_cnt++;
		break;
	case GVE_ADMINQ_REPORT_NIC_TIMESTAMP:
		adapter->adminq_report_nic_timestamp_cnt++;
		break;
	case GVE_ADMINQ_GET_PTYPE_MAP:
		adapter->adminq_get_ptype_map_cnt++;
		break;
	case GVE_ADMINQ_VERIFY_DRIVER_COMPATIBILITY:
		adapter->adminq_verify_driver_compatibility_cnt++;
		break;
	case GVE_ADMINQ_QUERY_FLOW_RULES:
		adapter->adminq_query_flow_rules_cnt++;
		break;
	case GVE_ADMINQ_CONFIGURE_FLOW_RULE:
		adapter->adminq_cfg_flow_rule_cnt++;
		break;
	case GVE_ADMINQ_CONFIGURE_RSS:
		adapter->adminq_cfg_rss_cnt++;
		break;
	case GVE_ADMINQ_QUERY_RSS:
		adapter->adminq_query_rss_cnt++;
		break;
	default:
		dev_err(&adapter->pdev->dev, "unknown AQ command opcode %d\n",
			opcode);
		return -EINVAL;
	}

	return 0;
}

static int gve_adminq_execute_cmd(struct gve_adapter *adapter,
				  union gve_adminq_command *cmd_orig)
{
	struct gve_adminq_registers __iomem *reg_bar0 = adapter->reg_bar0;
	u32 tail, head;
	int err;

	mutex_lock(&adapter->adminq_lock);
	tail = ioread32be(&reg_bar0->adminq_event_counter);
	head = adapter->adminq_prod_cnt;
	if (tail != head) {
		err = -EINVAL;
		goto out;
	}

	err = gve_adminq_issue_cmd(adapter, cmd_orig);
	if (err)
		goto out;

	err = gve_adminq_kick_and_wait(adapter);

out:
	mutex_unlock(&adapter->adminq_lock);
	return err;
}

static int gve_adminq_execute_extended_cmd(struct gve_adapter *adapter,
					   u32 opcode, size_t cmd_size,
					   void *cmd_orig)
{
	union gve_adminq_command cmd;
	dma_addr_t inner_cmd_bus;
	void *inner_cmd;
	int err;

	inner_cmd = dma_alloc_coherent(&adapter->pdev->dev, cmd_size,
				       &inner_cmd_bus, GFP_KERNEL);
	if (!inner_cmd)
		return -ENOMEM;

	memcpy(inner_cmd, cmd_orig, cmd_size);

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_EXTENDED_COMMAND);
	cmd.extended_command = (struct gve_adminq_extended_command){
		.inner_opcode = cpu_to_be32(opcode),
		.inner_length = cpu_to_be32(cmd_size),
		.inner_command_addr = cpu_to_be64(inner_cmd_bus),
	};

	err = gve_adminq_execute_cmd(adapter, &cmd);

	dma_free_coherent(&adapter->pdev->dev, cmd_size, inner_cmd,
			  inner_cmd_bus);
	return err;
}

/* The device specifies that the management vector can either be the first irq
 * or the last irq. ntfy_blk_msix_base_idx indicates the first irq assigned to
 * the ntfy blks. If it is 0 then the management vector is last, if it is 1 then
 * the management vector is first.
 *
 * gve arranges the msix vectors so that the management vector is last.
 */
#define GVE_NTFY_BLK_BASE_MSIX_IDX 0
int gve_adminq_configure_device_resources(struct gve_priv *priv,
					  dma_addr_t counter_array_bus_addr,
					  u32 num_counters,
					  dma_addr_t db_array_bus_addr,
					  u32 num_ntfy_blks)
{
	struct gve_adapter *adapter = priv->adapter;
	union gve_adminq_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_CONFIGURE_DEVICE_RESOURCES);
	cmd.configure_device_resources =
		(struct gve_adminq_configure_device_resources){
			.counter_array = cpu_to_be64(counter_array_bus_addr),
			.num_counters = cpu_to_be32(num_counters),
			.irq_db_addr = cpu_to_be64(db_array_bus_addr),
			.num_irq_dbs = cpu_to_be32(num_ntfy_blks),
			.irq_db_stride =
				cpu_to_be32(sizeof(*priv->irq_db_indices)),
			.ntfy_blk_msix_base_idx =
				cpu_to_be32(GVE_NTFY_BLK_BASE_MSIX_IDX),
			.queue_format = priv->queue_format,
		};

	return gve_adminq_execute_cmd(adapter, &cmd);
}

int gve_adminq_deconfigure_device_resources(struct gve_priv *priv)
{
	struct gve_adapter *adapter = priv->adapter;
	union gve_adminq_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_DECONFIGURE_DEVICE_RESOURCES);

	return gve_adminq_execute_cmd(adapter, &cmd);
}

static int gve_adminq_create_tx_queue(struct gve_priv *priv, u32 queue_index)
{
	struct gve_tx_ring *tx = &priv->tx[queue_index];
	struct gve_adapter *adapter = priv->adapter;
	union gve_adminq_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_CREATE_TX_QUEUE);
	cmd.create_tx_queue = (struct gve_adminq_create_tx_queue){
		.queue_id = cpu_to_be32(queue_index),
		.queue_resources_addr = cpu_to_be64(tx->q_resources_bus),
		.tx_ring_addr = cpu_to_be64(tx->bus),
		.ntfy_id = cpu_to_be32(tx->ntfy_id),
		.tx_ring_size = cpu_to_be16(priv->tx_desc_cnt),
	};

	if (gve_is_gqi(priv)) {
		u32 qpl_id = priv->queue_format == GVE_GQI_RDA_FORMAT ?
				     GVE_RAW_ADDRESSING_QPL_ID :
				     tx->tx_fifo.qpl->id;

		cmd.create_tx_queue.queue_page_list_id = cpu_to_be32(qpl_id);
	} else {
		u32 qpl_id = 0;

		if (priv->queue_format == GVE_DQO_RDA_FORMAT)
			qpl_id = GVE_RAW_ADDRESSING_QPL_ID;
		else
			qpl_id = tx->dqo.qpl->id;
		cmd.create_tx_queue.queue_page_list_id = cpu_to_be32(qpl_id);
		cmd.create_tx_queue.tx_comp_ring_addr =
			cpu_to_be64(tx->complq_bus_dqo);
		cmd.create_tx_queue.tx_comp_ring_size =
			cpu_to_be16(priv->tx_desc_cnt);
	}

	return gve_adminq_issue_cmd(adapter, &cmd);
}

int gve_adminq_create_tx_queues(struct gve_adapter *adapter, u32 start_id,
				u32 num_queues)
{
	struct gve_priv *priv = adapter->priv;
	int err;
	int i;

	mutex_lock(&adapter->adminq_lock);

	for (i = start_id; i < start_id + num_queues; i++) {
		err = gve_adminq_create_tx_queue(priv, i);
		if (err)
			goto out;
	}

	err = gve_adminq_kick_and_wait(adapter);

out:
	mutex_unlock(&adapter->adminq_lock);
	return err;
}

static void gve_adminq_get_create_rx_queue_cmd(struct gve_priv *priv,
					       union gve_adminq_command *cmd,
					       u32 queue_index)
{
	struct gve_rx_ring *rx = &priv->rx[queue_index];

	memset(cmd, 0, sizeof(*cmd));
	cmd->opcode = cpu_to_be32(GVE_ADMINQ_CREATE_RX_QUEUE);
	cmd->create_rx_queue = (struct gve_adminq_create_rx_queue){
		.queue_id = cpu_to_be32(queue_index),
		.ntfy_id = cpu_to_be32(rx->ntfy_id),
		.queue_resources_addr = cpu_to_be64(rx->q_resources_bus),
		.rx_ring_size = cpu_to_be16(priv->rx_desc_cnt),
		.packet_buffer_size = cpu_to_be16(rx->packet_buffer_size),
	};

	if (gve_is_gqi(priv)) {
		u32 qpl_id = priv->queue_format == GVE_GQI_RDA_FORMAT ?
				     GVE_RAW_ADDRESSING_QPL_ID :
				     rx->data.qpl->id;

		cmd->create_rx_queue.rx_desc_ring_addr =
			cpu_to_be64(rx->desc.bus);
		cmd->create_rx_queue.rx_data_ring_addr =
			cpu_to_be64(rx->data.data_bus);
		cmd->create_rx_queue.index = cpu_to_be32(queue_index);
		cmd->create_rx_queue.queue_page_list_id = cpu_to_be32(qpl_id);
	} else {
		u32 qpl_id = 0;

		if (priv->queue_format == GVE_DQO_RDA_FORMAT)
			qpl_id = GVE_RAW_ADDRESSING_QPL_ID;
		else
			qpl_id = rx->dqo.qpl->id;
		cmd->create_rx_queue.queue_page_list_id = cpu_to_be32(qpl_id);
		cmd->create_rx_queue.rx_desc_ring_addr =
			cpu_to_be64(rx->dqo.complq.bus);
		cmd->create_rx_queue.rx_data_ring_addr =
			cpu_to_be64(rx->dqo.bufq.bus);
		cmd->create_rx_queue.rx_buff_ring_size =
			cpu_to_be16(priv->rx_desc_cnt);
		cmd->create_rx_queue.enable_rsc =
			!!(priv->dev->features & NETIF_F_LRO);
		if (priv->header_split_enabled)
			cmd->create_rx_queue.header_buffer_size =
				cpu_to_be16(priv->header_buf_size);
	}
}

static int gve_adminq_create_rx_queue(struct gve_priv *priv, u32 queue_index)
{
	struct gve_adapter *adapter = priv->adapter;
	union gve_adminq_command cmd;

	gve_adminq_get_create_rx_queue_cmd(priv, &cmd, queue_index);
	return gve_adminq_issue_cmd(adapter, &cmd);
}

/* Unlike gve_adminq_create_rx_queue, this actually rings the doorbell */
int gve_adminq_create_single_rx_queue(struct gve_priv *priv, u32 queue_index)
{
	struct gve_adapter *adapter = priv->adapter;
	union gve_adminq_command cmd;

	gve_adminq_get_create_rx_queue_cmd(priv, &cmd, queue_index);
	return gve_adminq_execute_cmd(adapter, &cmd);
}

int gve_adminq_create_rx_queues(struct gve_adapter *adapter, u32 num_queues)
{
	struct gve_priv *priv = adapter->priv;
	int err;
	int i;

	mutex_lock(&adapter->adminq_lock);

	for (i = 0; i < num_queues; i++) {
		err = gve_adminq_create_rx_queue(priv, i);
		if (err)
			goto out;
	}

	err = gve_adminq_kick_and_wait(adapter);

out:
	mutex_unlock(&adapter->adminq_lock);
	return err;
}

static int gve_adminq_destroy_tx_queue(struct gve_priv *priv, u32 queue_index)
{
	struct gve_adapter *adapter = priv->adapter;
	union gve_adminq_command cmd;
	int err;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_DESTROY_TX_QUEUE);
	cmd.destroy_tx_queue = (struct gve_adminq_destroy_tx_queue){
		.queue_id = cpu_to_be32(queue_index),
	};

	err = gve_adminq_issue_cmd(adapter, &cmd);
	if (err)
		return err;

	return 0;
}

int gve_adminq_destroy_tx_queues(struct gve_priv *priv, u32 start_id,
				 u32 num_queues)
{
	struct gve_adapter *adapter = priv->adapter;
	int err;
	int i;

	mutex_lock(&adapter->adminq_lock);

	for (i = start_id; i < start_id + num_queues; i++) {
		err = gve_adminq_destroy_tx_queue(priv, i);
		if (err)
			goto out;
	}

	err = gve_adminq_kick_and_wait(adapter);

out:
	mutex_unlock(&adapter->adminq_lock);
	return err;
}

static void gve_adminq_make_destroy_rx_queue_cmd(union gve_adminq_command *cmd,
						 u32 queue_index)
{
	memset(cmd, 0, sizeof(*cmd));
	cmd->opcode = cpu_to_be32(GVE_ADMINQ_DESTROY_RX_QUEUE);
	cmd->destroy_rx_queue = (struct gve_adminq_destroy_rx_queue){
		.queue_id = cpu_to_be32(queue_index),
	};
}

static int gve_adminq_destroy_rx_queue(struct gve_priv *priv, u32 queue_index)
{
	struct gve_adapter *adapter = priv->adapter;
	union gve_adminq_command cmd;

	gve_adminq_make_destroy_rx_queue_cmd(&cmd, queue_index);
	return gve_adminq_issue_cmd(adapter, &cmd);
}

/* Unlike gve_adminq_destroy_rx_queue, this actually rings the doorbell */
int gve_adminq_destroy_single_rx_queue(struct gve_priv *priv, u32 queue_index)
{
	struct gve_adapter *adapter = priv->adapter;
	union gve_adminq_command cmd;

	gve_adminq_make_destroy_rx_queue_cmd(&cmd, queue_index);
	return gve_adminq_execute_cmd(adapter, &cmd);
}

int gve_adminq_destroy_rx_queues(struct gve_priv *priv, u32 num_queues)
{
	struct gve_adapter *adapter = priv->adapter;
	int err;
	int i;

	mutex_lock(&adapter->adminq_lock);

	for (i = 0; i < num_queues; i++) {
		err = gve_adminq_destroy_rx_queue(priv, i);
		if (err)
			goto out;
	}

	err = gve_adminq_kick_and_wait(adapter);

out:
	mutex_unlock(&adapter->adminq_lock);
	return err;
}

static void gve_enable_supported_features(
	struct gve_adapter *adapter, u32 supported_features_mask,
	const struct gve_device_option_jumbo_frames *dev_op_jumbo_frames,
	const struct gve_device_option_dqo_qpl *dev_op_dqo_qpl,
	const struct gve_device_option_buffer_sizes *dev_op_buffer_sizes,
	const struct gve_device_option_flow_steering *dev_op_flow_steering,
	const struct gve_device_option_rss_config *dev_op_rss_config,
	const struct gve_device_option_nic_timestamp *dev_op_nic_timestamp,
	const struct gve_device_option_modify_ring *dev_op_modify_ring)
{
	struct gve_device_info *device_info = adapter->device_info;

	/* Before control reaches this point, the page-size-capped max MTU from
	 * the gve_device_descriptor field has already been stored in
	 * priv->dev->max_mtu. We overwrite it with the true max MTU below.
	 */
	if (dev_op_jumbo_frames &&
	    (supported_features_mask & GVE_SUP_JUMBO_FRAMES_MASK)) {
		dev_info(&adapter->pdev->dev,
			 "JUMBO FRAMES device option enabled.\n");
		device_info->max_mtu =
			be16_to_cpu(dev_op_jumbo_frames->max_mtu);
	}

	if (dev_op_buffer_sizes &&
	    (supported_features_mask & GVE_SUP_BUFFER_SIZES_MASK)) {
		device_info->max_rx_buffer_size =
			be16_to_cpu(dev_op_buffer_sizes->packet_buffer_size);
		device_info->header_buf_size =
			be16_to_cpu(dev_op_buffer_sizes->header_buffer_size);
		dev_info(
			&adapter->pdev->dev,
			"BUFFER SIZES device option enabled with max_rx_buffer_size of %u, header_buf_size of %u.\n",
			device_info->max_rx_buffer_size,
			device_info->header_buf_size);
	}

	/* Read and store ring size ranges given by device */
	if (dev_op_modify_ring &&
	    (supported_features_mask & GVE_SUP_MODIFY_RING_MASK)) {
		device_info->modify_ring_size_enabled = true;

		/* max ring size for DQO QPL should not be overwritten because of device limit */
		if (device_info->queue_format != GVE_DQO_QPL_FORMAT) {
			device_info->max_rx_ring_size = be16_to_cpu(
				dev_op_modify_ring->max_rx_ring_size);
			device_info->max_tx_ring_size = be16_to_cpu(
				dev_op_modify_ring->max_tx_ring_size);
		}
		if (device_info->default_min_ring_size) {
			/* If device hasn't provided minimums, use default minimums */
			device_info->min_tx_ring_size =
				GVE_DEFAULT_MIN_TX_RING_SIZE;
			device_info->min_rx_ring_size =
				GVE_DEFAULT_MIN_RX_RING_SIZE;
		} else {
			device_info->min_rx_ring_size = be16_to_cpu(
				dev_op_modify_ring->min_rx_ring_size);
			device_info->min_tx_ring_size = be16_to_cpu(
				dev_op_modify_ring->min_tx_ring_size);
		}
	}

	if (dev_op_flow_steering &&
	    (supported_features_mask & GVE_SUP_FLOW_STEERING_MASK)) {
		if (dev_op_flow_steering->max_flow_rules) {
			device_info->max_flow_rules = be32_to_cpu(
				dev_op_flow_steering->max_flow_rules);
			dev_info(
				&adapter->pdev->dev,
				"FLOW STEERING device option enabled with max rule limit of %u.\n",
				device_info->max_flow_rules);
		}
	}

	if (dev_op_rss_config &&
	    (supported_features_mask & GVE_SUP_RSS_CONFIG_MASK)) {
		device_info->rss_key_size =
			be16_to_cpu(dev_op_rss_config->hash_key_size);
		device_info->rss_lut_size =
			be16_to_cpu(dev_op_rss_config->hash_lut_size);
		device_info->cache_rss_config = false;
		dev_dbg(&adapter->pdev->dev,
			"RSS device option enabled with key size of %u, lut size of %u.\n",
			device_info->rss_key_size, device_info->rss_lut_size);
	}

	if (dev_op_nic_timestamp &&
	    (supported_features_mask & GVE_SUP_NIC_TIMESTAMP_MASK)) {
		device_info->nic_timestamp_supported = true;
		device_info->clk_read_type = GVE_DEV_CLK_ADMINQ;
	}
}

static void
gve_process_device_descriptor(struct gve_adapter *adapter,
			      struct gve_device_descriptor *descriptor)
{
	struct gve_device_info *info = adapter->device_info;

	memcpy(info->mac, descriptor->mac, sizeof(descriptor->mac));
	info->max_mtu = be16_to_cpu(descriptor->mtu);
	info->default_rx_ring_size = be16_to_cpu(descriptor->rx_queue_entries);
	info->default_tx_ring_size = be16_to_cpu(descriptor->tx_queue_entries);
	info->default_rx_queues = be16_to_cpu(descriptor->default_num_queues);
	info->default_tx_queues = be16_to_cpu(descriptor->default_num_queues);
	info->tx_pages_per_qpl = be16_to_cpu(descriptor->tx_pages_per_qpl);
}

int gve_adminq_describe_device(struct gve_adapter *adapter)
{
	struct gve_device_option_nic_timestamp *dev_op_nic_timestamp = NULL;
	struct gve_device_option_flow_steering *dev_op_flow_steering = NULL;
	struct gve_device_option_buffer_sizes *dev_op_buffer_sizes = NULL;
	struct gve_device_option_jumbo_frames *dev_op_jumbo_frames = NULL;
	struct gve_device_option_modify_ring *dev_op_modify_ring = NULL;
	struct gve_device_option_rss_config *dev_op_rss_config = NULL;
	struct gve_device_info *device_info = adapter->device_info;
	struct gve_device_option_gqi_rda *dev_op_gqi_rda = NULL;
	struct gve_device_option_gqi_qpl *dev_op_gqi_qpl = NULL;
	struct gve_device_option_dqo_rda *dev_op_dqo_rda = NULL;
	struct gve_device_option_dqo_qpl *dev_op_dqo_qpl = NULL;
	struct gve_device_descriptor *descriptor;
	u32 supported_features_mask = 0;
	union gve_adminq_command cmd;
	dma_addr_t descriptor_bus;
	u16 default_num_queues;
	int err = 0;

	memset(&cmd, 0, sizeof(cmd));
	descriptor = dma_pool_alloc(adapter->adminq_pool, GFP_KERNEL,
				    &descriptor_bus);
	if (!descriptor)
		return -ENOMEM;
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_DESCRIBE_DEVICE);
	cmd.describe_device.device_descriptor_addr =
		cpu_to_be64(descriptor_bus);
	cmd.describe_device.device_descriptor_version =
		cpu_to_be32(GVE_ADMINQ_DEVICE_DESCRIPTOR_VERSION);
	cmd.describe_device.available_length =
		cpu_to_be32(GVE_ADMINQ_BUFFER_SIZE);

	err = gve_adminq_execute_cmd(adapter, &cmd);
	if (err)
		goto free_device_descriptor;

	err = gve_process_device_options(
		adapter, descriptor, &dev_op_gqi_rda, &dev_op_gqi_qpl,
		&dev_op_dqo_rda, &dev_op_jumbo_frames, &dev_op_dqo_qpl,
		&dev_op_buffer_sizes, &dev_op_flow_steering, &dev_op_rss_config,
		&dev_op_nic_timestamp, &dev_op_modify_ring);
	if (err)
		goto free_device_descriptor;

	/* If the GQI_RAW_ADDRESSING option is not enabled and the queue format
	 * is not set to GqiRda, choose the queue format in a priority order:
	 * DqoRda, DqoQpl, GqiRda, GqiQpl. Use GqiQpl as default.
	 */
	if (dev_op_dqo_rda) {
		device_info->queue_format = GVE_DQO_RDA_FORMAT;
		dev_info(&adapter->pdev->dev,
			 "Driver is running with DQO RDA queue format.\n");
		supported_features_mask =
			be32_to_cpu(dev_op_dqo_rda->supported_features_mask);
	} else if (dev_op_dqo_qpl) {
		device_info->queue_format = GVE_DQO_QPL_FORMAT;
		supported_features_mask =
			be32_to_cpu(dev_op_dqo_qpl->supported_features_mask);
	} else if (dev_op_gqi_rda) {
		device_info->queue_format = GVE_GQI_RDA_FORMAT;
		dev_info(&adapter->pdev->dev,
			 "Driver is running with GQI RDA queue format.\n");
		supported_features_mask =
			be32_to_cpu(dev_op_gqi_rda->supported_features_mask);
	} else if (device_info->queue_format == GVE_GQI_RDA_FORMAT) {
		dev_info(&adapter->pdev->dev,
			 "Driver is running with GQI RDA queue format.\n");
	} else {
		device_info->queue_format = GVE_GQI_QPL_FORMAT;
		if (dev_op_gqi_qpl)
			supported_features_mask = be32_to_cpu(
				dev_op_gqi_qpl->supported_features_mask);
		dev_info(&adapter->pdev->dev,
			 "Driver is running with GQI QPL queue format.\n");
	}

	device_info->max_registered_pages =
		be64_to_cpu(descriptor->max_registered_pages);
	device_info->num_event_counters = be16_to_cpu(descriptor->counters);

	default_num_queues = be16_to_cpu(descriptor->default_num_queues);
	device_info->default_tx_queues = default_num_queues;
	device_info->default_rx_queues = default_num_queues;

	gve_process_device_descriptor(adapter, descriptor);

	gve_enable_supported_features(adapter, supported_features_mask,
				      dev_op_jumbo_frames, dev_op_dqo_qpl,
				      dev_op_buffer_sizes, dev_op_flow_steering,
				      dev_op_rss_config, dev_op_nic_timestamp,
				      dev_op_modify_ring);

free_device_descriptor:
	dma_pool_free(adapter->adminq_pool, descriptor, descriptor_bus);
	return err;
}

int gve_adminq_register_page_list(struct gve_priv *priv,
				  struct gve_queue_page_list *qpl)
{
	struct gve_adapter *adapter = priv->adapter;
	struct device *hdev = &priv->pdev->dev;
	u32 num_entries = qpl->num_entries;
	u32 size = num_entries * sizeof(qpl->page_buses[0]);
	union gve_adminq_command cmd;
	dma_addr_t page_list_bus;
	__be64 *page_list;
	int err;
	int i;

	memset(&cmd, 0, sizeof(cmd));
	page_list = dma_alloc_coherent(hdev, size, &page_list_bus, GFP_KERNEL);
	if (!page_list)
		return -ENOMEM;

	for (i = 0; i < num_entries; i++)
		page_list[i] = cpu_to_be64(qpl->page_buses[i]);

	cmd.opcode = cpu_to_be32(GVE_ADMINQ_REGISTER_PAGE_LIST);
	cmd.reg_page_list = (struct gve_adminq_register_page_list){
		.page_list_id = cpu_to_be32(qpl->id),
		.num_pages = cpu_to_be32(num_entries),
		.page_address_list_addr = cpu_to_be64(page_list_bus),
		.page_size = cpu_to_be64(PAGE_SIZE),
	};

	err = gve_adminq_execute_cmd(adapter, &cmd);
	dma_free_coherent(hdev, size, page_list, page_list_bus);
	return err;
}

int gve_adminq_unregister_page_list(struct gve_priv *priv, u32 page_list_id)
{
	struct gve_adapter *adapter = priv->adapter;
	union gve_adminq_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_UNREGISTER_PAGE_LIST);
	cmd.unreg_page_list = (struct gve_adminq_unregister_page_list){
		.page_list_id = cpu_to_be32(page_list_id),
	};

	return gve_adminq_execute_cmd(adapter, &cmd);
}

int gve_adminq_report_stats(struct gve_adapter *adapter, u64 stats_report_len,
			    dma_addr_t stats_report_addr, u64 interval)
{
	union gve_adminq_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_REPORT_STATS);
	cmd.report_stats = (struct gve_adminq_report_stats){
		.stats_report_len = cpu_to_be64(stats_report_len),
		.stats_report_addr = cpu_to_be64(stats_report_addr),
		.interval = cpu_to_be64(interval),
	};

	return gve_adminq_execute_cmd(adapter, &cmd);
}

int gve_adminq_verify_driver_compatibility(struct gve_adapter *adapter)
{
	struct gve_driver_info *driver_info;
	union gve_adminq_command cmd;
	dma_addr_t driver_info_bus;
	int err;

	driver_info = dma_alloc_coherent(&adapter->pdev->dev,
					 sizeof(struct gve_driver_info),
					 &driver_info_bus, GFP_KERNEL);
	if (!driver_info)
		return -ENOMEM;

	*driver_info = (struct gve_driver_info) {
		.os_type = 1, /* Linux */
		.os_version_major = cpu_to_be32(LINUX_VERSION_MAJOR),
		.os_version_minor = cpu_to_be32(LINUX_VERSION_SUBLEVEL),
		.os_version_sub = cpu_to_be32(LINUX_VERSION_PATCHLEVEL),
		.driver_capability_flags = {
			cpu_to_be64(GVE_DRIVER_CAPABILITY_FLAGS1),
			cpu_to_be64(GVE_DRIVER_CAPABILITY_FLAGS2),
			cpu_to_be64(GVE_DRIVER_CAPABILITY_FLAGS3),
			cpu_to_be64(GVE_DRIVER_CAPABILITY_FLAGS4),
		},
	};
	strscpy(driver_info->os_version_str1, utsname()->release,
		sizeof(driver_info->os_version_str1));
	strscpy(driver_info->os_version_str2, utsname()->version,
		sizeof(driver_info->os_version_str2));

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_VERIFY_DRIVER_COMPATIBILITY);
	cmd.verify_driver_compatibility =
		(struct gve_adminq_verify_driver_compatibility){
			.driver_info_len =
				cpu_to_be64(sizeof(struct gve_driver_info)),
			.driver_info_addr = cpu_to_be64(driver_info_bus),
		};

	err = gve_adminq_execute_cmd(adapter, &cmd);

	/* It's ok if the device doesn't support this */
	if (err == -EOPNOTSUPP)
		err = 0;

	dma_free_coherent(&adapter->pdev->dev, sizeof(struct gve_driver_info),
			  driver_info, driver_info_bus);
	return err;
}

int gve_adminq_report_link_speed(struct gve_adapter *adapter)
{
	struct gve_priv *priv = adapter->priv;
	union gve_adminq_command gvnic_cmd;
	dma_addr_t link_speed_region_bus;
	__be64 *link_speed_region;
	int err;

	link_speed_region =
		dma_alloc_coherent(&priv->pdev->dev, sizeof(*link_speed_region),
				   &link_speed_region_bus, GFP_KERNEL);

	if (!link_speed_region)
		return -ENOMEM;

	memset(&gvnic_cmd, 0, sizeof(gvnic_cmd));
	gvnic_cmd.opcode = cpu_to_be32(GVE_ADMINQ_REPORT_LINK_SPEED);
	gvnic_cmd.report_link_speed.link_speed_address =
		cpu_to_be64(link_speed_region_bus);

	err = gve_adminq_execute_cmd(adapter, &gvnic_cmd);

	priv->link_speed = be64_to_cpu(*link_speed_region);
	dma_free_coherent(&priv->pdev->dev, sizeof(*link_speed_region),
			  link_speed_region, link_speed_region_bus);
	return err;
}

int gve_adminq_report_nic_ts(struct gve_priv *priv,
			     dma_addr_t nic_ts_report_addr)
{
	struct gve_adapter *adapter = priv->adapter;
	union gve_adminq_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_REPORT_NIC_TIMESTAMP);
	cmd.report_nic_ts = (struct gve_adminq_report_nic_ts){
		.nic_ts_report_len =
			cpu_to_be64(sizeof(struct gve_nic_ts_report)),
		.nic_ts_report_addr = cpu_to_be64(nic_ts_report_addr),
	};

	return gve_adminq_execute_cmd(adapter, &cmd);
}

int gve_adminq_get_ptype_map_dqo(struct gve_adapter *adapter)
{
	struct gve_priv *priv = adapter->priv;
	struct gve_ptype_map *ptype_map;
	union gve_adminq_command cmd;
	dma_addr_t ptype_map_bus;
	int err = 0;
	int i;

	memset(&cmd, 0, sizeof(cmd));
	ptype_map = dma_alloc_coherent(&priv->pdev->dev, sizeof(*ptype_map),
				       &ptype_map_bus, GFP_KERNEL);
	if (!ptype_map)
		return -ENOMEM;

	cmd.opcode = cpu_to_be32(GVE_ADMINQ_GET_PTYPE_MAP);
	cmd.get_ptype_map = (struct gve_adminq_get_ptype_map){
		.ptype_map_len = cpu_to_be64(sizeof(*ptype_map)),
		.ptype_map_addr = cpu_to_be64(ptype_map_bus),
	};

	err = gve_adminq_execute_cmd(adapter, &cmd);
	if (err)
		goto err;

	/* Populate ptype_lut. */
	for (i = 0; i < GVE_NUM_PTYPES; i++) {
		priv->ptype_lut_dqo->ptypes[i].l3_type =
			ptype_map->ptypes[i].l3_type;
		priv->ptype_lut_dqo->ptypes[i].l4_type =
			ptype_map->ptypes[i].l4_type;
	}
err:
	dma_free_coherent(&priv->pdev->dev, sizeof(*ptype_map), ptype_map,
			  ptype_map_bus);
	return err;
}

static int gve_adminq_configure_flow_rule(
	struct gve_priv *priv,
	struct gve_adminq_configure_flow_rule *flow_rule_cmd)
{
	struct gve_adapter *adapter = priv->adapter;
	int err = gve_adminq_execute_extended_cmd(
		adapter, GVE_ADMINQ_CONFIGURE_FLOW_RULE,
		sizeof(struct gve_adminq_configure_flow_rule), flow_rule_cmd);

	if (err == -ETIME) {
		dev_err(&priv->pdev->dev,
			"Timeout to configure the flow rule, trigger reset");
		gve_reset(priv);
	} else if (!err) {
		priv->flow_rules_cache.rules_cache_synced = false;
	}

	return err;
}

int gve_adminq_add_flow_rule(struct gve_adapter *adapter,
			     struct gve_flow_rule *rule, u32 loc)
{
	struct gve_adminq_configure_flow_rule flow_rule_cmd = {
		.opcode = cpu_to_be16(GVE_FLOW_RULE_CFG_ADD),
		.location = cpu_to_be32(loc),
	};

	flow_rule_cmd.rule.flow_type = cpu_to_be16(rule->flow_type);
	flow_rule_cmd.rule.action = cpu_to_be16(rule->action);
	flow_rule_cmd.rule.key = rule->key;
	flow_rule_cmd.rule.mask = rule->mask;

	return gve_adminq_configure_flow_rule(adapter->priv, &flow_rule_cmd);
}

int gve_adminq_del_flow_rule(struct gve_adapter *adapter, u32 loc)
{
	struct gve_adminq_configure_flow_rule flow_rule_cmd = {
		.opcode = cpu_to_be16(GVE_FLOW_RULE_CFG_DEL),
		.location = cpu_to_be32(loc),
	};

	return gve_adminq_configure_flow_rule(adapter->priv, &flow_rule_cmd);
}

int gve_adminq_reset_flow_rules(struct gve_adapter *adapter)
{
	struct gve_adminq_configure_flow_rule flow_rule_cmd = {
		.opcode = cpu_to_be16(GVE_FLOW_RULE_CFG_RESET),
	};

	return gve_adminq_configure_flow_rule(adapter->priv, &flow_rule_cmd);
}

int gve_adminq_configure_rss(struct gve_adapter *adapter,
			     struct ethtool_rxfh_param *rxfh)
{
	dma_addr_t lut_bus = 0, key_bus = 0;
	union gve_adminq_command cmd;
	__be32 *lut = NULL;
	u8 hash_alg = 0;
	u8 *key = NULL;
	int err = 0;
	u16 i;

	switch (rxfh->hfunc) {
	case ETH_RSS_HASH_NO_CHANGE:
		fallthrough;
	case ETH_RSS_HASH_TOP:
		hash_alg = ETH_RSS_HASH_TOP;
		break;
	default:
		return -EOPNOTSUPP;
	}

	if (rxfh->indir) {
		lut = dma_alloc_coherent(&adapter->pdev->dev,
					 rxfh->indir_size * sizeof(*lut),
					 &lut_bus, GFP_KERNEL);
		if (!lut)
			return -ENOMEM;

		for (i = 0; i < rxfh->indir_size; i++)
			lut[i] = cpu_to_be32(rxfh->indir[i]);
	}

	if (rxfh->key) {
		key = dma_alloc_coherent(&adapter->pdev->dev, rxfh->key_size,
					 &key_bus, GFP_KERNEL);
		if (!key) {
			err = -ENOMEM;
			goto out;
		}

		memcpy(key, rxfh->key, rxfh->key_size);
	}

	/* Zero-valued fields in the cmd.configure_rss instruct the device to
	 * not update those fields.
	 */
	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_CONFIGURE_RSS);
	cmd.configure_rss = (struct gve_adminq_configure_rss){
		.hash_types = cpu_to_be16(
			BIT(GVE_RSS_HASH_TCPV4) | BIT(GVE_RSS_HASH_UDPV4) |
			BIT(GVE_RSS_HASH_TCPV6) | BIT(GVE_RSS_HASH_UDPV6)),
		.hash_alg = hash_alg,
		.hash_key_size = cpu_to_be16((key_bus) ? rxfh->key_size : 0),
		.hash_lut_size = cpu_to_be16((lut_bus) ? rxfh->indir_size : 0),
		.hash_key_addr = cpu_to_be64(key_bus),
		.hash_lut_addr = cpu_to_be64(lut_bus),
	};

	err = gve_adminq_execute_cmd(adapter, &cmd);

out:
	if (lut)
		dma_free_coherent(&adapter->pdev->dev,
				  rxfh->indir_size * sizeof(*lut), lut,
				  lut_bus);
	if (key)
		dma_free_coherent(&adapter->pdev->dev, rxfh->key_size, key,
				  key_bus);
	return err;
}

static void gve_adminq_cache_flow_rules(struct gve_flow_rules_cache *cache,
					struct gve_flow_rule_config *rules,
					u32 num_rules)
{
	int i;

	for (i = 0; i < num_rules; i++) {
		struct gve_flow_rule *rule = &cache->rules_cache[i].flow_rule;

		cache->rules_cache[i].location = be32_to_cpu(rules[i].location);

		rule->flow_type = be16_to_cpu(rules[i].flow_rule.flow_type);
		rule->action = be16_to_cpu(rules[i].flow_rule.action);

		/* Key and mask will always be in network byte order. */
		rule->key = rules[i].flow_rule.key;
		rule->mask = rules[i].flow_rule.mask;
	}

	cache->rules_cache_num = num_rules;
}

static void gve_adminq_cache_flow_rule_ids(struct gve_flow_rules_cache *cache,
					   __be32 *rule_ids, u32 num_rule_ids)
{
	int i;

	for (i = 0; i < num_rule_ids; i++)
		cache->rule_ids_cache[i] = be32_to_cpu(rule_ids[i]);

	cache->rule_ids_cache_num = num_rule_ids;
}

/* In the dma memory that the driver allocated for the device to query the flow rules, the device
 * will first write it with a struct of gve_query_flow_rules_descriptor. Next to it, the device
 * will write an array of rules or rule ids with the count that specified in the descriptor.
 * For GVE_FLOW_RULE_QUERY_STATS, the device will only write the descriptor.
 */
static int gve_adminq_process_flow_rules_query(
	struct gve_priv *priv, u16 query_opcode,
	struct gve_query_flow_rules_descriptor *descriptor)
{
	struct gve_flow_rules_cache *flow_rules_cache = &priv->flow_rules_cache;
	u32 num_queried_rules, total_memory_len, rule_info_len;
	void *rule_info;

	total_memory_len = be32_to_cpu(descriptor->total_length);
	num_queried_rules = be32_to_cpu(descriptor->num_queried_rules);
	rule_info = (void *)(descriptor + 1);

	switch (query_opcode) {
	case GVE_FLOW_RULE_QUERY_RULES:
		rule_info_len = num_queried_rules *
				sizeof(*flow_rules_cache->rules_cache);
		if (sizeof(*descriptor) + rule_info_len != total_memory_len) {
			dev_err(&priv->dev->dev,
				"flow rules query is out of memory.\n");
			return -ENOMEM;
		}

		gve_adminq_cache_flow_rules(
			flow_rules_cache,
			(struct gve_flow_rule_config *)rule_info,
			num_queried_rules);
		break;
	case GVE_FLOW_RULE_QUERY_IDS:
		rule_info_len = num_queried_rules *
				sizeof(*flow_rules_cache->rule_ids_cache);
		if (sizeof(*descriptor) + rule_info_len != total_memory_len) {
			dev_err(&priv->dev->dev,
				"flow rule ids query is out of memory.\n");
			return -ENOMEM;
		}

		gve_adminq_cache_flow_rule_ids(flow_rules_cache,
					       (__be32 *)rule_info,
					       num_queried_rules);
		break;
	case GVE_FLOW_RULE_QUERY_STATS:
		priv->num_flow_rules = be32_to_cpu(descriptor->num_flow_rules);
		priv->max_flow_rules = be32_to_cpu(descriptor->max_flow_rules);
		return 0;
	default:
		return -EINVAL;
	}

	return 0;
}

int gve_adminq_query_flow_rules(struct gve_adapter *adapter, u16 query_opcode,
				u32 starting_loc)
{
	struct gve_query_flow_rules_descriptor *descriptor;
	struct gve_priv *priv = adapter->priv;
	union gve_adminq_command cmd;
	dma_addr_t descriptor_bus;
	int err = 0;

	memset(&cmd, 0, sizeof(cmd));
	descriptor = dma_pool_alloc(adapter->adminq_pool, GFP_KERNEL,
				    &descriptor_bus);
	if (!descriptor)
		return -ENOMEM;

	cmd.opcode = cpu_to_be32(GVE_ADMINQ_QUERY_FLOW_RULES);
	cmd.query_flow_rules = (struct gve_adminq_query_flow_rules){
		.opcode = cpu_to_be16(query_opcode),
		.starting_rule_id = cpu_to_be32(starting_loc),
		.available_length = cpu_to_be64(GVE_ADMINQ_BUFFER_SIZE),
		.rule_descriptor_addr = cpu_to_be64(descriptor_bus),
	};
	err = gve_adminq_execute_cmd(adapter, &cmd);
	if (err)
		goto out;

	err = gve_adminq_process_flow_rules_query(priv, query_opcode,
						  descriptor);

out:
	dma_pool_free(adapter->adminq_pool, descriptor, descriptor_bus);
	return err;
}

static int
gve_adminq_process_rss_query(struct gve_priv *priv,
			     struct gve_query_rss_descriptor *descriptor,
			     struct ethtool_rxfh_param *rxfh)
{
	u32 total_memory_length;
	u16 hash_lut_length;
	void *rss_info_addr;
	__be32 *lut;
	u16 i;

	total_memory_length = be32_to_cpu(descriptor->total_length);
	hash_lut_length = priv->rss_lut_size * sizeof(*rxfh->indir);

	if (sizeof(*descriptor) + priv->rss_key_size + hash_lut_length !=
	    total_memory_length) {
		dev_err(&priv->dev->dev,
			"rss query desc from device has invalid length parameter.\n");
		return -EINVAL;
	}

	rxfh->hfunc = descriptor->hash_alg;

	rss_info_addr = (void *)(descriptor + 1);
	if (rxfh->key) {
		rxfh->key_size = priv->rss_key_size;
		memcpy(rxfh->key, rss_info_addr, priv->rss_key_size);
	}

	rss_info_addr += priv->rss_key_size;
	lut = (__be32 *)rss_info_addr;
	if (rxfh->indir) {
		rxfh->indir_size = priv->rss_lut_size;
		for (i = 0; i < priv->rss_lut_size; i++)
			rxfh->indir[i] = be32_to_cpu(lut[i]);
	}

	return 0;
}

int gve_adminq_query_rss_config(struct gve_adapter *adapter,
				struct ethtool_rxfh_param *rxfh)
{
	struct gve_query_rss_descriptor *descriptor;
	struct gve_priv *priv = adapter->priv;
	union gve_adminq_command cmd;
	dma_addr_t descriptor_bus;
	int err = 0;

	descriptor = dma_pool_alloc(adapter->adminq_pool, GFP_KERNEL,
				    &descriptor_bus);
	if (!descriptor)
		return -ENOMEM;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = cpu_to_be32(GVE_ADMINQ_QUERY_RSS);
	cmd.query_rss = (struct gve_adminq_query_rss){
		.available_length = cpu_to_be64(GVE_ADMINQ_BUFFER_SIZE),
		.rss_descriptor_addr = cpu_to_be64(descriptor_bus),
	};
	err = gve_adminq_execute_cmd(adapter, &cmd);
	if (err)
		goto out;

	err = gve_adminq_process_rss_query(priv, descriptor, rxfh);

out:
	dma_pool_free(adapter->adminq_pool, descriptor, descriptor_bus);
	return err;
}

int gve_adminq_map_db_bar(struct gve_adapter *adapter)
{
	struct gve_priv *priv = adapter->priv;
	struct pci_dev *pdev = adapter->pdev;
	void __iomem *db_bar;

	db_bar = pci_iomap(pdev, GVE_DOORBELL_BAR, 0);
	if (!db_bar) {
		dev_err(&pdev->dev, "Failed to map doorbell bar!\n");
		return -ENOMEM;
	}
	priv->db_adminq_bar2 = db_bar;
	return 0;
}

void gve_adminq_unmap_db_bar(struct gve_adapter *adapter)
{
	struct gve_priv *priv = adapter->priv;
	struct pci_dev *pdev = adapter->pdev;

	pci_iounmap(pdev, priv->db_adminq_bar2);
}

int gve_adminq_set_num_ntfy_blks(struct gve_adapter *adapter)
{
	struct gve_priv *priv = adapter->priv;
	int num_ntfy;
	int err = 0;

	num_ntfy = pci_msix_vec_count(priv->pdev);
	if (num_ntfy <= 0) {
		dev_err(&priv->pdev->dev,
			"could not count MSI-x vectors: err=%d\n", num_ntfy);
		err = -EINVAL;
		goto err;
	} else if (num_ntfy < GVE_MIN_MSIX) {
		dev_err(&priv->pdev->dev,
			"gve needs at least %d MSI-x vectors, but only has %d\n",
			GVE_MIN_MSIX, num_ntfy);
		err = -EINVAL;
		goto err;
	}

	/* gvnic has one Notification Block per MSI-x vector, except for the
	 * management vector
	 */
	priv->num_ntfy_blks = (num_ntfy - 1) & ~0x1;
	priv->mgmt_msix_idx = priv->num_ntfy_blks;

err:
	return err;
}

void gve_adminq_set_num_queues(struct gve_adapter *adapter)
{
	struct gve_priv *priv = adapter->priv;

	priv->tx_cfg.max_queues =
		min_t(int, priv->tx_cfg.max_queues, priv->num_ntfy_blks / 2);
	priv->rx_cfg.max_queues =
		min_t(int, priv->rx_cfg.max_queues, priv->num_ntfy_blks / 2);

	priv->tx_cfg.num_queues = priv->tx_cfg.max_queues;
	priv->rx_cfg.num_queues = priv->rx_cfg.max_queues;
	if (priv->default_num_queues > 0) {
		priv->tx_cfg.num_queues = min_t(int, priv->default_num_queues,
						priv->tx_cfg.num_queues);
		priv->rx_cfg.num_queues = min_t(int, priv->default_num_queues,
						priv->rx_cfg.num_queues);
	}
	//TODO
	//priv->default_num_queues = be16_to_cpu(device_info->default_num_queues);
}

void gve_adminq_get_max_queues(struct gve_adapter *adapter, int *max_tx_queues,
			       int *max_rx_queues)
{
	struct gve_adminq_registers __iomem *reg_bar = adapter->reg_bar0;

	*max_tx_queues = ioread32be(&reg_bar->max_tx_queues);
	*max_rx_queues = ioread32be(&reg_bar->max_rx_queues);
}

int gve_adminq_request_db_info(struct gve_adapter *adapter)
{
	struct gve_priv *priv = adapter->priv;
	int err;
	int i;

	/* Alloc dma addrs needed for shm regions */
	err = gve_alloc_counter_array(priv);
	if (err) {
		dev_err(&priv->pdev->dev, "Failed to alloc db counter array.");
		return err;
	}

	priv->irq_db_indices = dma_alloc_coherent(
		&priv->pdev->dev,
		priv->num_ntfy_blks * sizeof(*priv->irq_db_indices),
		&priv->irq_db_indices_bus, GFP_KERNEL);
	if (!priv->irq_db_indices) {
		err = -ENOMEM;
		goto abort_with_counter_array;
	}

	err = gve_adminq_configure_device_resources(
		priv, priv->counter_array_bus, priv->num_event_counters,
		priv->irq_db_indices_bus, priv->num_ntfy_blks);
	if (unlikely(err)) {
		dev_err(&priv->pdev->dev,
			"could not setup device_resources: err=%d\n", err);
		err = -ENXIO;
		goto abort_with_irq_db_indices;
	}

	for (i = 0; i < priv->num_ntfy_blks; i++)
		priv->ntfy_blocks[i].aq_irq_db_index =
			&priv->irq_db_indices[i].index;

	return 0;
abort_with_irq_db_indices:
	dma_free_coherent(&priv->pdev->dev,
			  priv->num_ntfy_blks * sizeof(*priv->irq_db_indices),
			  priv->irq_db_indices, priv->irq_db_indices_bus);
	priv->irq_db_indices = NULL;
abort_with_counter_array:
	gve_free_counter_array(priv);
	return err;
}

void gve_adminq_free_db_resources(struct gve_adapter *adapter)
{
	struct gve_priv *priv = adapter->priv;
	int err;

	/* Log error in deconfigure device, but don't fail. This is only ever
	 * called as a reset is about to be triggered, so it would be redundant
	 * ot trigger a reset. */
	err = gve_adminq_deconfigure_device_resources(priv);
	if (err)
		dev_err(&priv->pdev->dev,
			"Could not deconfigure device resources: err=%d\n",
			err);

	dma_free_coherent(&priv->pdev->dev,
			  priv->num_ntfy_blks * sizeof(*priv->irq_db_indices),
			  priv->irq_db_indices, priv->irq_db_indices_bus);
	priv->irq_db_indices = NULL;
	gve_free_counter_array(priv);
}

static irqreturn_t gve_mgmnt_intr(int irq, void *arg)
{
	struct gve_priv *priv = arg;

	queue_work(priv->gve_wq, &priv->service_task);
	return IRQ_HANDLED;
}

int gve_adminq_setup_mgmt_irq(struct gve_adapter *adapter)
{
	struct gve_priv *priv = adapter->priv;
	int err;

	snprintf(priv->mgmt_msix_name, sizeof(priv->mgmt_msix_name),
		 "gve-mgmnt@pci:%s", pci_name(priv->pdev));
	err = request_irq(priv->msix_vectors[priv->mgmt_msix_idx].vector,
			  gve_mgmnt_intr, 0, priv->mgmt_msix_name, priv);
	if (err) {
		dev_err(&priv->pdev->dev,
			"Did not receive management vector.\n");
		return err;
	}

	return 0;
}

void gve_adminq_teardown_mgmt_irq(struct gve_adapter *adapter)
{
	struct gve_priv *priv = adapter->priv;

	free_irq(priv->msix_vectors[priv->mgmt_msix_idx].vector, adapter->priv);
}

int gve_adminq_report_link_status(struct gve_adapter *adapter)
{
	struct gve_priv *priv = adapter->priv;
	u32 status;

	status = ioread32be(&priv->reg_bar0->device_status);
	priv->link_up = !!(GVE_DEVICE_STATUS_LINK_STATUS_MASK & status);
	return 0;
}

int gve_adminq_create_queues(struct gve_adapter *adapter)
{
	int num_tx_queues = gve_num_tx_queues(adapter->priv);
	struct gve_priv *priv = adapter->priv;
	int err;

	err = gve_adminq_create_tx_queues(adapter, 0, num_tx_queues);
	if (err) {
		netif_err(priv, drv, priv->dev,
			  "failed to create %d tx queues\n", num_tx_queues);
		/* This failure will trigger a reset - no need to clean
		 * up
		 */
		return err;
	}
	netif_dbg(priv, drv, priv->dev, "created %d tx queues\n",
		  num_tx_queues);

	err = gve_adminq_create_rx_queues(adapter, priv->rx_cfg.num_queues);
	if (err) {
		netif_err(priv, drv, priv->dev,
			  "failed to create %d rx queues\n",
			  priv->rx_cfg.num_queues);
		/* This failure will trigger a reset - no need to clean
		 * up
		 */
		return err;
	}
	netif_dbg(priv, drv, priv->dev, "created %d rx queues\n",
		  priv->rx_cfg.num_queues);

	return err;
}

void gve_adminq_write_q_doorbell(struct gve_adapter *adapter,
				 const struct gve_queue_resources *q_resources,
				 u32 val)
{
	u64 index = be32_to_cpu(q_resources->db_index);

	iowrite32(val, &adapter->priv->db_adminq_bar2[index]);
}

void gve_adminq_write_irq_doorbell_dqo(struct gve_adapter *adapter,
				       const struct gve_notify_block *block,
				       u32 val)
{
	u32 index;

	index = be32_to_cpu(*block->aq_irq_db_index);
	iowrite32(val, &adapter->priv->db_adminq_bar2[index]);
}
