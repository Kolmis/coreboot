/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2014 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef ONBOARD_H
#define ONBOARD_H

#define BOARD_TRACKPAD_NAME		"trackpad"
#define BOARD_TRACKPAD_IRQ		37	/* PIRQV */
#define BOARD_TRACKPAD_WAKE_GPIO	12	/* GPIO12 */
#define BOARD_TRACKPAD_I2C_BUS		1	/* I2C0 */
#define BOARD_TRACKPAD_I2C_ADDR		0x67
#define BOARD_TRACKPAD_GEN5_I2C_ADDR	0x24

#endif
