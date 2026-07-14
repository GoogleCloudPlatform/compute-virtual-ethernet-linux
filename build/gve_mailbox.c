// SPDX-License-Identifier: GPL-2.0 OR MIT
/* Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2025 Google LLC
 */

#include "gve_linux_version.h"
#include <linux/etherdevice.h>
#include "gve.h"
#include "gve_adminq.h"
#include "gve_mailbox.h"
#include "gve_dqo.h"

static const char *gve_mbx_opcode_to_string(enum gve_mbx_opcode opcode)
{
	switch (opcode) {
	case GVE_MBX_NEGOTIATE_CAPABILITIES: return "GVE_MBX_NEGOTIATE_CAPABILITIES";
	case GVE_MBX_EVENT: return "GVE_MBX_EVENT";
	case GVE_MBX_GET_INFO_FLOW_STEERING: return "GVE_MBX_GET_INFO_FLOW_STEERING";
	case GVE_MBX_GET_INFO_NIC_TSTAMP_REG: return "GVE_MBX_GET_INFO_NIC_TSTAMP_REG";
	case GVE_MBX_GET_INTERRUPT_DBS: return "GVE_MBX_GET_INTERRUPT_DBS";
	case GVE_MBX_GET_PTYPE_MAP: return "GVE_MBX_GET_PTYPE_MAP";
	case GVE_MBX_REPORT_LINK_STATUS: return "GVE_MBX_REPORT_LINK_STATUS";
	case GVE_MBX_CREATE_TX_QUEUES: return "GVE_MBX_CREATE_TX_QUEUES";
	case GVE_MBX_CREATE_RX_QUEUES: return "GVE_MBX_CREATE_RX_QUEUES";
	case GVE_MBX_ENABLE_TX_QUEUES: return "GVE_MBX_ENABLE_TX_QUEUES";
	case GVE_MBX_ENABLE_RX_QUEUES: return "GVE_MBX_ENABLE_RX_QUEUES";
	case GVE_MBX_DESTROY_TX_QUEUES: return "GVE_MBX_DESTROY_TX_QUEUES";
	case GVE_MBX_DESTROY_RX_QUEUES: return "GVE_MBX_DESTROY_RX_QUEUES";
	case GVE_MBX_QUERY_RSS: return "GVE_MBX_QUERY_RSS";
	case GVE_MBX_CONFIGURE_RSS: return "GVE_MBX_CONFIGURE_RSS";
	case GVE_MBX_QUERY_FLOW_RULE_STATS: return "GVE_MBX_QUERY_FLOW_RULE_STATS";
	case GVE_MBX_QUERY_FLOW_RULE_IDS: return "GVE_MBX_QUERY_FLOW_RULE_IDS";
	case GVE_MBX_QUERY_FLOW_RULES: return "GVE_MBX_QUERY_FLOW_RULES";
	case GVE_MBX_ADD_FLOW_RULE: return "GVE_MBX_ADD_FLOW_RULE";
	case GVE_MBX_DEL_FLOW_RULE: return "GVE_MBX_DEL_FLOW_RULE";
	case GVE_MBX_RESET_FLOW_RULES: return "GVE_MBX_RESET_FLOW_RULES";
	default: return "[invalid opcode]";
	}
}

static const char *gve_mbx_event_id_to_string(enum gve_mbx_event_id event_id)
{
	switch (event_id) {
	case GVE_MBX_EVENT_LINK_STATUS_CHANGE: return "GVE_MBX_EVENT_LINK_STATUS_CHANGE";
	default: return "[invalid event_id]";
	}
}

static bool gve_mbx_should_log(enum gve_mbx_opcode opcode)
{
	switch (opcode) {
	case GVE_MBX_NEGOTIATE_CAPABILITIES:
	case GVE_MBX_EVENT:
	case GVE_MBX_GET_INFO_FLOW_STEERING:
	case GVE_MBX_GET_INFO_NIC_TSTAMP_REG:
	case GVE_MBX_GET_INTERRUPT_DBS:
	case GVE_MBX_GET_PTYPE_MAP:
	case GVE_MBX_REPORT_LINK_STATUS:
	case GVE_MBX_CREATE_TX_QUEUES:
	case GVE_MBX_CREATE_RX_QUEUES:
	case GVE_MBX_ENABLE_TX_QUEUES:
	case GVE_MBX_ENABLE_RX_QUEUES:
	case GVE_MBX_DESTROY_TX_QUEUES:
	case GVE_MBX_DESTROY_RX_QUEUES:
	case GVE_MBX_QUERY_RSS:
	case GVE_MBX_CONFIGURE_RSS:
	case GVE_MBX_RESET_FLOW_RULES:
		return true;
	default:
		return false;
	}
}

static void gve_write_mbx_irq_db(struct gve_adapter *adapter, u32 val)
{
	iowrite32(val, (u8*)adapter->reg_bar0 + adapter->mbx_irq_db_offset);
}


void gve_mbx_task(struct work_struct *work)
{
	struct gve_adapter *adapter;
	int err = 0;

	adapter = container_of(work, struct gve_adapter, gve_mbx_task.work);

	do {
		err = gve_receive_mbx_msg(adapter);
		if (err == -EIO) {
			dev_err(&adapter->pdev->dev, "Mailbox queue not set up.");
			break;
		}
	} while (err != -EAGAIN);

	if (gve_get_mailbox_interrupt_ok(adapter))
		gve_write_mbx_irq_db(adapter, GVE_ITR_ENABLE_BIT_DQO);

	else
		queue_delayed_work(adapter->gve_mbx_wq, &adapter->gve_mbx_task,
				msecs_to_jiffies(300));
}

void gve_mbx_lsc_event_task(struct work_struct *work)
{
	struct gve_adapter *adapter;
	int err;

	adapter = container_of(work, struct gve_adapter, gve_mbx_lsc_event_task);
	err = gve_mbx_report_link_status(adapter);
	if (err)
		dev_warn_ratelimited(&adapter->pdev->dev,
				     "Failed to get link status report");

	gve_handle_link_status(adapter->priv);
}

static void gve_mbx_reg_init(struct gve_mbx_queue *mbx_q, void __iomem *bar0)
{
	if (mbx_q->q_type == GVE_GVE_MBX_Q_TYPE_RX) {
		mbx_q->reg = (struct gve_mbx_registers __iomem *)(bar0 + GVE_MBX_RX_BASE);

		mbx_q->len_mask = GVE_MBX_RX_LEN_M;
		mbx_q->len_ena_mask = GVE_MBX_RX_ENABLE_M;
		mbx_q->head_mask = GVE_MBX_RX_HEAD_M;

		iowrite32(mbx_q->ring_size - 1, &mbx_q->reg->queue_tail);
	} else {
		mbx_q->reg = (struct gve_mbx_registers __iomem *)(bar0 + GVE_MBX_TX_BASE);

		mbx_q->len_mask = GVE_MBX_TX_LEN_M;
		mbx_q->len_ena_mask = GVE_MBX_TX_ENABLE_M;
		mbx_q->head_mask = GVE_MBX_TX_HEAD_M;

		iowrite32(0, &mbx_q->reg->queue_tail);
	}

	iowrite32(0, &mbx_q->reg->queue_head);
	iowrite32(lower_32_bits(mbx_q->desc_ring.pa),
		  &mbx_q->reg->base_addr_low);
	iowrite32(upper_32_bits(mbx_q->desc_ring.pa),
		  &mbx_q->reg->base_addr_high);
	iowrite32((mbx_q->ring_size | mbx_q->len_ena_mask),
		  &mbx_q->reg->queue_len);
}

static void gve_clean_send_mbx(struct gve_adapter *adapter)
{
	struct gve_mbx_queue *mbx_tx = adapter->mbx_tx;
	struct gve_dma_mem *clean_msg;
	struct gve_mbx_desc *desc;
	u16 num_to_clean;
	u16 ntc;
	int i;

	/* Attempt to clean the entire queue */
	num_to_clean = mbx_tx->ring_size;
	ntc = mbx_tx->next_to_clean;

	for (i = 0; i < num_to_clean; i++) {
		/* should clean from ntc */
		desc = GVE_MBX_DESC(mbx_tx, ntc);

		/* check if desc is marked as done */
		if (!(le16_to_cpu(desc->flags) & GVE_MBX_FLAG_DD))
			break;

		clean_msg = adapter->mbx_tx_bufs[ntc];
		if (clean_msg) {
			dma_free_coherent(&adapter->pdev->dev, clean_msg->size,
					  clean_msg->va, clean_msg->pa);
			kfree(clean_msg);
		}

		adapter->mbx_tx_bufs[ntc] = NULL;
		memset(desc, 0, sizeof(*desc));

		ntc++;
		if (ntc == mbx_tx->ring_size)
			ntc = 0;
	}

	mbx_tx->next_to_clean = ntc;
}

static int gve_mbx_get_free_send_idx(struct gve_mbx_msg_queue *mbx_msg_queue,
				     u16 *index)
{
	int idx;

	idx = find_first_zero_bit(mbx_msg_queue->msg_queue_map,
				      mbx_msg_queue->size);

	if (idx >= mbx_msg_queue->size)
		return -EBUSY;

	*index = idx;
	return 0;
}

static bool gve_mbx_in_reset(struct gve_adapter *adapter)
{
	struct gve_mbx_queue *mbx_rx = adapter->mbx_rx;

	if (!mbx_rx)
		return true;

	return !(ioread32(&mbx_rx->reg->queue_len) & GVE_MBX_RX_LEN_M);
}

int gve_send_mbx_msg_wait(struct gve_adapter *adapter, u32 opcode, u16 msg_size,
			  u8 *msg)
{
	struct gve_mbx_msg_queue *mbx_msg_queue = adapter->mbx_msg_queue;
	struct gve_mbx_msg *mbx_msg;
	u16 cookie, index = 0;
	int err;

	if (gve_mbx_in_reset(adapter)) {
		dev_err_ratelimited(&adapter->pdev->dev,
				    "Mailbox reset detected, cannot send mbx msg\n");

		/* if reset is already in progress, simply return */
		if (gve_get_reset_in_progress(adapter))
			return -EIO;

		gve_schedule_reset(adapter->priv);
		return -EIO;
	}

	if (!gve_get_mbx_queue_ok(adapter))
		return -EIO;

	mbx_msg = kzalloc(sizeof(*mbx_msg), GFP_KERNEL);
	if (!mbx_msg)
		return -ENOMEM;

	init_completion(&mbx_msg->work);

	spin_lock(&mbx_msg_queue->mbx_msg_q_lock);

	if (gve_mbx_get_free_send_idx(mbx_msg_queue, &index)) {
		err = -EBUSY;
		goto err_unlock;
	}
	BUG_ON(index >= mbx_msg_queue->size);

	/* create sw cookie */
	mbx_msg_queue->counter++;
	cookie = BIT(15) |
		 ((mbx_msg_queue->counter & 0x1FF) << GVE_MBX_COOKIE_INDEX_BITS) |
		 (index & GVE_MBX_COOKIE_INDEX_MASK);


	mbx_msg->sw_cookie = cookie;
	mbx_msg->opcode = opcode;
	set_bit(index, mbx_msg_queue->msg_queue_map);
	adapter->mbx_msgs[index] = mbx_msg;

	if (gve_mbx_should_log(opcode))
		dev_info(
			&adapter->pdev->dev,
			"Sending MBX msg: opcode=%s 0x%x, index=%u, salt=%u, cookie=0x%04x\n",
			gve_mbx_opcode_to_string(opcode), opcode, index,
			(mbx_msg_queue->counter & 0x1FF), cookie);

	err = gve_send_mbx_msg(adapter, opcode, msg_size, msg, cookie);
	if (err) {
		goto err_unmap_cookie;
	}

	spin_unlock(&mbx_msg_queue->mbx_msg_q_lock);

	/* wait for timeout */
	if (!wait_for_completion_timeout(&mbx_msg->work,
				    msecs_to_jiffies(GVE_MBX_MSG_TIMEOUT))) {
		err = -ETIMEDOUT;
		goto err_lock_free_cookie;
	}

	if (mbx_msg->status == GVE_MBX_STATUS_PASSED)
                err = 0;
        else
                err = -EBADMSG;

err_lock_free_cookie:
	spin_lock(&adapter->mbx_msg_queue->mbx_msg_q_lock);

err_unmap_cookie:
	clear_bit(index, mbx_msg_queue->msg_queue_map);
	adapter->mbx_msgs[index] = NULL;

err_unlock:
	spin_unlock(&adapter->mbx_msg_queue->mbx_msg_q_lock);
	kfree(mbx_msg);
	// printk("%s: returning with err = %d\n", __func__, err);
	return err;
}

int gve_send_mbx_msg(struct gve_adapter *adapter, u32 opcode, u16 msg_size,
		     u8 *msg, u16 cookie)
{
	struct gve_mbx_queue *mbx_tx = adapter->mbx_tx;
	struct gve_mbx_desc *send_desc;
	struct gve_dma_mem *send_msg;
	u16 flags = 0;
	int err = 0;
	u32 val;

	/* reclaim TX mbx descriptors */
	gve_clean_send_mbx(adapter);

	send_desc = GVE_MBX_DESC(mbx_tx, mbx_tx->next_to_use);

	send_msg = kzalloc(sizeof(*send_msg), GFP_ATOMIC);
	if (!send_msg)
		return -ENOMEM;

	send_desc->destination = cpu_to_le16(0x0801); /* send message to CP */
	send_desc->pfid_vfid = 0;
	send_desc->buf_len = cpu_to_le16(msg_size);
	send_desc->cmd_opcode = cpu_to_le32(opcode);
	send_desc->cmd_cookie = cpu_to_le16(cookie);

	/* paylod */
	send_msg->va = dma_alloc_coherent(&adapter->pdev->dev, GVE_MBX_BUF_SIZE,
					  &send_msg->pa, GFP_ATOMIC);

	if (!send_msg->va) {
		err = -ENOMEM;
		goto dma_alloc_error;
	}
	send_msg->size = GVE_MBX_BUF_SIZE;

	send_desc->addr_high = cpu_to_le32(upper_32_bits(send_msg->pa));
	send_desc->addr_low = cpu_to_le32(lower_32_bits(send_msg->pa));

	/* set required flags */
	flags |= GVE_MBX_FLAG_BUF;
	flags |= GVE_MBX_FLAG_RD;

	send_desc->flags = cpu_to_le16(flags);

	if (msg && msg_size)
		memcpy(send_msg->va, msg, msg_size);

	adapter->mbx_tx_bufs[mbx_tx->next_to_use] = send_msg;

	mbx_tx->next_to_use++;
	if (mbx_tx->next_to_use == mbx_tx->ring_size)
		mbx_tx->next_to_use = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
	dma_wmb();
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) */
	wmb();
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) */
	iowrite32(mbx_tx->next_to_use, &adapter->mbx_tx->reg->queue_tail);
	val = ioread32(&adapter->mbx_tx->reg->queue_tail);
	return 0;

dma_alloc_error:
	kfree(send_msg);
	return err;
}

static void gve_fill_version_info(struct gve_mbx_caps_req *gve_caps_msg)
{
	gve_caps_msg->os_type = 1; /* Linux */
	gve_caps_msg->os_version_major = cpu_to_le32(LINUX_VERSION_MAJOR);
	gve_caps_msg->os_version_minor = cpu_to_le32(LINUX_VERSION_SUBLEVEL);
	gve_caps_msg->os_version_sub = cpu_to_le32(LINUX_VERSION_PATCHLEVEL);

	strscpy(gve_caps_msg->os_version_str, utsname()->release,
		sizeof(gve_caps_msg->os_version_str));
	strscpy(gve_caps_msg->driver_version_str, utsname()->version,
		sizeof(gve_caps_msg->driver_version_str));
}

static void gve_mbx_get_info_nic_tstamp_reg(struct gve_adapter *adapter)
{
	int err = gve_send_mbx_msg_wait(adapter,
					GVE_MBX_GET_INFO_NIC_TSTAMP_REG, 0,
					NULL);
	if (err)
		dev_err(&adapter->pdev->dev, "Failed get nic tstamp reg info.");

}

static void gve_mbx_get_info_flow_steering(struct gve_adapter *adapter)
{
	int err = gve_send_mbx_msg_wait(adapter, GVE_MBX_GET_INFO_FLOW_STEERING,
					0, NULL);
	if (err)
		dev_err(&adapter->pdev->dev,
			"Failed get flow steering info.");
}

int gve_mbx_negotiate_caps(struct gve_adapter *adapter)
{
	struct gve_mbx_caps_req *gve_caps_msg;
	int err;

	gve_caps_msg = kzalloc(sizeof(*gve_caps_msg), GFP_KERNEL);
	if (!gve_caps_msg)
		return -ENOMEM;

	gve_caps_msg->msg_version = cpu_to_le32(GVE_MBX_CAPS_MSG_V1);
	gve_caps_msg->supported_caps |=
		cpu_to_le64(GVE_MBX_CAP_DQO_RDA) |
		cpu_to_le64(GVE_MBX_CAP_FLOW_STEERING) |
		cpu_to_le64(GVE_MBX_CAP_NIC_TSTAMP_REG) |
		cpu_to_le64(GVE_MBX_CAP_NIC_TSTAMP_CMD);

	gve_fill_version_info(gve_caps_msg);
	gve_caps_msg->msg_size = cpu_to_le32(sizeof(*gve_caps_msg));

	dev_info(&adapter->pdev->dev, "%s: gve_caps_msg->msg_version = %d\n", __func__, le32_to_cpu(gve_caps_msg->msg_version));
	dev_info(&adapter->pdev->dev, "%s: gve_caps_msg->supported_caps = %llu\n", __func__, le64_to_cpu(gve_caps_msg->supported_caps));
	dev_info(&adapter->pdev->dev, "%s: gve_caps_msg->msg_size = %d\n", __func__, le32_to_cpu(gve_caps_msg->msg_size));

	err = gve_send_mbx_msg_wait(adapter, GVE_MBX_NEGOTIATE_CAPABILITIES,
				    sizeof(*gve_caps_msg), (u8 *)gve_caps_msg);
	if (err) {
		dev_err(&adapter->pdev->dev, "Failed to send negotiate caps mailbox msg\n");
		goto free_caps_msg;
	} else {
		dev_info(&adapter->pdev->dev, "Successfully sent negotiate caps mailbox msg\n");
	}

	if (adapter->caps->negotiated_caps & GVE_MBX_CAP_DQO_RDA) {
		adapter->device_info->queue_format = GVE_DQO_RDA_FORMAT;
	} else {
		dev_err(&adapter->pdev->dev,
			"Device does not support DQO RDA format, but driver only supports DQO RDA format in mailbox mode.\n");
		err = -EINVAL;
		goto free_caps_msg;
	}

	/* Get feature info for negotiated caps. */
	if (adapter->caps->negotiated_caps & GVE_MBX_CAP_FLOW_STEERING)
		gve_mbx_get_info_flow_steering(adapter);

	if (adapter->caps->negotiated_caps & GVE_MBX_CAP_NIC_TSTAMP_REG)
		gve_mbx_get_info_nic_tstamp_reg(adapter);

free_caps_msg:
	kfree(gve_caps_msg);
	return err;
}

static int gve_mbx_get_interrupt_dbs(struct gve_adapter *adapter,
				     struct gve_mbx_get_interrupt_dbs_req *request)
{
	int err;

	err = gve_send_mbx_msg_wait(adapter, GVE_MBX_GET_INTERRUPT_DBS,
				    sizeof(*request), (u8 *)request);
	if (err)
		dev_err(&adapter->pdev->dev,
			"Failed to send get ptype map message.");

	return err;

}

int gve_mbx_request_db_info(struct gve_adapter *adapter)
{
	int db_infos_per_response, remain_db_infos;
	struct gve_priv *priv = adapter->priv;
	// TODO: validate that his acutally needs to be num_tfy_blks
	int start_msix_idx = 1;

	/* Compute the maximum number of response items */
	db_infos_per_response = GVE_MBX_BUF_SIZE;
	db_infos_per_response -= sizeof(struct gve_mbx_get_interrupt_dbs_resp);
	db_infos_per_response /= sizeof(struct gve_mbx_interrupt_db_info);

	remain_db_infos = priv->num_ntfy_blks;
	adapter->next_msix_vec = start_msix_idx;
	do {
		struct gve_mbx_get_interrupt_dbs_req req;
		int err;

		req.num_vecs = min_t(int, remain_db_infos,
				     db_infos_per_response);
		req.start_msix_index = adapter->next_msix_vec;
		err = gve_mbx_get_interrupt_dbs(adapter, &req);
		if (err)
			return err;

		/* adapter->last_received_msix_vec is updated synchronously as
		 * part of mailbox RX processing */
		remain_db_infos = (priv->num_ntfy_blks + start_msix_idx) -
			adapter->next_msix_vec;
	} while (remain_db_infos);

	return 0;
}

static irqreturn_t gve_mbx_intr(int irq, void *arg)
{
	struct gve_adapter *adapter = arg;

	queue_delayed_work(adapter->gve_mbx_wq, &adapter->gve_mbx_task, 0);
	return IRQ_HANDLED;
}

int gve_mbx_setup_mgmt_irq(struct gve_adapter *adapter)
{
	struct gve_priv *priv = adapter->priv;
	int err;

	snprintf(priv->mgmt_msix_name, sizeof(priv->mgmt_msix_name),
		 "gve-mbx@pci:%s", pci_name(priv->pdev));
	err = request_irq(priv->msix_vectors[priv->mgmt_msix_idx].vector,
			  gve_mbx_intr, 0, priv->mgmt_msix_name, adapter);
	if (err) {
		dev_err(&adapter->pdev->dev,
			"Did not receive mailbox vector.\n");
		return err;
	}

	/* Cancel work queue to disable mbx polling. */
	cancel_delayed_work_sync(&adapter->gve_mbx_task);
	gve_set_mailbox_interrupt_ok(adapter);

	/* Re-scheule work queue to implicitly re-enable interrupts. */
	queue_delayed_work(adapter->gve_mbx_wq, &adapter->gve_mbx_task, 0);

	return 0;
}

void gve_mbx_teardown_mgmt_irq(struct gve_adapter *adapter)
{
	struct gve_priv *priv = adapter->priv;

	if (!priv->msix_vectors)
		return;

	gve_clear_mailbox_interrupt_ok(adapter);

	free_irq(priv->msix_vectors[priv->mgmt_msix_idx].vector,
		 adapter);

	/* Re-start mailbox polling. */
	queue_delayed_work(adapter->gve_mbx_wq, &adapter->gve_mbx_task, 0);
}

int gve_mbx_get_ptype_map(struct gve_adapter *adapter)
{
	int err = gve_send_mbx_msg_wait(adapter, GVE_MBX_GET_PTYPE_MAP, 0,
					NULL);
	if (err)
		dev_err(&adapter->pdev->dev,
			"Failed to send get ptype map message.");

	return err;
}

int gve_mbx_report_link_status(struct gve_adapter *adapter)
{
	int err;

	err = gve_send_mbx_msg_wait(adapter, GVE_MBX_REPORT_LINK_STATUS, 0,
				    NULL);
	if (err)
		dev_err(&adapter->pdev->dev,
			"Failed to send report link status request");

	return err;
}

int gve_mbx_query_rss(struct gve_adapter *adapter,
		      struct ethtool_rxfh_param *rxfh)
{
	struct gve_priv *priv = adapter->priv;
	int err;

	err = gve_send_mbx_msg_wait(adapter, GVE_MBX_QUERY_RSS, 0,
				    NULL);
	if (err) {
		dev_err(&adapter->pdev->dev,
			"Failed to query RSS confgiuration.");
		return err;
	}

	/* RSS information implicitly stored in priv as part of response message
	 * processing. */
	rxfh->hfunc = ETH_RSS_HASH_TOP;
	if (rxfh->indir) {
		rxfh->indir_size = priv->rss_lut_size;
		memcpy(rxfh->indir, priv->rss_config.hash_lut,
		       sizeof(*rxfh->indir) * rxfh->indir_size);
	}

	if (rxfh->key) {
		rxfh->key_size = priv->rss_key_size;
		memcpy(rxfh->key, priv->rss_config.hash_key,
		       sizeof(*rxfh->key) * rxfh->key_size);
	}

	return 0;
}

int gve_mbx_configure_rss(struct gve_adapter *adapter,
			  struct ethtool_rxfh_param *rxfh)
{
	struct gve_mbx_configure_rss_req *req;
	size_t request_size;
	int err = 0;
	int i;

	request_size = struct_size(req, hash_lut, rxfh->indir_size);
	req = kzalloc(request_size, GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	switch (rxfh->hfunc) {
	case ETH_RSS_HASH_NO_CHANGE:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,4,0)
		fallthrough;
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,4,0) */
		/* fallthrough */
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,4,0) */
	case ETH_RSS_HASH_TOP:
		req->hash_alg = GVE_MBX_HASH_ALG_TOEPLITZ;
		break;
	default:
		err = -EOPNOTSUPP;
		goto free_request;
	}

	req->hash_key_size = rxfh->key_size;
	if (rxfh->key)
		memcpy(req->hash_key, rxfh->key, rxfh->key_size);

	req->hash_lut_size = rxfh->indir_size;
	if (rxfh->indir) {
		for (i = 0; i < req->hash_lut_size; i++)
			req->hash_lut[i] = rxfh->indir[i];
	}

	err = gve_send_mbx_msg_wait(adapter, GVE_MBX_CONFIGURE_RSS,
				    request_size, (u8*)req);
	if (err)
		dev_err(&adapter->pdev->dev, "Failed to configure RSS.");

free_request:
	kfree(req);
	return err;
}

static int gve_mbx_query_flow_rules__ids(struct gve_adapter *adapter,
					 u32 starting_rule_id)
{
	struct gve_mbx_query_flow_rule_ids_req req;
	int err;

	/* Determine number of rules that can fit in response. */
	req.num_rule_ids = GVE_MBX_BUF_SIZE;
	req.num_rule_ids -= sizeof(struct gve_mbx_query_flow_rule_ids_resp);
	req.num_rule_ids /= sizeof(__le32);

	req.starting_rule_id = starting_rule_id;

	err = gve_send_mbx_msg_wait(adapter, GVE_MBX_QUERY_FLOW_RULE_IDS,
				    sizeof(req), (u8*)&req);
	if (err)
		dev_err(&adapter->pdev->dev, "Failed to query flow rule ids.");

	return err;
}

static int gve_mbx_query_flow_rules__rules(struct gve_adapter *adapter,
					   u32 starting_rule_id)
{
	struct gve_mbx_query_flow_rules_req req;
	int err;

	/* Determine number of rules that can fit in response. */
	req.num_rules = GVE_MBX_BUF_SIZE;
	req.num_rules -= sizeof(struct gve_mbx_query_flow_rules_resp);
	req.num_rules /= sizeof(struct gve_flow_rule_config);

	req.starting_rule_id = starting_rule_id;

	err = gve_send_mbx_msg_wait(adapter, GVE_MBX_QUERY_FLOW_RULES,
				    sizeof(req), (u8*)&req);
	if (err)
		dev_err(&adapter->pdev->dev, "Failed to query flow rules.");

	return err;
}

int gve_mbx_query_flow_rules(struct gve_adapter *adapter, u16 query_opcode,
			     u32 starting_loc)
{
	int err;

	switch(query_opcode) {
	case GVE_FLOW_RULE_QUERY_STATS:
		err = gve_send_mbx_msg_wait(adapter,
					    GVE_MBX_QUERY_FLOW_RULE_STATS, 0,
					    NULL);
		break;
	case GVE_FLOW_RULE_QUERY_IDS:
		err = gve_mbx_query_flow_rules__ids(adapter, starting_loc);
		break;
	case GVE_FLOW_RULE_QUERY_RULES:
		err = gve_mbx_query_flow_rules__rules(adapter, starting_loc);
		break;
	default:
		dev_err(&adapter->pdev->dev,
			"Unrecognized flow rules query opcode: %d",
			query_opcode);
		err = -EINVAL;
		break;
	}

	return err;
}

int gve_mbx_add_flow_rule(struct gve_adapter *adapter,
			  struct gve_flow_rule *rule, u32 loc)
{
	struct gve_mbx_add_flow_rule_req req;
	int err;

	req.rule_id = cpu_to_le32(loc);
	req.rule.flow_type = cpu_to_le16(rule->flow_type);
	req.rule.action = cpu_to_le16(rule->action);
	req.rule.key = rule->key;
	req.rule.mask = rule->mask;

	err = gve_send_mbx_msg_wait(adapter, GVE_MBX_ADD_FLOW_RULE, sizeof(req),
				    (u8*)&req);
	if (err)
		dev_err(&adapter->pdev->dev, "Failed to add flow rule %d.",
			loc);

	adapter->priv->flow_rules_cache.rules_cache_synced = false;
	return err;
}

int gve_mbx_del_flow_rule(struct gve_adapter *adapter, u32 loc)
{
	struct gve_mbx_del_flow_rule_req req;
	int err;

	req.rule_id = cpu_to_le32(loc);
	err = gve_send_mbx_msg_wait(adapter, GVE_MBX_DEL_FLOW_RULE, sizeof(req),
				    (u8*)&req);
	if (err)
		dev_err(&adapter->pdev->dev, "Failed to delete flow rule %d.",
			loc);

	adapter->priv->flow_rules_cache.rules_cache_synced = false;
	return err;
}

int gve_mbx_reset_flow_rules(struct gve_adapter *adapter)
{
	int err = gve_send_mbx_msg_wait(adapter, GVE_MBX_RESET_FLOW_RULES, 0,
					NULL);
	if (err)
		dev_err(&adapter->pdev->dev, "Failed to reset flow rules.");

	adapter->priv->flow_rules_cache.rules_cache_synced = false;
	return 0;
}

static void gve_post_rx_buffs(struct gve_adapter *adapter,
			      struct gve_dma_mem *dma_mem)
{
	struct gve_mbx_queue *mbx_rx = adapter->mbx_rx;
	u16 ntp = mbx_rx->next_to_post;
	struct gve_mbx_desc *desc;

	if (((ntp + 1) % mbx_rx->ring_size) == mbx_rx->next_to_clean)
		/* no buffers to clean */
		return;

	while (((ntp + 1) % mbx_rx->ring_size) != mbx_rx->next_to_clean) {
		desc = GVE_MBX_DESC(mbx_rx, ntp);

		if (adapter->mbx_rx_bufs[ntp])
			goto prep_desc;

		/* take back buffer passed down after recv msg */
		adapter->mbx_rx_bufs[ntp] = dma_mem;

prep_desc:
		desc->flags = cpu_to_le16(GVE_MBX_FLAG_BUF | GVE_MBX_FLAG_RD);
		desc->buf_len = GVE_MBX_BUF_SIZE;
		desc->addr_high =
			cpu_to_le32(upper_32_bits(adapter->mbx_rx_bufs[ntp]->pa));
		desc->addr_low =
			cpu_to_le32(lower_32_bits(adapter->mbx_rx_bufs[ntp]->pa));

		ntp++;
		if (ntp == mbx_rx->ring_size)
			ntp = 0;
	}

	/* update tail if buffers were posted */
	if (mbx_rx->next_to_post != ntp) {
		if (ntp)
			mbx_rx->next_to_post = ntp - 1;
		else
			mbx_rx->next_to_post = mbx_rx->ring_size - 1;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
		dma_wmb();
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) */
		wmb();
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) */
		iowrite32(mbx_rx->next_to_post, &mbx_rx->reg->queue_tail);
	}
}

int gve_mbx_set_ntfy_blks(struct gve_adapter *adapter)
{
	struct gve_device_info *device_info = adapter->device_info;
	struct gve_priv *priv = adapter->priv;

	/* The first vector is reserved for mailbox interrupt */
	priv->num_ntfy_blks = device_info->num_msix_vectors - 1;
	priv->mgmt_msix_idx = 0;
	return 0;
}

void gve_mbx_set_num_queues(struct gve_adapter *adapter)
{
	struct gve_device_info *device_info = adapter->device_info;
	struct gve_priv *priv = adapter->priv;

	priv->tx_cfg.max_queues = device_info->max_tx_queues;
	priv->rx_cfg.max_queues = device_info->max_rx_queues;

	priv->tx_cfg.num_queues = device_info->default_tx_queues;
	priv->rx_cfg.num_queues = device_info->default_rx_queues;
}

void gve_mbx_get_max_queues(struct gve_adapter *adapter, int *max_tx_queues,
			    int *max_rx_queues)
{
	struct gve_device_info *device_info = adapter->device_info;

	*max_tx_queues = device_info->max_tx_queues;
	*max_rx_queues = device_info->max_rx_queues;
}

static int gve_mbx_create_tx_queues(struct gve_adapter *adapter, u32 start_id,
				    u32 num_queues)
{
	struct gve_mbx_create_tx_q_req *create_tx_q_info;
	struct gve_priv *priv = adapter->priv;
	int size, i = 0, err = 0;
	u32 q_idx;

	size = struct_size(create_tx_q_info, tx_queues, num_queues);

	/* validate size */
	if (size > GVE_MBX_BUF_SIZE)
		return -ENOMEM;

	create_tx_q_info = kzalloc(size, GFP_KERNEL);
	if (!create_tx_q_info)
		return -ENOMEM;

	create_tx_q_info->num_queues = cpu_to_le16(num_queues);

	for (q_idx = start_id; q_idx < start_id + num_queues; q_idx++, i++) {
		struct gve_tx_ring *tx = &priv->tx[q_idx];
		u32 ntfy_idx = gve_tx_idx_to_ntfy(priv, q_idx);
		struct gve_mbx_tx_q_info *tx_q_info;

		tx_q_info = &create_tx_q_info->tx_queues[i];

		tx_q_info->queue_id = cpu_to_le32(q_idx);
		tx_q_info->msix_index =
			cpu_to_le16(gve_ntfy_to_msix_idx(priv, ntfy_idx));
		tx_q_info->queue_page_list_id = GVE_RAW_ADDRESSING_QPL_ID;
		tx_q_info->tx_ring_addr = cpu_to_le64(tx->bus);
		tx_q_info->tx_comp_ring_addr =
					cpu_to_le64(tx->complq_bus_dqo);
		tx_q_info->tx_ring_size = cpu_to_le16(priv->tx_desc_cnt);
		tx_q_info->tx_comp_ring_size = cpu_to_le16(priv->tx_desc_cnt);
		dev_info(&priv->pdev->dev,
    "GVE TX Config [q_idx %d]: ID=%u MSIX=%u QPL=%d RingAddr=%llx CompAddr=%llx Size=%u\n",
    q_idx,
    q_idx,                            /* queue_id */
    gve_ntfy_to_msix_idx(priv, ntfy_idx),            /* msix_index */
    GVE_RAW_ADDRESSING_QPL_ID,        /* queue_page_list_id */
    (unsigned long long)tx->bus,      /* tx_ring_addr */
    (unsigned long long)tx->complq_bus_dqo, /* tx_comp_ring_addr */
    priv->tx_desc_cnt                 /* tx_ring_size */
);
	}


	err = gve_send_mbx_msg_wait(adapter, GVE_MBX_CREATE_TX_QUEUES, size,
				    (u8 *)create_tx_q_info);
	if (err)
		dev_err(&adapter->pdev->dev, "Failed to send TX create queues message\n");
	else
		dev_info(&adapter->pdev->dev, "Successfully sent TX create queues message\n");

	kfree(create_tx_q_info);
	return err;
}

static int gve_mbx_create_rx_queues(struct gve_adapter *adapter, u32 num_queues)
{
	struct gve_mbx_create_rx_qs_req *create_rx_q_info;
	struct gve_priv *priv = adapter->priv;
	int size, err = 0, i = 0;
	u32 q_idx;

	size = struct_size(create_rx_q_info, rx_queues, num_queues);

	/* validate size */
	if (size > GVE_MBX_BUF_SIZE)
		return -ENOMEM;

	create_rx_q_info = kzalloc(size, GFP_KERNEL);
	if (!create_rx_q_info)
		return -ENOMEM;

	create_rx_q_info->num_queues = cpu_to_le16(num_queues);

	for (q_idx = 0; q_idx < num_queues; q_idx++, i++) {
		u32 ntfy_idx = gve_rx_idx_to_ntfy(priv, q_idx);
		struct gve_rx_ring *rx = &priv->rx[q_idx];
		struct gve_mbx_rx_q_info *rx_q_info;
		struct gve_notify_block *block;
		u32 flags = 0;

		block = &priv->ntfy_blocks[ntfy_idx];
		rx_q_info = &create_rx_q_info->rx_queues[i];

		if (priv->dev->features & NETIF_F_LRO)
			flags |= GVE_MBX_RX_QUEUE_ENABLE_RSC;

		rx_q_info->queue_id = cpu_to_le32(q_idx);
		rx_q_info->msix_index =
			cpu_to_le16(gve_ntfy_to_msix_idx(priv, ntfy_idx));
		rx_q_info->queue_page_list_id = GVE_RAW_ADDRESSING_QPL_ID;
		rx_q_info->flags = cpu_to_le32(flags);
		rx_q_info->rx_desc_ring_addr = cpu_to_le64(rx->dqo.complq.bus);
		rx_q_info->rx_data_ring_addr = cpu_to_le64(rx->dqo.bufq.bus);
		rx_q_info->rx_desc_ring_size = cpu_to_le16(priv->rx_desc_cnt);
		rx_q_info->rx_data_ring_size = cpu_to_le16(priv->rx_desc_cnt);
		rx_q_info->packet_buffer_size =
			cpu_to_le16(rx->packet_buffer_size);
		if (priv->header_split_enabled)
			rx_q_info->header_buffer_size =
				cpu_to_le16(priv->header_buf_size);
		dev_info(&priv->pdev->dev,
    "GVE RX Config [q_idx %d]: ID=%u MSIX=%u QPL=%d Flags=0x%x DescAddr=%llx DataAddr=%llx Size=%u\n",
    q_idx,
    q_idx,                            /* queue_id */
    gve_ntfy_to_msix_idx(priv, ntfy_idx),            /* msix_index */
    GVE_RAW_ADDRESSING_QPL_ID,        /* queue_page_list_id */
    flags,                            /* flags */
    (unsigned long long)rx->dqo.complq.bus, /* rx_desc_ring_addr */
    (unsigned long long)rx->dqo.bufq.bus,   /* rx_data_ring_addr */
    priv->rx_desc_cnt                 /* sizes (same for desc and data) */
);
	}


	err = gve_send_mbx_msg_wait(adapter, GVE_MBX_CREATE_RX_QUEUES, size,
				    (u8 *)create_rx_q_info);
	if (err)
		dev_err(&adapter->pdev->dev, "Failed to send create RX queues message\n");
	else
		dev_info(&adapter->pdev->dev, "Successfully sent create RX queues message\n");

	kfree(create_rx_q_info);
	return err;
}

static int gve_mbx_disable_tx_queues(struct gve_adapter *adapter, u16 *queues,
				     u16 num_queues)
{
	struct gve_mbx_disable_qs_req *req;
	size_t req_size;
	int err;
	int i;

	req_size = struct_size(req, queue_id, num_queues);
	req = kzalloc(req_size, GFP_KERNEL);

	req->num_queues = num_queues;
	for (i = 0; i < num_queues; i++)
		req->queue_id[i] = cpu_to_le16(queues[i]);

	err = gve_send_mbx_msg_wait(adapter, GVE_MBX_DESTROY_TX_QUEUES,
				    req_size, (u8 *)req);
	if (err)
		dev_err(&adapter->pdev->dev,
			"Request to disable tx queues failed.");

	return err;
}

static int gve_mbx_disable_rx_queues(struct gve_adapter *adapter, u16 *queues,
				     u16 num_queues)
{
	struct gve_mbx_disable_qs_req *req;
	size_t req_size;
	int err;
	int i;

	req_size = struct_size(req, queue_id, num_queues);
	req = kzalloc(req_size, GFP_KERNEL);

	req->num_queues = num_queues;
	for (i = 0; i < num_queues; i++)
		req->queue_id[i] = cpu_to_le16(queues[i]);

	err = gve_send_mbx_msg_wait(adapter, GVE_MBX_DESTROY_RX_QUEUES,
				    req_size, (u8 *)req);
	if (err)
		dev_err(&adapter->pdev->dev,
			"Request to disable rx queues failed.");

	return err;
}

int gve_mbx_disable_queues(struct gve_adapter *adapter)
{
	u16 *rx_queues = NULL, *tx_queues = NULL;
	struct gve_priv *priv = adapter->priv;
	int err;
	int i;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	rx_queues = kzalloc(array_size(priv->rx_cfg.num_queues, sizeof(u16)),
			    GFP_KERNEL);
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0) */
	rx_queues = kzalloc((priv->rx_cfg.num_queues * sizeof(u16)),
			    GFP_KERNEL);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0) */
	if (!rx_queues)
		return -ENOMEM;

	for (i = 0; i < priv->rx_cfg.num_queues; i++)
		rx_queues[i] = i;


	err = gve_mbx_disable_rx_queues(adapter, rx_queues,
					priv->rx_cfg.num_queues);
	if (err)
		goto free_arrays;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	tx_queues = kzalloc(array_size(gve_num_tx_queues(priv), sizeof(u16)),
			    GFP_KERNEL);
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0) */
	tx_queues = kzalloc((gve_num_tx_queues(priv) * sizeof(u16)),
			    GFP_KERNEL);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0) */
	if (!tx_queues) {
		err = -ENOMEM;
		goto free_arrays;
	}

	for (i = 0; i < gve_num_tx_queues(priv); i++)
		tx_queues[i] = i;

	err = gve_mbx_disable_tx_queues(adapter, tx_queues,
					gve_num_tx_queues(priv));

free_arrays:
	kfree(rx_queues);
	kfree(tx_queues);

	return err;
}


static int gve_mbx_enable_tx_queues(struct gve_adapter *adapter, u32 start_id,
				    u32 num_queues)
{
	struct gve_mbx_enable_qs_req *enable_tx_q_info;
	int size, err = 0, i = 0;
	u32 q_idx;

	size = struct_size(enable_tx_q_info, queue_id, num_queues);

	/* validate size */
	if (size > GVE_MBX_BUF_SIZE)
		return -ENOMEM;

	enable_tx_q_info = kzalloc(size, GFP_KERNEL);
	if (!enable_tx_q_info)
		return -ENOMEM;

	enable_tx_q_info->num_queues = cpu_to_le16(num_queues);

	for (q_idx = start_id; q_idx < start_id + num_queues; q_idx++, i++)
		enable_tx_q_info->queue_id[i] = cpu_to_le16(q_idx);

	err = gve_send_mbx_msg_wait(adapter, GVE_MBX_ENABLE_TX_QUEUES, size,
				    (u8 *)enable_tx_q_info);
	if (err)
		dev_err(&adapter->pdev->dev, "Failed to send enable TX queues message\n");
	 else
		dev_info(&adapter->pdev->dev, "Successfully sent enable TX queues message\n");

	kfree(enable_tx_q_info);
	return err;
}

static int gve_mbx_enable_rx_queues(struct gve_adapter *adapter, u32 num_queues)
{
	struct gve_mbx_enable_qs_req *enable_rx_q_info;
	int size, err = 0, i = 0;
	u32 q_idx;

	size = struct_size(enable_rx_q_info, queue_id, num_queues);

	/* validate size */
	if (size > GVE_MBX_BUF_SIZE)
		return -ENOMEM;

	enable_rx_q_info = kzalloc(size, GFP_KERNEL);
	if (!enable_rx_q_info)
		return -ENOMEM;

	enable_rx_q_info->num_queues = cpu_to_le16(num_queues);
	for (q_idx = 0; q_idx < num_queues; q_idx++, i++)
		enable_rx_q_info->queue_id[i] = cpu_to_le16(q_idx);

	err = gve_send_mbx_msg_wait(adapter, GVE_MBX_ENABLE_RX_QUEUES, size,
				    (u8 *)enable_rx_q_info);
	if (err)
		dev_err(&adapter->pdev->dev, "Failed to send enable RX queues message\n");
	else
		dev_info(&adapter->pdev->dev, "Successfully sent enable RX queues message\n");

	kfree(enable_rx_q_info);
	return err;
}

int gve_mbx_create_queues(struct gve_adapter *adapter)
{
	int num_tx_queues = gve_num_tx_queues(adapter->priv);
	struct gve_priv *priv = adapter->priv;
	int err;
	int i;

	err = gve_mbx_create_tx_queues(adapter, 0, num_tx_queues);
	if (err) {
		goto err;
	}

	err = gve_mbx_create_rx_queues(adapter, priv->rx_cfg.num_queues);
	if (err) {
		goto err;
	}

	// RX
	for (i = 0; i < priv->rx_cfg.num_queues; i++) {
		gve_mbx_write_q_doorbell(adapter, priv->rx[i].q_resources, 0);
	}
	// TX
	for (i = 0; i < num_tx_queues; i++) {
		gve_mbx_write_q_doorbell(adapter, priv->tx[i].q_resources, 0);
	}

	/* RX queues have to be enabled first */
	err = gve_mbx_enable_rx_queues(adapter, priv->rx_cfg.num_queues);
	if (err) {
		goto err;
	}
	err = gve_mbx_enable_tx_queues(adapter, 0, num_tx_queues);
	if (err) {
		goto err;
	}


err:
	return err;
}

void gve_mbx_write_q_doorbell(struct gve_adapter *adapter,
			      const struct gve_queue_resources *q_resources,
			      u32 val)
{
	u64 index = le32_to_cpu(q_resources->mbx_db_index);

	iowrite32(val, (u8*)adapter->reg_bar0 + index);
}

void gve_mbx_write_irq_doorbell_dqo(struct gve_adapter *adapter,
				    const struct gve_notify_block *block,
				    u32 val)
{
	u32 index;

	index = le32_to_cpu(block->mbx_db_info.irq_db_offset);
	iowrite32(val, (u8*)adapter->reg_bar0 + index);
}

static void gve_mbx_process_flow_steering_info(struct gve_adapter *adapter,
					       struct gve_dma_mem *recv_msg)
{
	struct gve_mbx_get_info_flow_steering_resp *resp = recv_msg->va;

	adapter->device_info->max_flow_rules = resp->max_flow_rules;
}

static void gve_print_caps(struct gve_adapter *adapter)
{
	struct device *dev = &adapter->pdev->dev;
	u64 caps = le64_to_cpu(adapter->caps->negotiated_caps);

	/* Decode and print the negotiated capability flags */
	if (caps & GVE_MBX_CAP_DQO_RDA) {
		adapter->device_info->queue_format = GVE_DQO_RDA_FORMAT;
		dev_info(dev, "GVE_MBX_CAP_DQO_RDA supported\n");
	}
	if (caps & GVE_MBX_CAP_DQO_QPL)
		dev_info(dev, "GVE_MBX_CAP_DQO_QPL supported\n");
	if (caps & GVE_MBX_CAP_INTERRUPT_SHARING)
		dev_info(dev, "GVE_MBX_CAP_INTERRUPT_SHARING supported\n");
	if (caps & GVE_MBX_CAP_FLOW_STEERING)
		dev_info(dev, "GVE_MBX_CAP_FLOW_STEERING supported\n");
	if (caps & GVE_MBX_CAP_NIC_TSTAMP_REG)
		dev_info(dev, "GVE_MBX_CAP_NIC_TSTAMP_REG supported\n");
	if (caps & GVE_MBX_CAP_NIC_TSTAMP_CMD)
		dev_info(dev, "GVE_MBX_CAP_NIC_TSTAMP_CMD supported\n");
}

static void gve_mbx_print_caps(struct gve_adapter *adapter, struct gve_mbx_caps_resp *caps)
{
	dev_info(&adapter->pdev->dev, "%s: caps->msg_version = %d\n", __func__, le32_to_cpu(caps->msg_version));
	dev_info(&adapter->pdev->dev, "%s: caps->msg_size = %d\n", __func__, le32_to_cpu(caps->msg_size));
	dev_info(&adapter->pdev->dev, "%s: caps->negotiated_caps = %lld\n", __func__, le64_to_cpu(caps->negotiated_caps));
	dev_info(&adapter->pdev->dev, "%s: caps->db_bar = %d\n", __func__, caps->db_bar);
	dev_info(&adapter->pdev->dev, "%s: caps->mbx_irq_db_offset = %d\n", __func__, le32_to_cpu(caps->mbx_irq_db_offset));
	dev_info(&adapter->pdev->dev, "%s: caps->mbx_response_timeout_ms = %d\n", __func__, le32_to_cpu(caps->mbx_response_timeout_ms));
	dev_info(&adapter->pdev->dev, "%s: caps->tx_queue_watchdog_timeout_ms = %d\n", __func__, le16_to_cpu(caps->tx_queue_watchdog_timeout_ms));
	dev_info(&adapter->pdev->dev, "%s: caps->num_msix_vectors = %d\n", __func__, le16_to_cpu(caps->num_msix_vectors));
	dev_info(&adapter->pdev->dev, "%s: caps->default_tx_num_queues = %d\n", __func__, le16_to_cpu(caps->default_tx_queues));
	dev_info(&adapter->pdev->dev, "%s: caps->default_rx_num_queues = %d\n", __func__, le16_to_cpu(caps->default_rx_queues));
	dev_info(&adapter->pdev->dev, "%s: caps->max_tx_num_queues = %d\n", __func__, le16_to_cpu(caps->max_tx_queues));
	dev_info(&adapter->pdev->dev, "%s: caps->max_rx_num_queues = %d\n", __func__, le16_to_cpu(caps->max_rx_queues));
	dev_info(&adapter->pdev->dev, "%s: caps->max_mtu = %d\n", __func__, le16_to_cpu(caps->max_mtu));
	dev_info(&adapter->pdev->dev, "%s: mac = %pM\n", __func__, caps->mac);
	dev_info(&adapter->pdev->dev, "%s: caps->default_tx_ring_size = %d\n", __func__, le16_to_cpu(caps->default_tx_ring_size));
	dev_info(&adapter->pdev->dev, "%s: caps->default_rx_ring_size = %d\n", __func__, le16_to_cpu(caps->default_rx_ring_size));
	dev_info(&adapter->pdev->dev, "%s: caps->max_tx_ring_size = %d\n", __func__, le16_to_cpu(caps->max_tx_ring_size));
	dev_info(&adapter->pdev->dev, "%s: caps->max_rx_ring_size = %d\n", __func__, le16_to_cpu(caps->max_rx_ring_size));
	dev_info(&adapter->pdev->dev, "%s: caps->min_tx_ring_size = %d\n", __func__, le16_to_cpu(caps->min_tx_ring_size));
	dev_info(&adapter->pdev->dev, "%s: caps->min_rx_ring_size = %d\n", __func__, le16_to_cpu(caps->min_rx_ring_size));
	dev_info(&adapter->pdev->dev, "%s: caps->max_packet_buffer_size = %d\n", __func__, le16_to_cpu(caps->max_packet_buffer_size));
	dev_info(&adapter->pdev->dev, "%s: caps->max_header_buffer_size = %d\n", __func__, le16_to_cpu(caps->max_header_buffer_size));
}

static void gve_mbx_fill_device_properties(struct gve_adapter *adapter,
					   struct gve_dma_mem *recv_msg)
{
	struct gve_device_info *device_info = adapter->device_info;
	struct gve_mbx_caps_resp *caps = recv_msg->va;

	//TODO remove
	gve_mbx_print_caps(adapter, caps);

	adapter->mbx_irq_db_offset = caps->mbx_irq_db_offset;

	/* queue properties */
	device_info->default_tx_queues = le16_to_cpu(caps->default_tx_queues);
	device_info->default_rx_queues = le16_to_cpu(caps->default_rx_queues);
	device_info->max_tx_queues = le16_to_cpu(caps->max_tx_queues);
	device_info->max_rx_queues = le16_to_cpu(caps->max_rx_queues);

	/* ring size properties */
	device_info->modify_ring_size_enabled = true;
	device_info->default_tx_ring_size =
					le16_to_cpu(caps->default_tx_ring_size);
	device_info->default_rx_ring_size =
					le16_to_cpu(caps->default_rx_ring_size);
	device_info->max_tx_ring_size = le16_to_cpu(caps->max_tx_ring_size);
	device_info->max_rx_ring_size = le16_to_cpu(caps->max_rx_ring_size);
	device_info->min_tx_ring_size = le16_to_cpu(caps->min_tx_ring_size);
	device_info->min_rx_ring_size = le16_to_cpu(caps->min_rx_ring_size);

	device_info->num_msix_vectors = le16_to_cpu(caps->num_msix_vectors);
	device_info->max_mtu = le16_to_cpu(caps->max_mtu);
	ether_addr_copy(device_info->mac, caps->mac);

	device_info->max_rx_buffer_size =
				le16_to_cpu(caps->max_packet_buffer_size);
	device_info->header_buf_size =
				le16_to_cpu(caps->max_header_buffer_size);

	device_info->rss_key_size = le16_to_cpu(caps->hash_key_size);
	device_info->rss_lut_size = le16_to_cpu(caps->hash_lut_size);

	/* Print all device_info properties */
	dev_info(&adapter->pdev->dev,  "%s: device_info->default_tx_queues = %u\n", __func__, device_info->default_tx_queues);
	dev_info(&adapter->pdev->dev,  "%s: device_info->default_rx_queues = %u\n", __func__, device_info->default_rx_queues);
	dev_info(&adapter->pdev->dev,  "%s: device_info->max_tx_queues = %u\n", __func__, device_info->max_tx_queues);
	dev_info(&adapter->pdev->dev,  "%s: device_info->max_rx_queues = %u\n", __func__, device_info->max_rx_queues);
	dev_info(&adapter->pdev->dev,  "%s: device_info->default_tx_ring_size = %u\n", __func__, device_info->default_tx_ring_size);
	dev_info(&adapter->pdev->dev,  "%s: device_info->default_rx_ring_size = %u\n", __func__, device_info->default_rx_ring_size);
	dev_info(&adapter->pdev->dev,  "%s: device_info->max_tx_ring_size = %u\n", __func__, device_info->max_tx_ring_size);
	dev_info(&adapter->pdev->dev,  "%s: device_info->max_rx_ring_size = %u\n", __func__, device_info->max_rx_ring_size);
	dev_info(&adapter->pdev->dev,  "%s: device_info->min_tx_ring_size = %u\n", __func__, device_info->min_tx_ring_size);
	dev_info(&adapter->pdev->dev,  "%s: device_info->min_rx_ring_size = %u\n", __func__, device_info->min_rx_ring_size);
	dev_info(&adapter->pdev->dev,  "%s: device_info->num_msix_vectors = %u\n", __func__, device_info->num_msix_vectors);
	dev_info(&adapter->pdev->dev,  "%s: device_info->max_mtu = %u\n", __func__, device_info->max_mtu);
	dev_info(&adapter->pdev->dev,  "%s: device_info->mac = %pM\n", __func__, device_info->mac);
	dev_info(&adapter->pdev->dev,  "%s: device_info->max_rx_buffer_size = %u\n", __func__, device_info->max_rx_buffer_size);
	dev_info(&adapter->pdev->dev,  "%s: device_info->header_buf_size = %u\n", __func__, device_info->header_buf_size);
	dev_info(&adapter->pdev->dev,  "%s: device_info->rss_key_size = %u\n", __func__, device_info->rss_key_size);
	dev_info(&adapter->pdev->dev,  "%s: device_info->rss_lut_size = %u\n", __func__, device_info->rss_lut_size);
}

static int gve_mbx_process_capabilities(struct gve_adapter *adapter,
					struct gve_dma_mem *recv_msg)
{
	struct gve_mbx_caps_resp *caps = recv_msg->va;

	adapter->caps = kzalloc(sizeof(*caps), GFP_KERNEL);
	if (!adapter->caps)
		return -ENOMEM;

	if (caps)
		memcpy(adapter->caps, caps, sizeof(*caps));

	gve_print_caps(adapter);
	gve_mbx_fill_device_properties(adapter, recv_msg);
	return 0;
}

static int gve_mbx_process_event(struct gve_adapter *adapter,
				 struct gve_dma_mem *recv_msg)
{
	struct gve_mbx_event *mbx_event = recv_msg->va;
	struct gve_priv *priv = adapter->priv;
	u32 event_id = le32_to_cpu(mbx_event->event_id);

	dev_info(&adapter->pdev->dev, "Processing mailbox event: %s 0x%x\n",
		 gve_mbx_event_id_to_string(event_id), event_id);

	if (!priv) {
		dev_err(&priv->pdev->dev,
			"Received mailbox event before priv exists.");
		return -EEXIST;
	}

	switch (event_id) {
	case GVE_MBX_EVENT_LINK_STATUS_CHANGE:
		queue_work(priv->gve_wq, &adapter->gve_mbx_lsc_event_task);
		break;
	default:
		dev_err(&adapter->pdev->dev, "Unknown mailbox event: 0x%x",
			event_id);
	}

	return 0;
}

static void gve_mbx_process_nic_timestamp_reg_info(struct gve_adapter *adapter,
						   struct gve_dma_mem *recv_msg)
{
	struct gve_mbx_get_info_nic_tstamp_reg_resp *resp = recv_msg->va;

	if (resp->bar != 0) {
		dev_info(&adapter->pdev->dev,
			 "nktgrg: response bar is not 0\n");
		return;
	}

	/* TODO : handle bar and clk_type from the response */
	adapter->device_info->nic_timestamp_supported = true;
	adapter->device_info->clk_read_type = GVE_DEV_CLK_MMIO;
	adapter->dev_clk_ns_l =
		(void *)adapter->reg_bar0 + resp->dev_clk_ns_l_offset;
	adapter->dev_clk_ns_h =
		(void *)adapter->reg_bar0 + resp->dev_clk_ns_h_offset;
	adapter->dev_art_ns_l =
		(void *)adapter->reg_bar0 + resp->sys_clk_ns_l_offset;
	adapter->dev_art_ns_h =
		(void *)adapter->reg_bar0 + resp->sys_clk_ns_h_offset;
	adapter->dev_clk_cmd_sync =
		(void *)adapter->reg_bar0 + resp->cmd_sync_trigger_offset;
}

static int gve_mbx_process_create_tx_queues(struct gve_adapter *adapter,
					     struct gve_dma_mem *recv_msg)
{
	struct gve_mbx_create_tx_qs_resp *created_tx_q_info = recv_msg->va;
	struct gve_priv *priv = adapter->priv;
	int i;

	dev_info(&priv->pdev->dev, "CREATE_TX_QUEUES: num_qs: %d",
		 created_tx_q_info->num_queues);

	/* Populate notify blocks */
	for (i = 0; i < created_tx_q_info->num_queues; i++) {
		u32 q_idx = le32_to_cpu(created_tx_q_info->queues[i].queue_id);
		struct gve_tx_ring *tx_ring = &priv->tx[q_idx];

		if (!tx_ring)
			return -EINVAL;

		tx_ring->q_resources->mbx_db_index = created_tx_q_info->queues[i].tail_db_offset;
		dev_info(&adapter->pdev->dev, "%s TX Q idx = %d, db_indx = %d\n", __func__, q_idx, le32_to_cpu(tx_ring->q_resources->mbx_db_index));
	}
	return 0;
}

static int gve_mbx_process_create_rx_queues(struct gve_adapter *adapter,
					     struct gve_dma_mem *recv_msg)
{
	struct gve_mbx_create_rx_qs_resp *created_rx_q_info = recv_msg->va;
	struct gve_priv *priv = adapter->priv;
	int i;

	dev_info(&priv->pdev->dev, "CREATE_RX_QUEUES: num_qs: %d",
		 created_rx_q_info->num_queues);

	/* Populate notify blocks */
	for (i = 0; i < created_rx_q_info->num_queues; i++) {
		u32 q_idx = le32_to_cpu(created_rx_q_info->queues[i].queue_id);
		struct gve_rx_ring *rx_ring = &priv->rx[q_idx];

		if (!rx_ring)
			return -EINVAL;

		rx_ring->q_resources->mbx_db_index = created_rx_q_info->queues[i].tail_db_offset;
		dev_info(&adapter->pdev->dev,"%s RX Q idx = %d, db_indx = %d\n", __func__, q_idx, le32_to_cpu(rx_ring->q_resources->mbx_db_index));
	}
	return 0;
}

static void gve_mbx_process_interrupt_dbs(struct gve_adapter *adapter,
					 struct gve_dma_mem *recv_msg)
{
	struct gve_mbx_get_interrupt_dbs_resp *db_resp = recv_msg->va;
	struct gve_priv *priv = adapter->priv;
	int i;

	dev_info(&priv->pdev->dev, "GET_INTERRUPT_DBS: msix start id: %d, num_vecs: %d",
		 db_resp->start_msix_index, db_resp->num_vecs);

	/* Populate notify blocks */
	for (i = 0; i < db_resp->num_vecs; i++) {
		int msix_index = db_resp->start_msix_index + i;
		struct gve_notify_block *block;
		int notify_index;

		if (msix_index == 0 || msix_index > priv->num_ntfy_blks)
			continue;

		notify_index = gve_msix_idx_to_ntfy(priv, msix_index);
		block = &priv->ntfy_blocks[notify_index];
		memcpy(&block->mbx_db_info, &db_resp->info[i],
		       sizeof(struct gve_mbx_interrupt_db_info));
	}

	adapter->next_msix_vec =
		db_resp->start_msix_index + db_resp->num_vecs;
}

static int gve_mbx_process_ptype_map(struct gve_adapter *adapter,
				     struct gve_dma_mem *recv_msg)
{
	struct gve_ptype_lut *ptype_map = recv_msg->va;

	memcpy(adapter->priv->ptype_lut_dqo, ptype_map, sizeof(*ptype_map));
	return 0;
}

static int gve_mbx_process_link_status(struct gve_adapter *adapter,
				       struct gve_dma_mem *recv_msg)
{
	struct gve_mbx_report_link_status_resp *resp = recv_msg->va;
	struct gve_priv *priv = adapter->priv;

	priv->link_speed = le32_to_cpu(resp->link_speed);
	priv->link_up = resp->link_up;
	return 0;
}

static int gve_mbx_process_query_rss(struct gve_adapter *adapter,
				     struct gve_dma_mem *recv_msg)
{
	struct gve_rss_config *rss_config = &adapter->priv->rss_config;
	struct gve_mbx_query_rss_resp *resp = recv_msg->va;
	int i;

	for (i = 0; i < resp->hash_lut_size; i++) {
		rss_config->hash_lut[i] = le32_to_cpu(resp->hash_lut[i]);
	}

	memcpy(rss_config->hash_key, resp->hash_key, resp->hash_key_size);

	return 0;
}

static int gve_mbx_process_flow_rule_ids(struct gve_adapter *adapter,
					 struct gve_dma_mem *recv_msg)
{
	struct gve_mbx_query_flow_rule_ids_resp *resp = recv_msg->va;
	struct gve_priv *priv = adapter->priv;
	int i;

	for (i = 0; i < resp->num_rule_ids; i++)
		priv->flow_rules_cache.rule_ids_cache[i] =
			le32_to_cpu(resp->rule_ids[i]);
	priv->flow_rules_cache.rule_ids_cache_num = resp->num_rule_ids;

	return 0;
}

static int gve_mbx_process_flow_rules(struct gve_adapter *adapter,
				      struct gve_dma_mem *recv_msg)
{
	struct gve_mbx_query_flow_rules_resp *resp = recv_msg->va;
	struct gve_flow_rule_config *rules_cache;
	struct gve_priv *priv = adapter->priv;
	int i;

	rules_cache = priv->flow_rules_cache.rules_cache;

	priv->flow_rules_cache.rules_cache_num = resp->num_rules;
	for (i = 0; i < resp->num_rules; i++) {
		struct gve_flow_rule *rule = &rules_cache[i].flow_rule;

		rules_cache[i].location = le32_to_cpu(resp->rules[i].location);

		rule->flow_type =
			le16_to_cpu(resp->rules[i].flow_rule.flow_type);
		rule->action = le16_to_cpu(resp->rules[i].flow_rule.action);

		/* Key and mask will always be in network byte order. */
		rule->key = resp->rules[i].flow_rule.key;
		rule->mask = resp->rules[i].flow_rule.mask;
	}

	priv->flow_rules_cache.rules_cache_num = resp->num_rules;

	return 0;
}

static int gve_mbx_process_flow_rule_stats(struct gve_adapter *adapter,
					   struct gve_dma_mem *recv_msg)
{
	struct gve_mbx_query_flow_rule_stats_resp *resp = recv_msg->va;
	struct gve_priv *priv = adapter->priv;

	priv->max_flow_rules = resp->max_flow_rules;
	priv->num_flow_rules = resp->num_flow_rules;

	return 0;
}

static int gve_process_mbx_msg(struct gve_adapter *adapter, u32 opcode,
			       struct gve_dma_mem *recv_msg)
{
	int err = 0;

	switch (opcode) {
	case GVE_MBX_NEGOTIATE_CAPABILITIES:
		err = gve_mbx_process_capabilities(adapter, recv_msg);
		break;
	case GVE_MBX_GET_INFO_NIC_TSTAMP_REG:
		gve_mbx_process_nic_timestamp_reg_info(adapter, recv_msg);
		break;
	case GVE_MBX_GET_INFO_FLOW_STEERING:
		gve_mbx_process_flow_steering_info(adapter, recv_msg);
		break;
	case GVE_MBX_GET_INTERRUPT_DBS:
		gve_mbx_process_interrupt_dbs(adapter, recv_msg);
		break;
	case GVE_MBX_GET_PTYPE_MAP:
		err = gve_mbx_process_ptype_map(adapter, recv_msg);
		break;
	case GVE_MBX_REPORT_LINK_STATUS:
		err = gve_mbx_process_link_status(adapter, recv_msg);
		break;
	case GVE_MBX_CREATE_TX_QUEUES:
		err = gve_mbx_process_create_tx_queues(adapter, recv_msg);
		break;
	case GVE_MBX_CREATE_RX_QUEUES:
		err = gve_mbx_process_create_rx_queues(adapter, recv_msg);
		break;
	case GVE_MBX_ENABLE_TX_QUEUES:
		break;
	case GVE_MBX_ENABLE_RX_QUEUES:
		break;
	case GVE_MBX_DESTROY_TX_QUEUES:
		/* No processing needed */
		break;
	case GVE_MBX_DESTROY_RX_QUEUES:
		/* No processing needed */
		break;
	case GVE_MBX_QUERY_RSS:
		gve_mbx_process_query_rss(adapter, recv_msg);
		break;
	case GVE_MBX_CONFIGURE_RSS:
		/* No processing needed. */
		break;
	case GVE_MBX_QUERY_FLOW_RULE_STATS:
		err = gve_mbx_process_flow_rule_stats(adapter,
						      recv_msg);
		break;
	case GVE_MBX_QUERY_FLOW_RULE_IDS:
		err = gve_mbx_process_flow_rule_ids(adapter,
						    recv_msg);
		break;
	case GVE_MBX_QUERY_FLOW_RULES:
		err = gve_mbx_process_flow_rules(adapter, recv_msg);
		break;
	case GVE_MBX_ADD_FLOW_RULE:
		/* No processing needed. */
		break;
	case GVE_MBX_DEL_FLOW_RULE:
		/* No processing needed. */
		break;
	case GVE_MBX_RESET_FLOW_RULES:
		/* No processing needed. */
		break;
	default:
		err = -EBADMSG;
		dev_err_ratelimited(&adapter->pdev->dev,
				    "Received unrecognized opcode = 0x%x\n",
				    opcode);
	}

	return err;
}

static void gve_process_mbx_resp_completion(struct gve_adapter *adapter,
					    struct gve_mbx_desc *recv_desc)
{
	struct gve_mbx_msg_queue *msg_queue = adapter->mbx_msg_queue;
	struct gve_mbx_msg *mbx_msg;
	u16 cookie, index;
	u32 opcode;
	int err;

	err = le16_to_cpu(recv_desc->cmd_retval);
	cookie = le16_to_cpu(recv_desc->cmd_cookie);
	opcode = le32_to_cpu(recv_desc->cmd_opcode);

	index = cookie & GVE_MBX_COOKIE_INDEX_MASK;

	BUG_ON(opcode == GVE_MBX_EVENT);

	if (gve_mbx_should_log(opcode))
		dev_info(
			&adapter->pdev->dev,
			"Received MBX resp: opcode=%s 0x%x, index=%u, salt=%u, cookie=0x%04x\n",
			gve_mbx_opcode_to_string(opcode), opcode, index,
			(cookie >> GVE_MBX_COOKIE_INDEX_BITS) & 0x1FF, cookie);

	if (index >= msg_queue->size) {
		dev_err(&adapter->pdev->dev,
			"MBX response index out of bounds: cookie=0x%x opcode=%s 0x%x\n",
			cookie, gve_mbx_opcode_to_string(opcode), opcode);
		return;
	}

	mbx_msg = adapter->mbx_msgs[index];
	if (!test_bit(index, msg_queue->msg_queue_map) || !mbx_msg) {
		dev_err(&adapter->pdev->dev,
			"No pending mbx msg for response: cookie=0x%x, opcode=%s 0x%x\n",
			cookie, gve_mbx_opcode_to_string(opcode), opcode);
		return;
	}

	if (mbx_msg->sw_cookie != cookie) {
		dev_err(&adapter->pdev->dev,
			"MBX cookie mismatch expected: 0x%04x, received: 0x%04x, expected_opcode=%s 0x%x, received_opcode=%s 0x%x\n",
			mbx_msg->sw_cookie, cookie,
			gve_mbx_opcode_to_string(mbx_msg->opcode),
			mbx_msg->opcode, gve_mbx_opcode_to_string(opcode),
			opcode);
		return;
	}

	if (mbx_msg->opcode != opcode) {
		dev_err(&adapter->pdev->dev,
			"MBX opcode mismatch expected: %s 0x%x, received: %s 0x%x\n",
			gve_mbx_opcode_to_string(mbx_msg->opcode),
			mbx_msg->opcode, gve_mbx_opcode_to_string(opcode),
			opcode);
		return;
	}

	mbx_msg->status = err;
	complete(&mbx_msg->work);
}

static void gve_process_mbx_resp_status(struct gve_adapter *adapter,
					struct gve_mbx_desc *recv_desc)
{
	u16 opcode = le16_to_cpu(recv_desc->cmd_opcode);
	u16 retval = le16_to_cpu(recv_desc->cmd_retval);

	dev_err(&adapter->pdev->dev,
		"Error 0x%04X received in mailbox response for opcode %s 0x%04X\n",
		retval, gve_mbx_opcode_to_string(opcode), opcode);

	/* GVE_MBX_STATUS_UNAVAILABLE_ERROR means that the device is in
	 * an unrecoverable state. Trigger reset.
	 */
	if (recv_desc->cmd_retval == GVE_MBX_STATUS_UNAVAILABLE_ERROR &&
	    adapter->priv)
			gve_schedule_reset(adapter->priv);
}

int gve_receive_mbx_msg(struct gve_adapter *adapter)
{
	struct gve_mbx_queue *mbx_rx = adapter->mbx_rx;
	struct gve_mbx_desc *recv_desc;
	struct gve_dma_mem *recv_msg;
	u16 ntc, flags;
	int err = 0;

	if (!gve_get_mbx_queue_ok(adapter))
		return -EIO;

	spin_lock(&mbx_rx->q_lock);

	ntc = mbx_rx->next_to_clean;
	recv_desc = GVE_MBX_DESC(mbx_rx, ntc);
	flags = le16_to_cpu(recv_desc->flags);

	/* check if desc is marked as done */
	if (!(flags & GVE_MBX_FLAG_DD)) {
		err = -EAGAIN;
		goto err_unlock;
	}

	recv_msg = adapter->mbx_rx_bufs[ntc];

	u32 opcode = le32_to_cpu(recv_desc->cmd_opcode);
	if (opcode == GVE_MBX_EVENT) {
		if (recv_desc->cmd_retval != GVE_MBX_STATUS_PASSED) {
			dev_err(&adapter->pdev->dev,
				"Received unexpected status in MBX_EVENT notification: 0x%x\n",
				recv_desc->cmd_retval);
			err = -EBADMSG;
			goto update_tail;
		}

		gve_mbx_process_event(adapter, recv_msg);
		goto update_tail;
	}

	/* check for error status code in response */
	if (recv_desc->cmd_retval != GVE_MBX_STATUS_PASSED) {
		gve_process_mbx_resp_status(adapter, recv_desc);
		gve_process_mbx_resp_completion(adapter, recv_desc);
		err = -EBADMSG;
		goto update_tail;
	}

	if (recv_desc->buf_len) {
		err = gve_process_mbx_msg(adapter, opcode, recv_msg);
		if (err)
			dev_err(&adapter->pdev->dev,
				"Error processing message %s 0x%x at ntc = %d\n",
				gve_mbx_opcode_to_string(opcode), opcode, ntc);

		gve_process_mbx_resp_completion(adapter, recv_desc);
	} else {
		/* buffer not used, should not reach this condition */
		err = -EBADMSG;
		dev_err(&adapter->pdev->dev,
			"Direct message received, data_len in descriptor set to 0\n");
	}

update_tail:

	/* set to NULL so this can be posted back in post_buffers */
	adapter->mbx_rx_bufs[ntc] = NULL;

	memset(recv_desc, 0, sizeof(*recv_desc));

	/* update next_to_clean */
	ntc++;
	if (ntc == mbx_rx->ring_size)
		ntc = 0;

	adapter->mbx_rx->next_to_clean = ntc;

	spin_unlock(&mbx_rx->q_lock);

	/* post the buffers back */
	gve_post_rx_buffs(adapter, recv_msg);

	return err;

err_unlock:
	spin_unlock(&mbx_rx->q_lock);
	return err;
}

static void gve_free_mbx_rx_buf(struct gve_adapter *adapter,
				struct gve_dma_mem *rx_buf)
{
	dma_free_coherent(&adapter->pdev->dev, rx_buf->size, rx_buf->va,
			  rx_buf->pa);

	rx_buf->size = 0;
	rx_buf->va = NULL;
	rx_buf->pa = 0;
}

static void gve_free_mbx_rcv_buffers(struct gve_adapter *adapter)
{
	u16 ring_size = adapter->mbx_rx->ring_size;
	int i;

	for(i = 0; i < ring_size; i++) {
		if (adapter->mbx_rx_bufs[i]) {
			gve_free_mbx_rx_buf(adapter, adapter->mbx_rx_bufs[i]);
			kfree(adapter->mbx_rx_bufs[i]);
			adapter->mbx_rx_bufs[i] = NULL;
		}
	}

	if (adapter->mbx_rx_bufs)
		kfree(adapter->mbx_rx_bufs);
	adapter->mbx_rx_bufs = NULL;
}

static int gve_alloc_mbx_rcv_buffers(struct gve_adapter *adapter)
{
	u16 ring_size = adapter->mbx_rx->ring_size;
	int i;

	adapter->mbx_rx_bufs = kcalloc(ring_size, sizeof(struct gve_dma_mem *),
				    GFP_KERNEL);

	if (!adapter->mbx_rx_bufs)
		return -ENOMEM;

	for (i = 0; i < ring_size - 1; i++) {
		struct gve_dma_mem *rx_buf;

		adapter->mbx_rx_bufs[i] = kcalloc(1, sizeof(struct gve_dma_mem),
					       GFP_KERNEL);

		if (!adapter->mbx_rx_bufs[i])
			goto free_bufs;

		rx_buf = adapter->mbx_rx_bufs[i];
		rx_buf->va = dma_alloc_coherent(&adapter->pdev->dev,
						GVE_MBX_BUF_SIZE, &rx_buf->pa,
						GFP_KERNEL);
		if (!rx_buf->va) {
			kfree(adapter->mbx_rx_bufs[i]);
			goto free_bufs;
		}
		rx_buf->size = GVE_MBX_BUF_SIZE;
	}
	adapter->mbx_rx->next_to_post = ring_size - 1;

	return 0;

free_bufs:
	i--;
	for (; i >= 0; i--) {
		gve_free_mbx_rx_buf(adapter, adapter->mbx_rx_bufs[i]);
		kfree(adapter->mbx_rx_bufs[i]);
		adapter->mbx_rx_bufs = NULL;
	}
	kfree(adapter->mbx_rx_bufs);
	adapter->mbx_rx_bufs = NULL;
	return -ENOMEM;
}

static void gve_post_initial_rx_bufs(struct gve_adapter *adapter)
{
	struct gve_mbx_queue *mbx_rx = adapter->mbx_rx;
	int i;

	for (i = 0; i < mbx_rx->ring_size; i++) {
		struct gve_mbx_desc *desc = GVE_MBX_DESC(mbx_rx, i);
		struct gve_dma_mem *gve_rx_buf = adapter->mbx_rx_bufs[i];

		if (!gve_rx_buf)
			continue;

		desc->flags = cpu_to_le16(GVE_MBX_FLAG_BUF | GVE_MBX_FLAG_RD);
		desc->addr_high = cpu_to_le32(upper_32_bits(gve_rx_buf->pa));
		desc->addr_low = cpu_to_le32(lower_32_bits(gve_rx_buf->pa));
		desc->buf_len = GVE_MBX_BUF_SIZE;
	}
}

static int gve_alloc_mbx_desc_ring(struct gve_adapter *adapter,
				   struct gve_dma_mem *desc_ring,
				   u16 ring_size)
{
	int size = ring_size * sizeof(struct gve_mbx_desc);

	desc_ring->va = dma_alloc_coherent(&adapter->pdev->dev, size,
					   &desc_ring->pa, GFP_KERNEL);
	if (!desc_ring->va)
		return -ENOMEM;

	desc_ring->size = size;
	return 0;
}

static void gve_free_mbx_desc_ring(struct gve_adapter *adapter,
				   struct gve_mbx_queue *mbx_q)
{
	struct gve_dma_mem *desc_ring = &mbx_q->desc_ring;

	dma_free_coherent(&adapter->pdev->dev, desc_ring->size, desc_ring->va,
			  desc_ring->pa);
	desc_ring->size = 0;
	desc_ring->va = NULL;
	desc_ring->pa  = 0;
}

static void gve_free_mbx_msg_queue(struct gve_adapter *adapter)
{
	int i;

	if (!adapter->mbx_msg_queue)
		return;

	for (i = 0; i < adapter->mbx_msg_queue->size; i++) {
		if (adapter->mbx_msgs[i]) {
			struct gve_mbx_msg *mbx_msg = adapter->mbx_msgs[i];

			mbx_msg->status = GVE_MBX_STATUS_CANCELLED_ERROR;
			complete(&mbx_msg->work);
		}
	}

	kfree(adapter->mbx_msgs);
	adapter->mbx_msgs = NULL;
	bitmap_free(adapter->mbx_msg_queue->msg_queue_map);
	kfree(adapter->mbx_msg_queue);
	adapter->mbx_msg_queue = NULL;
}

void gve_free_mailbox(struct gve_adapter *adapter)
{
	int err;

	if (!gve_get_mbx_queue_ok(adapter))
		return;

	gve_clear_mbx_queue_ok(adapter);

	err = gve_mbx_reset(adapter);
	if (err)
		dev_err(&adapter->pdev->dev, "Failed to reset in mailbox mode\n");

	cancel_delayed_work_sync(&adapter->gve_mbx_task);
	if (adapter->mbx_rx) {
		gve_free_mbx_rcv_buffers(adapter);
		gve_free_mbx_desc_ring(adapter, adapter->mbx_rx);
		kfree(adapter->mbx_rx);
	}

	if (adapter->mbx_tx) {
		kfree(adapter->mbx_tx_bufs);
		gve_free_mbx_desc_ring(adapter, adapter->mbx_tx);
		kfree(adapter->mbx_tx);
	}

	adapter->mbx_rx = NULL;
	adapter->mbx_tx = NULL;
	gve_free_mbx_msg_queue(adapter);
	if (adapter->gve_mbx_wq) {
		destroy_workqueue(adapter->gve_mbx_wq);
		adapter->gve_mbx_wq = NULL;
	}
}

static int gve_alloc_mbx_msg_queue(struct gve_adapter *adapter)
{
	struct gve_mbx_msg_queue *mbx_msg_queue;
	int err;

	adapter->mbx_msg_queue = kzalloc(sizeof(*adapter->mbx_msg_queue),
					 GFP_KERNEL);
	if (!adapter->mbx_msg_queue)
		return -ENOMEM;

	mbx_msg_queue = adapter->mbx_msg_queue;
	mbx_msg_queue->size = GVE_MBX_MSG_QUEUE_LEN;
	mbx_msg_queue->msg_queue_map = bitmap_zalloc(mbx_msg_queue->size,
						     GFP_KERNEL);

	if (!mbx_msg_queue->msg_queue_map) {
		err = -ENOMEM;
		goto err_free_msg_queue;
	}

	adapter->mbx_msgs = kcalloc(mbx_msg_queue->size,
				    sizeof(struct gve_mbx_msg *), GFP_KERNEL);
	if (!adapter->mbx_msgs) {
		err = -ENOMEM;
		goto err_free_msg_queue_map;
	}

	spin_lock_init(&mbx_msg_queue->mbx_msg_q_lock);
	return 0;

err_free_msg_queue_map:
	bitmap_free(adapter->mbx_msg_queue->msg_queue_map);
err_free_msg_queue:
	kfree(adapter->mbx_msg_queue);
	adapter->mbx_msg_queue = NULL;
	return err;
}

static int gve_alloc_mailbox(struct gve_adapter *adapter)
{
	int err = 0;

	adapter->mbx_tx = kzalloc(sizeof(*adapter->mbx_tx), GFP_KERNEL);
	if (!adapter->mbx_tx)
		return -ENOMEM;

	adapter->mbx_rx = kzalloc(sizeof(*adapter->mbx_rx), GFP_KERNEL);
	if (!adapter->mbx_rx) {
		err = -ENOMEM;
		goto free_mbx_tx;
	}

	adapter->mbx_rx->q_type = GVE_GVE_MBX_Q_TYPE_RX;
	adapter->mbx_tx->q_type = GVE_GVE_MBX_Q_TYPE_TX;
	adapter->mbx_tx->ring_size = GVE_MBX_DEFAULT_RING_SIZE;
	adapter->mbx_rx->ring_size = GVE_MBX_DEFAULT_RING_SIZE;

	err = gve_alloc_mbx_desc_ring(adapter, &adapter->mbx_tx->desc_ring,
				      GVE_MBX_DEFAULT_RING_SIZE);
	if (err)
		goto free_mbx_rx;

	err = gve_alloc_mbx_desc_ring(adapter, &adapter->mbx_rx->desc_ring,
				      GVE_MBX_DEFAULT_RING_SIZE);
	if (err)
		goto free_mbx_tx_desc_ring;

	adapter->mbx_tx_bufs = kcalloc(adapter->mbx_tx->ring_size,
				       sizeof(struct gve_dma_mem *),
				       GFP_KERNEL);
	if (!adapter->mbx_tx_bufs)
		goto free_mbx_rx_desc_ring;

	err = gve_alloc_mbx_rcv_buffers(adapter);
	if (err)
		goto free_mbx_tx_bufs;

	err = gve_alloc_mbx_msg_queue(adapter);
	if (err)
		goto free_mbx_rcv_bufs;

	adapter->gve_mbx_wq = alloc_workqueue("gve-mbx", 0, 0);
	if (!adapter->gve_mbx_wq) {
		err = -ENOMEM;
		dev_err(&adapter->pdev->dev,
			"Failed to allocate mailbox workqueue\n");
		goto free_mbx_msg_queue;
	}

	gve_post_initial_rx_bufs(adapter);
	return 0;

free_mbx_msg_queue:
	gve_free_mbx_msg_queue(adapter);
free_mbx_rcv_bufs:
	gve_free_mbx_rcv_buffers(adapter);
free_mbx_tx_bufs:
	kfree(adapter->mbx_tx_bufs);
free_mbx_rx_desc_ring:
	gve_free_mbx_desc_ring(adapter, adapter->mbx_rx);
free_mbx_tx_desc_ring:
	gve_free_mbx_desc_ring(adapter, adapter->mbx_tx);
free_mbx_rx:
	kfree(adapter->mbx_rx);
	adapter->mbx_rx = NULL;
free_mbx_tx:
	kfree(adapter->mbx_tx);
	adapter->mbx_tx = NULL;
	return err;
}

static int gve_mbx_check_reset_complete(struct gve_adapter *adapter)
{
	void __iomem *reset_status_reg = adapter->reg_bar0 + GVE_MBX_RESET_STATUS;
	u32 reg_val;
	int i;

	for (i = 0; i < 2000; i++) {
		reg_val = ioread32(reset_status_reg);

		if (reg_val != 0xFFFFFFFF && (reg_val & 0x1))
			return 0;

		usleep_range(5000, 10000);
	}

	dev_warn_ratelimited(&adapter->pdev->dev, "Mailbox device reset timeout!");
	return -EBUSY;
}

int gve_mbx_reset(struct gve_adapter *adapter)
{
	void __iomem *reset_reg = adapter->reg_bar0 + GVE_MBX_RESET_CTRL;
	u32 reg_val;

	reg_val = ioread32(reset_reg);

	iowrite32(reg_val | BIT(0), reset_reg);
	return gve_mbx_check_reset_complete(adapter);
}

int gve_initialize_mbx(struct gve_adapter *adapter)
{
	int err;

	err = gve_mbx_reset(adapter);
	if (err) {
		dev_err(&adapter->pdev->dev, "Failed to reset in mailbox mode\n");
		return err;
	}

	if (gve_alloc_mailbox(adapter)) {
		dev_err(&adapter->pdev->dev,
			"Failed to alloc mailbox queues\n");
		return -ENOMEM;
	}

	/* Init the mailbox queues */
	gve_mbx_reg_init(adapter->mbx_tx, adapter->reg_bar0);
	gve_mbx_reg_init(adapter->mbx_rx, adapter->reg_bar0);

	INIT_DELAYED_WORK(&adapter->gve_mbx_task, gve_mbx_task);
	queue_delayed_work(adapter->gve_mbx_wq, &adapter->gve_mbx_task, 0);

	INIT_WORK(&adapter->gve_mbx_lsc_event_task, gve_mbx_lsc_event_task);
	gve_set_mbx_queue_ok(adapter);
	//TODO: log to be removed after debug */
	dev_info(&adapter->pdev->dev, "mailbox queues set up successfully");
	return 0;
}

int gve_mbx_map_db_bar(struct gve_adapter *adapter)
{
	return 0;
}

void gve_mbx_unmap_db_bar(struct gve_adapter *adapter)
{
}
