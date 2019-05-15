/* SPDX-License-Identifier: (GPL-2.0 OR MIT)
 * Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2015-2019 Google, Inc.
 */

#ifndef _GVE_ASSERT_H_
#define _GVE_ASSERT_H_
#define GVE_ASSERT_SIZE(tag, type, size) \
	static void gve_assert_size_ ## type(void) __attribute__((used)); \
	static inline void gve_assert_size_ ## type(void) \
	{ \
		BUILD_BUG_ON(sizeof(tag type) != (size)); \
	}
#endif /* _GVE_ASSERT_H_ */
