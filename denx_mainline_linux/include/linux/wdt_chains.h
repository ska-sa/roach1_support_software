/*
 * (C) Copyright 2000
 * Jörg Haider, SIEMENS AG
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * (C) 2002 Detlev Zundel, dzu@denx.de -- Added "watchdog chains"
 * (C) 2001 Wolfgang Denk, wd@denx.de -- Cleanup, Modifications for 2.4 kernels
 * (C) 2001 Wolfgang Denk, wd@denx.de -- Adaption for MAX706TESA Watchdog (LWMON)
 * (C) 2001 Steven Hein,  ssh@sgi.com -- Added timeout configuration option
 */

/*
 * The purpose of this header file is to provide an interface for the
 * driver of the watchdog chain timer wdt_chain. In essence this interface
 * consists of the three macros WDT_CHAIN_INIT, WDT_CHAIN_SERVICE,
 * WDT_CHAIN_CLOSE, and its functionality is described as follows:
 *
 * WDT_CHAIN_INIT:      opens the driver and initializes the timer to
 *                      300 seconds;
 *
 * WDT_CHAIN_SERVICE:   writes the value defined by the macro
 *                      WDT_CHAIN_DEF_SERVICE_TIME to the variable,
 *                      which serves as a timer counter;
 *
 * WDT_CHAIN_CLOSE:     closes the watchdog driver;
 *
 * Finally there is a macro called WDT_CHAIN_SET_SERVICE_TIME(sec)
 * for altering the value written to the timer counter to a value,
 * which is specified by sec.
 */


#ifndef _wdt_chain_h
#define _wdt_chain_h

typedef struct	wdt_chain_param {
	unsigned chainid;
	unsigned long timer_count[3];
	int action[3];
	int signal;
} wdt_chain_param_t;

/* Constants for the action[] fields */
#define WDT_CHAIN_ACTION_NO	0
#define WDT_CHAIN_ACTION_SIGNAL	1
#define WDT_CHAIN_ACTION_KILL	2
#define WDT_CHAIN_ACTION_REBOOT	3
#define WDT_CHAIN_ACTION_RESET	4

#define	WDT_CHAIN_IOCTL_BASE	'W'

#define WDT_CHAIN_OPEN_ONLY	_IO (WDT_CHAIN_IOCTL_BASE, 0)
#define WDT_CHAIN_ALWAYS	_IO (WDT_CHAIN_IOCTL_BASE, 1)
#define WDT_CHAIN_REGISTER	_IOW(WDT_CHAIN_IOCTL_BASE, 2, wdt_chain_param_t)
#define WDT_CHAIN_RESET		_IOW(WDT_CHAIN_IOCTL_BASE, 3, int)
#define WDT_CHAIN_UNREGISTER	_IOW(WDT_CHAIN_IOCTL_BASE, 4, int)

#ifndef	__KERNEL__

#include <fcntl.h>
#include <unistd.h>
#include <linux/ioctl.h>

#define WDT_CHAIN_DEVICE "/dev/watchdog"
#define WDT_CHAIN_DEF_SERVICE_TIME 300

int wdt_chain_fd;
int wdt_chain_value = WDT_CHAIN_DEF_SERVICE_TIME;

#define WDT_CHAIN_INIT (wdt_chain_fd = open(WDT_CHAIN_DEVICE, O_RDWR, 0))

#define WDT_CHAIN_SET_SERVICE_TIME(sec) wdt_chain_value = (sec);

#define WDT_CHAIN_SERVICE write(wdt_chain_fd, (char *) &wdt_chain_value, sizeof (wdt_chain_value))

#define WDT_CHAIN_CLOSE close(wdt_chain_fd)

#else

extern	int wdt_chain_hwl_start(void);
extern	int wdt_chain_hwl_stop(void);
extern	int wdt_hwl_init(void);
extern	void wdt_hwl_reset(void);

#endif	/* __KERNEL__ */

#endif	/* _wdt_chain_h */
