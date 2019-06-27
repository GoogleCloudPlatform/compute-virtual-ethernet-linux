/* SPDX-License-Identifier: (GPL-2.0 OR MIT)
 * Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2015-2019 Google, Inc.
 */

#ifndef _GVE_ASSERT_H_
#define _GVE_ASSERT_H_
#define static_assert(expr, ...) _Static_assert(expr, #expr)
#endif /* _GVE_ASSERT_H_ */
