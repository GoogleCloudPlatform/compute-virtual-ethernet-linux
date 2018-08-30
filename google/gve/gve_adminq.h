/*
 * Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2015-2018 Google, Inc.
 *
 * This software is available to you under a choice of one of two licenses. You
 * may choose to be licensed under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation, and may be copied,
 * distributed, and modified under those terms. See the GNU General Public
 * License for more details. Otherwise you may choose to be licensed under the
 * terms of the MIT license below.
 *
 * --------------------------------------------------------------------------
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 */

#ifndef _GVE_ADMINQ_H
#define _GVE_ADMINQ_H

#include "gve_size_assert.h"

/* Admin queue opcodes */
#define GVE_ADMINQ_DESCRIBE_DEVICE		0x1
#define GVE_ADMINQ_CONFIGURE_DEVICE_RESOURCES	0x2
#define GVE_ADMINQ_REGISTER_PAGE_LIST		0x3
#define GVE_ADMINQ_UNREGISTER_PAGE_LIST		0x4
#define GVE_ADMINQ_CREATE_TX_QUEUE		0x5
#define GVE_ADMINQ_CREATE_RX_QUEUE		0x6
#define GVE_ADMINQ_DESTROY_TX_QUEUE		0x7
#define GVE_ADMINQ_DESTROY_RX_QUEUE		0x8
#define GVE_ADMINQ_DECONFIGURE_DEVICE_RESOURCES	0x9

/* Admin queue status codes */
#define GVE_ADMINQ_COMMAND_UNSET 0x0
#define GVE_ADMINQ_COMMAND_PASSED 0x1
#define GVE_ADMINQ_COMMAND_ABORTED_ERROR 0XFFFFFFF0
#define GVE_ADMINQ_COMMAND_ALREADY_EXISTS_ERROR 0XFFFFFFF1
#define GVE_ADMINQ_COMMAND_CANCELLED_ERROR 0XFFFFFFF2
#define GVE_ADMINQ_COMMAND_DATALOSS_ERROR 0XFFFFFFF3
#define GVE_ADMINQ_COMMAND_DEADLINE_EXCEEDED_ERROR 0xFFFFFFF4
#define GVE_ADMINQ_COMMAND_FAILED_PRECONDITION_ERROR 0xFFFFFFF5
#define GVE_ADMINQ_COMMAND_INTERNAL_ERROR 0XFFFFFFF6
#define GVE_ADMINQ_COMMAND_INVALID_ARGUMENT_ERROR 0xFFFFFFF7
#define GVE_ADMINQ_COMMAND_NOT_FOUND_ERROR 0XFFFFFFF8
#define GVE_ADMINQ_COMMAND_OUT_OF_RANGE_ERROR 0XFFFFFFF9
#define GVE_ADMINQ_COMMAND_PERMISSION_DENIED_ERROR 0xFFFFFFFA
#define GVE_ADMINQ_COMMAND_UNAUTHENTICATED_ERROR 0xFFFFFFFB
#define GVE_ADMINQ_COMMAND_RESOURCE_EXHAUSTED_ERROR 0xFFFFFFFC
#define GVE_ADMINQ_COMMAND_UNAVAILABLE_ERROR 0XFFFFFFFD
#define GVE_ADMINQ_COMMAND_UNIMPLEMENTED_ERROR 0XFFFFFFFE
#define GVE_ADMINQ_COMMAND_UNKNOWN_ERROR 0XFFFFFFFF

#define GVE_MAX_ADMINQ_EVENT_COUNTER_CHECK	100

#define GVE_ADMINQ_DEVICE_DESCRIPTOR_VERSION 1

/* All AdminQ command structs should be naturally packed. The GVE_ASSERT_SIZE
 * calls make sure this is the case at compile time.
 */

struct gve_adminq_describe_device {
	__be64 device_descriptor_addr;
	__be32 device_descriptor_version;
	__be32 available_length;
};
GVE_ASSERT_SIZE(struct, gve_adminq_describe_device, 16);

struct gve_device_descriptor {
	__be64 max_registered_pages;
	__be16 reserved1;
	__be16 tx_queue_entries;
	__be16 rx_queue_entries;
	__be16 default_num_queues;
	__be16 mtu;
	__be16 counters;
	__be16 tx_pages_per_qpl;
	__be16 rx_pages_per_qpl;
	u8  mac[ETH_ALEN];
	__be16 num_device_options;
	__be16 total_length;
	u8  reserved2[6];
};
GVE_ASSERT_SIZE(struct, gve_device_descriptor, 40);

struct device_option {
	__be32 option_id;
	__be32 option_length;
};
GVE_ASSERT_SIZE(struct, device_option, 8);

struct gve_adminq_configure_device_resources {
	__be64 counter_array;
	__be64 irq_db_addr;
	__be32 num_counters;
	__be32 num_irq_dbs;
	__be32 irq_db_stride;
	__be32 ntfy_blk_msix_base_idx;
};
GVE_ASSERT_SIZE(struct, gve_adminq_configure_device_resources, 32);

struct gve_adminq_register_page_list {
	__be32 page_list_id;
	__be32 num_pages;
	__be64 page_address_list_addr;
};
GVE_ASSERT_SIZE(struct, gve_adminq_register_page_list, 16);

struct gve_adminq_unregister_page_list {
	__be32 page_list_id;
};
GVE_ASSERT_SIZE(struct, gve_adminq_unregister_page_list, 4);

struct gve_adminq_create_tx_queue {
	__be32 queue_id;
	__be32 reserved;
	__be64 queue_resources_addr;
	__be64 tx_ring_addr;
	__be32 queue_page_list_id;
	__be32 ntfy_id;
};
GVE_ASSERT_SIZE(struct, gve_adminq_create_tx_queue, 32);

struct gve_adminq_create_rx_queue {
	__be32 queue_id;
	__be32 index;
	__be32 reserved;
	__be32 ntfy_id;
	__be64 queue_resources_addr;
	__be64 rx_desc_ring_addr;
	__be64 rx_data_ring_addr;
	__be32 queue_page_list_id;
};
GVE_ASSERT_SIZE(struct, gve_adminq_create_rx_queue, 48);

/* Queue resources that are shared with the device */
struct gve_queue_resources {
	union {
		struct {
			volatile __be32 db_index;	/* Device -> Guest */
			volatile __be32 counter_index;	/* Device -> Guest */
		};
		u8 reserved[64];
	};
};
GVE_ASSERT_SIZE(struct, gve_queue_resources, 64);

struct gve_adminq_destroy_tx_queue {
	__be32 queue_id;
};
GVE_ASSERT_SIZE(struct, gve_adminq_destroy_tx_queue, 4);

struct gve_adminq_destroy_rx_queue {
	__be32 queue_id;
};
GVE_ASSERT_SIZE(struct, gve_adminq_destroy_rx_queue, 4);

union gve_adminq_command {
	struct {
		__be32 opcode;
		__be32 status;
		union {
			struct gve_adminq_configure_device_resources
						configure_device_resources;
			struct gve_adminq_create_tx_queue create_tx_queue;
			struct gve_adminq_create_rx_queue create_rx_queue;
			struct gve_adminq_destroy_tx_queue destroy_tx_queue;
			struct gve_adminq_destroy_rx_queue destroy_rx_queue;
			struct gve_adminq_describe_device describe_device;
			struct gve_adminq_register_page_list reg_page_list;
			struct gve_adminq_unregister_page_list unreg_page_list;
		};
	};
	u8 reserved[64];
};
GVE_ASSERT_SIZE(union, gve_adminq_command, 64);

int gve_alloc_adminq(struct device *dev, struct gve_priv *priv);
void gve_free_adminq(struct device *dev, struct gve_priv *priv);
int gve_execute_adminq_cmd(struct gve_priv *priv,
			   union gve_adminq_command *cmd_orig);
int gve_adminq_describe_device(struct gve_priv *priv);
int gve_adminq_configure_device_resources(struct gve_priv *priv,
					  dma_addr_t counter_array_bus_addr,
					  int num_counters,
					  dma_addr_t db_array_bus_addr,
					  int num_ntfy_blks);
int gve_adminq_deconfigure_device_resources(struct gve_priv *priv);
int gve_adminq_create_tx_queue(struct gve_priv *priv, u32 queue_id);
int gve_adminq_destroy_tx_queue(struct gve_priv *priv, u32 queue_id);
int gve_adminq_create_rx_queue(struct gve_priv *priv, u32 queue_id);
int gve_adminq_destroy_rx_queue(struct gve_priv *priv, u32 queue_id);
int gve_adminq_register_page_list(struct gve_priv *priv,
				  struct gve_queue_page_list *qpl);
int gve_adminq_unregister_page_list(struct gve_priv *priv, u32 page_list_id);
#endif /* _GVE_ADMINQ_H */
