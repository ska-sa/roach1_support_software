/*
 *  include/linux/cpld.h
 *
 * Copyright 2008 DENX Software Engineering GmbH
 * Author: Heiko Schocher <hs@denx.de>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef _LINUX_CPLD_H
#define _LINUX_CPLD_H
#include <linux/ioctl.h>

#define TIMER_START	_IOR( 'C', 0x00, int )
#define TIMER_STOP	_IOR( 'C', 0x01, int )
#define TIMER_PRINT	_IOR( 'C', 0x02, int )

#define CPLD_MAJOR	245
#define CPLD_BASE	0xc0000000
#define TIMER_IRQ	12

#endif
