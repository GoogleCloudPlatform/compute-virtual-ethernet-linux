/* SPDX-License-Identifier: (GPL-2.0 OR MIT)
 * Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2015-2018 Google, Inc.
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
