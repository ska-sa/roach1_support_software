/*
 * Header file for the mucmc52 IO
 *
 * Copyright (C) 2008 hs@denx.de <hs@denx.de>
 * (C) Copyright 2009 Heiko Schocher, DENX <hs@denx.de>
 * adapted to Linux 2.6.31
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
 */

#ifndef MUCMC52_IO_H
#define MUCMC52_IO_H

#define IOTYPE unsigned char

typedef struct {
	IOTYPE	resetstate;
	IOTYPE	taster;
	IOTYPE	drehschalter;
	IOTYPE	flash;
	IOTYPE	jumper;
	IOTYPE	epld_vers;
	IOTYPE	led_h8;
	IOTYPE	led_h7;
	IOTYPE	led_h6;
	IOTYPE	led_h5;
	IOTYPE	led_h4;
	IOTYPE	led_h3;
	IOTYPE	led_h2;
	IOTYPE	led_hb;
	IOTYPE	power1;
	IOTYPE	power2;
	IOTYPE	rs422;
	IOTYPE	cf;
	IOTYPE	local_ibs;
} STATUSINFO;

#endif /* MUCMC52_IO_H */
