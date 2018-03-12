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

#ifndef _GVE_REGISTER_H_
#define _GVE_REGISTER_H_

/* Fixed Configuration Registers */
#define GVE_REG_BASE 0x0
#define GVE_REG_END 0x20

#define GVE_DEVICE_STATUS_OFFSET 0x0
#define GVE_DEVICE_STATUS (GVE_REG_BASE + GVE_DEVICE_STATUS_OFFSET)
#define GVE_DEVICE_STATUS_RESET_MASK BIT(1)

#define GVE_DRIVER_STATUS_OFFSET 0x4
#define GVE_DRIVER_STATUS (GVE_REG_BASE + GVE_DRIVER_STATUS_OFFSET)

#define GVE_DEVICE_MAX_TX_QUEUES_OFFSET 0x8
#define GVE_DEVICE_MAX_TX_QUEUES \
	(GVE_REG_BASE + GVE_DEVICE_MAX_TX_QUEUES_OFFSET)

#define GVE_DEVICE_MAX_RX_QUEUES_OFFSET 0xC
#define GVE_DEVICE_MAX_RX_QUEUES \
	(GVE_REG_BASE + GVE_DEVICE_MAX_RX_QUEUES_OFFSET)

#define GVE_ADMIN_QUEUE_PFN_OFFSET 0x10
#define GVE_ADMIN_QUEUE_PFN (GVE_REG_BASE + GVE_ADMIN_QUEUE_PFN_OFFSET)

#define GVE_ADMIN_QUEUE_DOORBELL_OFFSET 0x14
#define GVE_ADMIN_QUEUE_DOORBELL \
	(GVE_REG_BASE + GVE_ADMIN_QUEUE_DOORBELL_OFFSET)

#define GVE_ADMIN_QUEUE_EVENT_COUNTER_OFFSET 0x18
#define GVE_ADMIN_QUEUE_EVENT_COUNTER \
	(GVE_REG_BASE + GVE_ADMIN_QUEUE_EVENT_COUNTER_OFFSET)

#define GVE_DRIVER_VERSION_END_OFFSET 0x1
#define GVE_DRIVER_VERSION (GVE_REG_END - GVE_DRIVER_VERSION_END_OFFSET)
#endif /* _GVE_REGISTER_H_ */
