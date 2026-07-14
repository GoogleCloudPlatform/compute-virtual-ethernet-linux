// SPDX-License-Identifier: GPL-2.0 OR MIT
/* Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2025 Google LLC
 */

#ifndef _GVE_MAILBOX_H
#define _GVE_MAILBOX_H

#include <linux/io.h>
#include <linux/kernel.h>

#include "gve_flow_rule.h"

/* Mailbox Queue defines */
#define GVE_MBX_RESET_CTRL		0x0840700C
#define GVE_MBX_RESET_STATUS		0x08407008

#define GVE_MBX_RX_BASE			0x08400000
#define GVE_MBX_TX_BASE			(GVE_MBX_RX_BASE + 0x14)

#define GVE_MBX_RX_LEN_M		GENMASK(12, 0)
#define GVE_MBX_RX_ENABLE_M		BIT(31)
#define GVE_MBX_RX_HEAD_M		GENMASK(12, 0)

#define GVE_MBX_TX_LEN_M		GENMASK(9, 0)
#define GVE_MBX_TX_ENABLE_M		BIT(31)
#define GVE_MBX_TX_HEAD_M		GENMASK(9, 0)

#define GVE_MBX_DEFAULT_RING_SIZE	64
/* Length of msg Q is < mbx Q to allow for async msgs from the device */
#define GVE_MBX_MSG_QUEUE_LEN		48
#define GVE_MBX_COOKIE_INDEX_BITS 6
#define GVE_MBX_COOKIE_INDEX_MASK GENMASK(GVE_MBX_COOKIE_INDEX_BITS - 1, 0)
static_assert(GVE_MBX_MSG_QUEUE_LEN <= (1 << GVE_MBX_COOKIE_INDEX_BITS));

#define GVE_MBX_BUF_SIZE		4096

#define GVE_MBX_FLAG_DD_S		0
#define GVE_MBX_FLAG_ERR_S		2
#define GVE_MBX_FLAG_RD_S               10
#define GVE_MBX_FLAG_BUF_S              12

#define GVE_MBX_FLAG_DD			BIT(GVE_MBX_FLAG_DD_S)	/* 0x1 */
#define GVE_MBX_FLAG_ERR		BIT(GVE_MBX_FLAG_ERR_S) /* 0x4 */
#define GVE_MBX_FLAG_RD                 BIT(GVE_MBX_FLAG_RD_S)  /* 0x400  */
#define GVE_MBX_FLAG_BUF                BIT(GVE_MBX_FLAG_BUF_S) /* 0x1000 */

#define GVE_MBX_DESC(R, i) \
	(&(((struct gve_mbx_desc *)((R)->desc_ring.va))[i]))

#define GVE_MBX_MSG_TIMEOUT	60000

struct gve_adapter;
struct gve_priv;
struct gve_queue_resources;
struct gve_notify_block;

struct gve_dma_mem {
	void *va;
	dma_addr_t pa;
	u16 size;
};

struct gve_mbx_registers {
	/* Lower 6bits are 0 to meet the 64-byte alignment */
	__le32 base_addr_low;
	__le32 base_addr_high;
	/* Max size required by the hw is 1023 */
	__le32 queue_len;
	__le32 queue_head;
	__le32 queue_tail;
};

enum gve_mbx_queue_type {
	GVE_GVE_MBX_Q_TYPE_UNKNOWN,
	GVE_GVE_MBX_Q_TYPE_TX,
	GVE_GVE_MBX_Q_TYPE_RX,
};

struct gve_mbx_queue {
	enum gve_mbx_queue_type q_type;
	struct gve_dma_mem desc_ring;
	u16 buf_size;
	u16 ring_size;
	struct gve_mbx_registers *reg;
	u32 len_mask;
	u32 len_ena_mask;
	u32 head_mask;
	u16 next_to_use;
	u16 next_to_clean;
	u16 next_to_post;
	spinlock_t q_lock; /* mbx q lock */
};

/* Queue which has a list of outstanding commands */
struct gve_mbx_msg_queue {
	unsigned long *msg_queue_map;
	struct gve_mbx_msg **mbx_msgs;
	spinlock_t mbx_msg_q_lock;
	u16 size;
	u32 counter;
};

struct gve_mbx_msg {
	struct completion work;
	u16 sw_cookie;
	int status;
	u32 opcode;
};

enum gve_mbx_status {
	GVE_MBX_STATUS_UNSET				= 0,
	GVE_MBX_STATUS_PASSED				= 1,
	GVE_MBX_STATUS_UNSUPPORTED_ERROR		= 0xFFEF,
	GVE_MBX_STATUS_ABORTED_ERROR			= 0xFFF0,
	GVE_MBX_STATUS_ALREADY_EXISTS_ERROR		= 0xFFF1,
	GVE_MBX_STATUS_CANCELLED_ERROR			= 0xFFF2,
	GVE_MBX_STATUS_DATA_LOSS_ERROR			= 0xFFF3,
	GVE_MBX_STATUS_DEADLINE_EXCEEDED_ERROR		= 0xFFF4,
	GVE_MBX_STATUS_FAILED_PRECONDITION_ERROR	= 0xFFF5,
	GVE_MBX_STATUS_INTERNAL_ERROR			= 0xFFF6,
	GVE_MBX_STATUS_INVALID_ARGUMENT_ERROR		= 0xFFF7,
	GVE_MBX_STATUS_NOT_FOUND_ERROR			= 0xFFF8,
	GVE_MBX_STATUS_OUT_OF_RANGE_ERROR		= 0xFFF9,
	GVE_MBX_STATUS_PERMISSION_DENIED_ERROR		= 0xFFFA,
	GVE_MBX_STATUS_UNAUTHENTICATED_ERROR		= 0xFFFB,
	GVE_MBX_STATUS_RESOURCE_EXHAUSTED_ERROR		= 0xFFFC,
	GVE_MBX_STATUS_UNAVAILABLE_ERROR		= 0xFFFD,
	GVE_MBX_STATUS_UNIMPLEMENTED_ERROR		= 0xFFFE,
	GVE_MBX_STATUS_UNKNOWN_ERROR			= 0xFFFF,
};

enum gve_mbx_opcode {
	GVE_MBX_NEGOTIATE_CAPABILITIES	= 0x6001,
	GVE_MBX_EVENT			= 0x6002,
	GVE_MBX_GET_INFO_FLOW_STEERING	= 0x6003,
	GVE_MBX_GET_INFO_NIC_TSTAMP_REG = 0x6004,
	GVE_MBX_GET_INTERRUPT_DBS	= 0x6005,
	GVE_MBX_GET_PTYPE_MAP		= 0x6006,
	GVE_MBX_REPORT_LINK_STATUS	= 0x6007,
	GVE_MBX_CREATE_TX_QUEUES	= 0x6008,
	GVE_MBX_CREATE_RX_QUEUES        = 0x6009,
	GVE_MBX_ENABLE_TX_QUEUES        = 0x600c,
	GVE_MBX_ENABLE_RX_QUEUES        = 0x600d,
	GVE_MBX_DESTROY_TX_QUEUES	= 0x600e,
	GVE_MBX_DESTROY_RX_QUEUES       = 0x600f,
	GVE_MBX_QUERY_RSS		= 0x6010,
	GVE_MBX_CONFIGURE_RSS		= 0x6011,
	GVE_MBX_QUERY_FLOW_RULE_STATS	= 0x6012,
	GVE_MBX_QUERY_FLOW_RULE_IDS	= 0x6013,
	GVE_MBX_QUERY_FLOW_RULES	= 0x6014,
	GVE_MBX_ADD_FLOW_RULE		= 0x6015,
	GVE_MBX_DEL_FLOW_RULE		= 0x6016,
	GVE_MBX_RESET_FLOW_RULES	= 0x6017,
};

enum gve_mbx_caps {
	GVE_MBX_CAP_DQO_RDA		= BIT(0),
	GVE_MBX_CAP_DQO_QPL		= BIT(1),
	GVE_MBX_CAP_INTERRUPT_SHARING	= BIT(2),
	GVE_MBX_CAP_FLOW_STEERING	= BIT(3),
	GVE_MBX_CAP_NIC_TSTAMP_REG	= BIT(4),
	GVE_MBX_CAP_NIC_TSTAMP_CMD	= BIT(5),
	GVE_MBX_CAP_HW_GRO		= BIT(6),
};

enum gve_mbx_negotiate_caps_msg_version {
	GVE_MBX_CAPS_MSG_V1 = 1,
};

struct gve_mbx_caps_req {
	__le32 msg_version;
	__le32 msg_size;
	__le64 supported_caps;
	u8 os_type; /* 0x01 = Linux */
	u8 driver_major;
	u8 driver_minor;
	u8 driver_sub;
	__le32 os_version_major;
	__le32 os_version_minor;
	__le32 os_version_sub;
	u8 os_version_str[512];
	u8 driver_version_str[64];
};

/* this structure layout cannot be modified,
 * new fields to be only added in the end
 * when bumping msg_version
 */
struct gve_mbx_caps_resp {
	__le32 msg_version;
	__le32 msg_size;
	__le64 negotiated_caps;
	u8 db_bar; /* doorbell BAR no */
	u8 pad[3];
	/* Offset in bytes into db_bar for mbx IRQ doorbell register */
	__le32 mbx_irq_db_offset;
	__le16 mbx_response_timeout_ms;
	__le16 tx_queue_watchdog_timeout_ms;
	__le16 num_msix_vectors;
	__le16 default_tx_queues;
	__le16 default_rx_queues;
	__le16 max_tx_queues;
	__le16 max_rx_queues;
	__le16 max_mtu;
	u8 mac[ETH_ALEN];
	__le16 default_tx_ring_size;
	__le16 default_rx_ring_size;
	__le16 max_tx_ring_size;
	__le16 max_rx_ring_size;
	__le16 min_tx_ring_size;
	__le16 min_rx_ring_size;
	__le16 max_packet_buffer_size;
	__le16 max_header_buffer_size;
	__le16 hash_key_size;
	__le16 hash_lut_size;
};

/**
 * struct gve_mbx_get_info_flow_steering_resp - get flow steering info
 *
 * @max_flow_rules: maximum number of flow rules supported by device.
 */
struct gve_mbx_get_info_flow_steering_resp {
   __le32 max_flow_rules;
};

/**
 * enum gve_mbx_event_id - identifiers for GVE mailbox events
 */
enum gve_mbx_event_id {
	GVE_MBX_EVENT_LINK_STATUS_CHANGE = 1,
};

/**
 * struct gve_mbx_event - message type of asynchronous mailbox event
 *
 * This is the only mailbox message type which is not a response to a mailbox
 * request message.
 *
 * @event_id: identifier for event. Possible values in gve_mbx_event_id.
 */
struct gve_mbx_event {
	__le32 event_id;
};

struct gve_mbx_get_info_nic_tstamp_reg_resp {
	__le64 dev_clk_ns_l_offset;
	__le64 dev_clk_ns_h_offset;
	__le64 sys_clk_ns_l_offset;
	__le64 sys_clk_ns_h_offset;
	__le64 cmd_sync_trigger_offset;
	u8 bar;
	u8 clk_type;
	u8 pad[6];
};

/**
 * struct gve_mbx_report_link_status_resp - response for REPORT_LINK_STATUS
 *
 * Response for link status request containing the link speed and whether the
 * link is up.
 */
struct gve_mbx_report_link_status_resp {
	__le64 link_speed; // in mbps
	u8 link_up;
	u8 pad[7];
};
static_assert(sizeof(struct gve_mbx_report_link_status_resp) == 16);

/**
 * struct gve_mbx_get_interrupt_dbs_req - reqeust for GET_INTERRUPT_DBS
 *
 * Request for getting MSI-X vectors and doorbell registers from device.
 *
 * @start_msix_index: first MSI-X index to retrieve info for.
 * @num_vecs: number of interrupt vectors to retrieve info for.
 */
struct gve_mbx_get_interrupt_dbs_req {
	__le16 start_msix_index;
	__le16 num_vecs;
};
static_assert(sizeof(struct gve_mbx_get_interrupt_dbs_req) == 4);

/**
 * struct gve_mbx_interrupt_db_info - irq and coalescing doorbell information.
 *
 * @irq_db_offset: Offset in bytes from db_bar for this vector's IRQ doorbell
 *	register
 * @irq_coalesc_db_offset: Offset in bytes from db_bar for this vector's
 *	coalescing doorbell register
 */
struct gve_mbx_interrupt_db_info {
	__le32 irq_db_offset;
	__le32 irq_coalesce_db_offset;
};
static_assert(sizeof(struct gve_mbx_interrupt_db_info) == 8);

/**
 * struct gve_mbx_get_interrupt_dbs_resp - response to GET_INTERRUPT_DBS
 * request.
 *
 * There is a 1:1:1 mapping between MSI-X vector, IRQ doorbell, and IRQ
 * coalescing doorbell.
 *
 * @start_msix_index: First MSI-X vector for which to get the doorbell
 *	registers for.
 * @num_vecs: Number of vectors in following payload.
 * @info: Array of interrupt doorbell info, starting at @start_msix_index, with
 *	@num_vecs elements.
 */
struct gve_mbx_get_interrupt_dbs_resp {
	__le16 start_msix_index;
	__le16 num_vecs;
	struct gve_mbx_interrupt_db_info info[];
};

enum gve_mbx_hash_alg {
	GVE_MBX_HASH_ALG_TOEPLITZ = 1,
};

struct gve_mbx_query_rss_resp {
	__le16 hash_types;	// gve_mbx_rss_hash_type
	u8 hash_alg;		// gve_mbx_hash_alg
	u8 reserved;
	__le16 hash_key_size;
	__le16 hash_lut_size;	// in number of elements
	u8 hash_key[256];
	__le32 hash_lut[];
};

struct gve_mbx_configure_rss_req {
	__le16 hash_types;	// gve_mbx_rss_hash_type
	u8 hash_alg;		// gve_mbx_hash_alg
	u8 reserved;
	__le16 hash_key_size;
	__le16 hash_lut_size;	// in number of elements
	u8 hash_key[256];
	__le32 hash_lut[];
};

/**
 * struct gve_mbx_query_flow_rule_ids_req - request for
 * GVE_MBX_QUERY_FLOW_RULE_IDS
 *
 * @starting_rule_id: First rule ID to get rule for. Following rules are
 *	returned in increasing order of rule ID
 * @num_rule_ids: number of rule IDs to request
 */
struct gve_mbx_query_flow_rule_ids_req {
	__le32 starting_rule_id;
	__le32 num_rule_ids;
};

/**
 * struct gve_mbx_query_flow_rule_ids_resp - response to
 * GVE_MBX_QUERY_FLOW_RULE_IDS
 *
 * @num_rule_ids: nubmer of rule ids in response
 * @rule_ids: array of @num_rule_ids IDs for flow rules currently programmed on
 *	the device
 */
struct gve_mbx_query_flow_rule_ids_resp {
	__le32 num_rule_ids;
	__le32 rule_ids[];
};

/**
 * struct gve_mbx_query_flow_rules_req - request for GVE_MBX_QUERY_FLOW_RULES
 *
 * @starting_rule_id: First rule ID to get rule for. Following rules are
 *	returned in increasing order of rule ID
 * @num_rules: number of rules to request
 */
struct gve_mbx_query_flow_rules_req {
	__le32 starting_rule_id;
	__le32 num_rules;
};

/**
 * struct gve_mbx_query_flow_rules_resp - response to GVE_MBX_QUERY_FLOW_RULES
 *
 * @num_rules: number of flow rules contained in respose
 * @rules: flexible array of @num_rules flow rules
 */
struct gve_mbx_query_flow_rules_resp {
	__le32 num_rules;
	struct gve_flow_rule_config rules[];
};

/**
 * struct gve_mbx_query_flow_rule_stats_resp - response to
 * GVE_MBX_QUERY_FLOW_RULE_STATS
 *
 * Returns the max and current number of flow rules programmed on the device.
 *
 * @num_flow_rules: current number of flow rules
 * @max_flow_rules: max number of allowed flow rules
 */
struct gve_mbx_query_flow_rule_stats_resp {
	__le32 num_flow_rules;
	__le32 max_flow_rules;
};

/**
 * struct gve_mbx_add_flow_rule - request for ADD_FLOW_RULE
 *
 * @rule_id: ID of rule to add.
 * @rule: flow spec for rule.
 */
struct gve_mbx_add_flow_rule_req {
	__le32 rule_id;
	struct gve_flow_rule rule;
};


/**
 * struct gve_mbx_del_flow_rule_req - request for DEL_FLOW_RULE
 *
 * @rule_id: ID of rule to delete.
 */
struct gve_mbx_del_flow_rule_req {
	__le32 rule_id;
};



struct gve_mbx_tx_q_info {
	__le32 queue_id;
#define GVE_MBX_NO_INTERRUPT 0xffff
	__le16 msix_index;
	u8 pad1[2];
#define GVE_RAW_ADDRESSING_QPL_ID 0xFFFFFFFF
	__le32 queue_page_list_id;
	u8 pad2[4];
	__le64 tx_ring_addr;
	__le64 tx_comp_ring_addr;
	__le16 tx_ring_size;
	__le16 tx_comp_ring_size;
	u8 pad3[4];
};

struct gve_mbx_create_tx_q_req {
	__le16 num_queues;
	u8 pad[6];
	struct gve_mbx_tx_q_info tx_queues[] __counted_by_le(num_queues);
};

struct gve_mbx_created_tx_q_info {
	__le32 queue_id;
	__le32 tail_db_offset;
};

struct gve_mbx_create_tx_qs_resp {
	__le16 num_queues;
	u8 pad[6];
	struct gve_mbx_created_tx_q_info queues[] __counted_by_le(num_queues);
};

enum gve_mbx_rx_queue_flags {
	GVE_MBX_RX_QUEUE_ENABLE_RSC = BIT(0),
};

struct gve_mbx_rx_q_info {
	__le32 queue_id;
	__le16 msix_index;
	u8 pad[2];
	__le32 queue_page_list_id;
	__le32 flags;
	__le64 rx_desc_ring_addr;
	__le64 rx_data_ring_addr;
	__le16 rx_desc_ring_size;
	__le16 rx_data_ring_size;
	__le16 packet_buffer_size;
	__le16 header_buffer_size;
};

struct gve_mbx_create_rx_qs_req {
	__le16 num_queues;
	u8 pad[6];
	struct gve_mbx_rx_q_info rx_queues[] __counted_by_le(num_queues);
};

struct gve_mbx_created_rx_q_info {
	__le32 queue_id;
	__le32 tail_db_offset;
};

struct gve_mbx_create_rx_qs_resp {
	__le16 num_queues;
	u8 pad[6];
	struct gve_mbx_created_rx_q_info queues[] __counted_by_le(num_queues);
};

struct gve_mbx_disable_qs_req {
	__le16 num_queues;
	__le16 queue_id[] __counted_by_le(num_queues);
};

struct gve_mbx_enable_qs_req {
	__le16 num_queues;
	__le16 queue_id[] __counted_by_le(num_queues);
};

struct gve_mbx_desc {
	__le16 flags;			/* DD bit, extra payload etc */
	__le16 destination;		/* send to CP/HMA 0x0801 */
	__le16 buf_len;			/* 0 when no extra payload, max is 4k */
	union {
		__le16 retval;		/* MBX RX: status of message */
		__le16 pfid_vfid;	/* MBX TX: func_id, 0 for PF */
	};
	__le32 cmd_opcode;
	__le16 cmd_retval;		/* size of the message */
	__le16 reserved1;
	__le32 function_id;
	__le16 reserved2;
	__le16 cmd_cookie;		/* for SW use */
	__le32 addr_high;		/* of the allocated buffer */
	__le32 addr_low;		/* of the allocated buffer */
};

int gve_mbx_negotiate_caps(struct gve_adapter *adapter);
int gve_mbx_get_ptype_map(struct gve_adapter *adapter);
int gve_mbx_request_db_info(struct gve_adapter *adapter);
int gve_mbx_setup_mgmt_irq(struct gve_adapter *adapter);
void gve_mbx_teardown_mgmt_irq(struct gve_adapter *adapter);
void gve_mbx_task(struct work_struct *work);
void gve_mbx_lsc_event_task(struct work_struct *work);
int gve_send_mbx_msg_wait(struct gve_adapter *adapter, u32 opcode, u16 msg_size,
			  u8 *msg);
int gve_send_mbx_msg(struct gve_adapter *adapter, u32 opcode, u16 msg_size,
		     u8 *msg, u16 cookie);
int gve_receive_mbx_msg(struct gve_adapter *adapter);
void gve_free_mailbox(struct gve_adapter *adapter);
int gve_initialize_mbx(struct gve_adapter *adapter);
int gve_mbx_reset(struct gve_adapter *adapter);
int gve_mbx_map_db_bar(struct gve_adapter *adapter);
void gve_mbx_unmap_db_bar(struct gve_adapter *adapter);
int gve_mbx_set_ntfy_blks(struct gve_adapter *adapter);
void gve_mbx_set_num_queues(struct gve_adapter *adapter);
void gve_mbx_get_max_queues(struct gve_adapter *adapter, int *max_tx_queues,
			    int *max_rx_queues);
int gve_mbx_report_link_status(struct gve_adapter *adapter);
int gve_mbx_create_queues(struct gve_adapter *adapter);
int gve_mbx_disable_queues(struct gve_adapter *adapter);
void gve_mbx_write_q_doorbell(struct gve_adapter *adapter,
			      const struct gve_queue_resources *q_resources,
			      u32 val);
void gve_mbx_write_irq_doorbell_dqo(struct gve_adapter *adapter,
				    const struct gve_notify_block *block,
				    u32 val);
int gve_mbx_query_rss(struct gve_adapter *adapter,
		      struct ethtool_rxfh_param *rxfh);
int gve_mbx_configure_rss(struct gve_adapter *adapter,
			  struct ethtool_rxfh_param *rxfh);
int gve_mbx_query_flow_rules(struct gve_adapter *adapter, u16 query_opcode,
			     u32 starting_loc);
int gve_mbx_add_flow_rule(struct gve_adapter *adapter,
			  struct gve_flow_rule *rule, u32 loc);
int gve_mbx_del_flow_rule(struct gve_adapter *adapter, u32 loc);
int gve_mbx_reset_flow_rules(struct gve_adapter *adapter);
#endif /* _GVE_MAILBOX_H */
