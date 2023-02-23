/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2015 Google, Inc
 * (C) Copyright 2023 Dzmitry Sankouski <dsankouski@gmail.com>
 */

#ifndef __cmd_test_h
#define __cmd_test_h

/**
 * vidconsole_get_font_size() - get the current font name and size
 *
 * @dev: vidconsole device
 * @sizep: Place to put the font size (nominal height in pixels)
 * Returns: Current font name
 */
const char *vidconsole_get_font_size(struct udevice *dev, uint *sizep);

#endif
