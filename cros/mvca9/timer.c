/*
 * Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

/* Temporary timer implementation */

#include <common.h>

ulong timer_get_us(void)
{
	static ulong base_time;

	/*
	 * We can't implement this properly. Return 0 on the first call and
	 * larger values after that.
	 */
	if (base_time)
		return get_timer(base_time) * 1000;
	base_time = get_timer(0);
	return 0;
}
