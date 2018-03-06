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

#ifndef _GVE_LINUX_VERSION_H
#define _GVE_LINUX_VERSION_H

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#else
#define KERNEL_VERSION(a,b,c) ((((a) << 16) + (b) << 8) + (c))
#endif
#ifndef UTS_RELEASE
#include <generated/utsrelease.h>
#endif /* UTS_RELEASE */

#ifndef RHEL_RELEASE_CODE
#define RHEL_RELEASE_CODE 0
#endif /* RHEL_RELEASE_CODE */

#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a,b) (((a) << 8) + (b))
#endif /* RHEL_RELEASE_VERSION */

#ifndef UTS_UBUNTU_RELEASE_ABI
#define UTS_UBUNTU_RELEASE_ABI 0
#define UBUNTU_VERSION_CODE 0
#else
#define UBUNTU_VERSION_CODE (((LINUX_VERSION_CODE & ~0xFF) << 8) + (UTS_UBUNTU_RELEASE_ABI))
#endif /* UTS_UBUNTU_RELEASE_ABI */

#define UBUNTU_VERSION(a,b,c,d) ((KERNEL_VERSION(a,b,0) << 8) + (d))

#endif /* _GVE_LINUX_VERSION_H_ */
