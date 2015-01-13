/*
 * Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#ifndef CHROMEOS_2X_REFRESH_H_
#define CHROMEOS_2X_REFRESH_H_

/*
 * Enable 2x Refresh Mode
 */
void enable_2x_refresh(void);

/* Exported from drivers/mtd/spi/spi_flash_internal.h */
int spi_flash_cmd(struct spi_slave *spi, uint8_t cmd,
		  void *response, size_t len);

#endif /* CHROMEOS_2X_REFRESH_H_ */
