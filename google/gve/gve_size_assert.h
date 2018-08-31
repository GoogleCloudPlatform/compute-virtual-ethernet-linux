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

#ifndef _GVE_ASSERT_H_
#define _GVE_ASSERT_H_
#define GVE_ASSERT_SIZE(tag, type, size) \
	static void gve_assert_size_ ## type(void) __attribute__((used)); \
	static inline void gve_assert_size_ ## type(void) \
	{ \
		BUILD_BUG_ON(sizeof(tag type) != (size)); \
	}
#endif /* _GVE_ASSERT_H_ */
