/* SPDX-License-Identifier: (GPL-2.0 OR MIT)
 * Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2025 Google LLC
 */

#ifndef _GVE_FLOW_RULE_H_
#define _GVE_FLOW_RULE_H_

#include <linux/types.h>

enum gve_adminq_flow_type {
	GVE_FLOW_TYPE_TCPV4,
	GVE_FLOW_TYPE_UDPV4,
	GVE_FLOW_TYPE_SCTPV4,
	GVE_FLOW_TYPE_AHV4,
	GVE_FLOW_TYPE_ESPV4,
	GVE_FLOW_TYPE_TCPV6,
	GVE_FLOW_TYPE_UDPV6,
	GVE_FLOW_TYPE_SCTPV6,
	GVE_FLOW_TYPE_AHV6,
	GVE_FLOW_TYPE_ESPV6,
};

struct gve_flow_spec {
	__be32 src_ip[4];
	__be32 dst_ip[4];
	union {
		struct {
			__be16 src_port;
			__be16 dst_port;
		};
		__be32 spi;
	};
	union {
		u8 tos;
		u8 tclass;
	};
};

struct gve_flow_rule {
	u16 flow_type;
	u16 action; /* RX queue id */
	struct gve_flow_spec key;
	struct gve_flow_spec mask;
};

struct gve_flow_rule_config {
	u32 location;
	struct gve_flow_rule flow_rule;
};

enum gve_flow_rule_query_opcode {
	GVE_FLOW_RULE_QUERY_RULES	= 0,
	GVE_FLOW_RULE_QUERY_IDS		= 1,
	GVE_FLOW_RULE_QUERY_STATS	= 2,
};

#endif
