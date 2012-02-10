/***********************************************************************
 *
 * Copyright (C) 2009 DENX Software Engineering, Heiko Schocher, hs@denx.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 ***********************************************************************/

#ifndef __PDSP1880_DISPLAY_H
#define __PDSP1880_DISPLAY_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <stdint.h>
#endif

/*
 * Ioctl definitions
 */
#define DISPLAY_MAGIC		'D'

#define DISPLAY_SET_BRIGHTNESS		_IOW (DISPLAY_MAGIC, 0, unsigned int *)
#define DISPLAY_SET_CLEAR		_IOW (DISPLAY_MAGIC, 1, void *)

#endif /* __PDSP1880_DISPLAY_H */
