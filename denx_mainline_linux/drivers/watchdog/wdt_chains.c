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
 * (C) 2008 Heiko Scocher, hs@denx.de -- ported for 2.6
 *                                       seperated Hardwarelayer, now not
 *                                       only for MPC8xx.
 * (C) 2002 Detlev Zundel, dzu@denx.de -- Added "watchdog chains"
 * (C) 2001 Wolfgang Denk, wd@denx.de -- Cleanup, Modifications for 2.4 kernels
 * (C) 2001 Wolfgang Denk, wd@denx.de -- Adaption for MAX706TESA Watchdog (LWMON)
 * (C) 2001 Steven Hein,  ssh@sgi.com -- Added timeout configuration option
 * (C) 2001 Robert Enzinger, Robert.Enzinger.extern@icn.siemens.de,
 *		-- added ioctl() for dynamic configuration
 *
M* Modul:         wdt_mpc8xx.c
M*
M* Content:       Linux kernel driver for the watchdog.
 *
 */

/*---------------------------- Headerfiles ----------------------------*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>			/* for character devices	*/
#include <linux/miscdevice.h>		/* driver is a misc device	*/
#include <linux/version.h>
#include <linux/init.h>			/* for __initfunc		*/
#include <linux/spinlock.h>		/* for spinlocks                */
#include <linux/list.h>			/* for linked-list macros       */
#include <linux/sched.h>
#include <linux/slab.h>			/* for kmalloc and friends      */
#include <linux/reboot.h>		/* for sys_reboot               */
#include <linux/wdt_chains.h>
#include <asm/uaccess.h>		/* for put_user			*/

/*----------------- Local vars, datatypes and macros ------------------*/
/* #define DEBUG	1	//	*/

#ifdef DEBUG
# define debugk(fmt,args...) printk(fmt ,##args)
#else
# define debugk(fmt,args...)
#endif

#define	WDT_VERSION	"0.6"	/* version of this device driver		*/

#ifdef	CONFIG_CHAIN_WDT_TIMEOUT
#define TIMEOUT_VALUE CONFIG_CHAIN_WDT_TIMEOUT	/* configurable timeout		*/
#else
#define TIMEOUT_VALUE 300	/* reset after five minutes = 300 seconds	*/
#endif

#ifdef CONFIG_CHAIN_WDT_TIMEOUT_OPEN_ONLY
static int timeout_open_only = 1;
#else
static int timeout_open_only = 0;
#endif

/*
 * The shortest watchdog period of all boards is (so far) approx. 1 sec,
 * thus re-trigger watchdog by default every 500 ms = HZ / 2,
 * except for the LWMON board, which needs 50ms = HZ / 20.
 * The default can be overwritten using a boot argument.
 *
 * The external interface is in seconds, but internal all calculation
 * is done in jiffies.
 */
#ifdef	CONFIG_TRIGGER_PERIOD
# define TRIGGER_PERIOD   ( CONFIG_TRIGGER_PERIOD )
#else
# define TRIGGER_PERIOD   ( HZ / 2 )
#endif

typedef struct monitored_chain
{
	struct list_head list;
	unsigned int chainid;
	pid_t pid;
	int escalation;
	unsigned long expires;
	unsigned long timer_count[4];
	int action[4];
	int signal;
} monitored_chain_t;

static struct list_head mon_list;
static spinlock_t mon_lock;	/* lock for the monitored chain list */
static int mon_chains=0;

struct timer_list wd_timer;	/* structure for timer administration		*/
static unsigned long wdt_chain_dummy;

/*
 * Watchdog active? When disabled, it will get re-triggered
 * automatically without timeout, so it appears to be switched off
 * although actually it is still running.
 */
static int enabled = 1;

static unsigned long timer_count = TIMEOUT_VALUE * HZ;	/* remaining time	*/
static unsigned long timer_period= TRIGGER_PERIOD;	/* period to trigger WD	*/

static int device_open = 0;	/* to implement "run while open" mode	*/


/*------------------------- Extern prototypes -------------------------*/
extern inline void sync(void);
extern long sys_reboot(int, int, unsigned int, void *);
/*----------------------- Interface prototypes ------------------------*/
void wdt_chain_handler(unsigned long);
/*------------------------- Local prototypes --------------------------*/
static int wdt_chain_open(struct inode *, struct file *);
static int wdt_chain_release(struct inode *, struct file *);
static ssize_t wdt_chain_read(struct file *, char *, size_t, loff_t *);
static ssize_t wdt_chain_write(struct file *, const char *, size_t, loff_t *);
static int wdt_chain_ioctl(struct inode *, struct file *,
			     unsigned int, unsigned long);

static int register_mon_chain(wdt_chain_param_t *, int);
/*---------------------Kernel interface prototypes --------------------*/
int wdt_chain_register_mon_chain(wdt_chain_param_t *);
int wdt_chain_unregister_mon_chain(unsigned int);
int wdt_chain_reset_mon_chain(int);
static int process_mon_chains(void);
static monitored_chain_t *find_mon_chain_by_chainid(unsigned int);
static void insert_mon_chain(monitored_chain_t *);
#ifdef MODULE
static void free_mon_list(void);
#endif

struct file_operations wdt_chain_ops = {
	owner:			THIS_MODULE,
	open:			wdt_chain_open,
	release:		wdt_chain_release,
	read:			wdt_chain_read,
	write:			wdt_chain_write,
	ioctl:			wdt_chain_ioctl,
};

static struct miscdevice wdt_miscdev =	/* driver is a misc device */
{
	WATCHDOG_MINOR,
	"watchdog",
	&wdt_chain_ops
};

/* ---------------------------------------------------------------------- */
int __init wdt_chain_init(void)
{
	int rc;

	rc = wdt_hwl_init();	
	debugk("WDT_CHAIN HWL init: %d\n", rc);

	if (rc)
		return rc;

	/* register misc device */
	if ((rc = misc_register(&wdt_miscdev)) < 0) {
		printk("%s: failed with %d\n", __FUNCTION__, rc);
		return rc;
	}

	debugk("WDT_CHAIN registered: major=%d minor=%d\n",
		MISC_MAJOR, WATCHDOG_MINOR);

	INIT_LIST_HEAD(&mon_list);
	spin_lock_init(&mon_lock);

	init_timer(&wd_timer);			/* initialize timer-structure... */
	wd_timer.function = wdt_chain_handler;
	wd_timer.data = (unsigned long) &wdt_chain_dummy;
	wd_timer.expires = jiffies + timer_period;

	debugk("%s: watchdog timer initialized - timer_count = %d  period = %d\n",
		__FUNCTION__, timer_count / HZ, HZ / timer_period);

	add_timer(&wd_timer);			/* ...and activate timer */

	debugk("%s: timer activated\n", __FUNCTION__);

	printk("WDT_CHAIN: Software Watchdog Timer version " WDT_VERSION);

	if (enabled) {
		printk(", timeout %ld sec.\n", timer_count / HZ);
	} else {
		printk(" (disabled)\n");
	}
	return 0;
}
fs_initcall(wdt_chain_init);

/***********************************************************************
F* Function:     int __init wdt_chain_setup (char *options) P*A*Z*
 *
P* Parameters:   char *options
P*                - Options to parse
P*
P* Returnvalue:  int
P*                - 0 is always returned
 *
Z* Intention:    Parse the options passed on the linux command line
 *
D* Design:       Haider / wd@denx.de
C* Coding:       Haider / wd@denx.de
V* Verification: wd@denx.de / dzu@denx.de
 ***********************************************************************/
int __init wdt_chain_setup(char *options)
{
	while (options && *options) {
		if (strncmp(options, "off", 3) == 0) {
			options += 3;
			enabled = 0;
			if (*options != ',')
				return 0;
			options++;
		}

		if (strncmp(options, "timeout:", 8) == 0) {
			options += 8;
			if (!*options)
				return 0;

			timer_count = HZ * simple_strtoul(options, &options, 0);
			if (*options != ',')
				return 0;
			options++;
		}

		if (strncmp(options, "period:", 7) == 0) {
			options += 7;
			if (!*options)
				return 0;
			
			timer_period = simple_strtoul(options, &options, 0);
			if (*options != ',')
				return 0;
			options++;
		}
	}

	return 0;
}
__setup ("wdt_chain=", wdt_chain_setup);


/***********************************************************************
F* Function:     static int wdt_chain_open (struct inode *inode,
F*                                           struct file *filp) P*A*Z*
 *
P* Parameters:   struct inode *inode
P*                - Inode of the device file being opened
P*               struct file *file
P*                - Passed by the kernel, but not used
P*
P* Returnvalue:  int - 0 success
P*                    <0 Errorcondition, which can be
P*                     -ENXIO  Watchdog is not enabled
 *
Z* Intention:    This function is called by the kernel when a device file
Z*               for the driver is opened by open(2).
 *
D* Design:       Haider / wd@denx.de
C* Coding:       Haider / wd@denx.de
V* Verification: wd@denx.de / dzu@denx.de
 ***********************************************************************/
static int wdt_chain_open(struct inode *inode, struct file *filp)
{
	debugk("ENTER %s (%p, %p)\n", __FUNCTION__, inode, filp);

	if (!enabled) {			/* user interface disabled */
		return -ENXIO;
	}
	device_open++;			/* increment usage counter */
	debugk("%s: WDT is open\n", __FUNCTION__);
	return 0;
}


/***********************************************************************
F* Function:     static int wdt_chain_release (struct inode *inode,
F*                                              struct file *filp) P*A*Z*
 *
P* Parameters:   struct inode *inode
P*                - Inode of the device file being closed
P*               struct file *file
P*                - Passed by the kernel, but not used
P*
P* Returnvalue:  int
P*                - 0 is always returned
 *
Z* Intention:    This function is called by the kernel when a device file
Z*               of the driver is closed with close(2).
 *
D* Design:       Haider / wd@denx.de
C* Coding:       Haider / wd@denx.de
V* Verification: wd@denx.de / dzu@denx.de
 ***********************************************************************/
static int wdt_chain_release(struct inode *inode, struct file *filp)
{
	debugk("ENTER %s (%p, %p)\n", __FUNCTION__, inode, filp);

	device_open--;			/* decrement usage counter */
	debugk("%s: WDT is closed.\n", __FUNCTION__);

	return 0;
}


/***********************************************************************
F* Function:     static ssize_t wdt_chain_read (struct file *filp, char *buffer,
				size_t length, loff_t *offset) P*A*Z*
 *
P* Parameters:   struct file *file
P*                - Passed by the kernel, pointer to the file structure
P*                  for the device file
P*               char *buf
P*                - Pointer to buffer in userspace
P*               size_t count
P*                - Number of bytes to read
P*               loff_t *ppos
P*                - Offset for the read - ignored.
P*
P* Returnvalue:  int
P*                 - >0 number of bytes read, i.e. 4
P*                   <0 Errorcondition, which can be
P*                    -EINVAL  When trying to read fewer bytes than the
P*                             rest counter occupies, i.e. sizeof(unsigned)
P*                    -EFAULT  A user-provided pointer is invalid
 *
Z* Intention:    Read the rest counter from the device.
 *
D* Design:       Haider / wd@denx.de
C* Coding:       Haider / wd@denx.de
V* Verification: wd@denx.de / dzu@denx.de
 ***********************************************************************/
static ssize_t wdt_chain_read(struct file *filp, char *buffer,
				size_t length,   loff_t *offset)
{
	unsigned int rest_count = timer_count / HZ;
	int rc;

	debugk("ENTER %s (%p, %p, %d, %p)\n",
		__FUNCTION__, filp, buffer, length,  offset);

	if (length < sizeof(rest_count)) {
		debugk("wdt_chain_release/kernel: invalid argument\n");

		return -EINVAL;
	}
	/* copy value into userspace */
	if ((rc = put_user(rest_count, (int *) buffer)))
		return(rc);

	debugk("%s: rest_count=%i\n", __FUNCTION__, rest_count);

	return (sizeof(rest_count));	/* read always exactly 4 bytes */
}


/***********************************************************************
F* Function:     static ssize_t wdt_chain_write (struct file *filp, const char *buffer,
				                  size_t length, loff_t *offset) P*A*Z*
 *
P* Parameters:   struct file *file
P*                - Passed by the kernel, pointer to the file structure
P*                  for the device file
P*               char *buf
P*                - Pointer to buffer in userspace
P*               size_t count
P*                - Number of bytes to write
P*               loff_t *ppos
P*                - Offset for the write - ignored.
P*
P* Returnvalue:  int
P*                 - >0 number of bytes actually written, i.e. 4
P*                   <0 Errorcondition, which can be
P*                    -EINVAL  When trying to write more or less than 4
P*                             bytes, i.e. sizeof(unsigned)
P*                    -EFAULT  A user-provided pointer is invalid
 *
Z* Intention:    Set the rest counter to the value provided by the user
Z*               process interpreted as a number of seconds.
 *
D* Design:       Haider / wd@denx.de
C* Coding:       Haider / wd@denx.de
V* Verification: wd@denx.de / dzu@denx.de
 ***********************************************************************/
static ssize_t wdt_chain_write(struct file *filp, const char *buffer,
				 size_t length, loff_t *offset)
{
	int error;
	unsigned int new_count;

	debugk("ENTER %s (%p, %p, %d, %p)\n",
		__FUNCTION__, filp, buffer, length,  offset);

	if (length != sizeof(new_count)) {
		debugk("%s: invalid length (%d instead of %d)\n",
			__FUNCTION__, length, sizeof(new_count));

		return -EINVAL;
	}

	/* copy count value into kernel space */
	if ((error = get_user(new_count, (int *) buffer)) != 0) {
		debugk("%s: get_user failed: rc=%d\n",
			__FUNCTION__, error);

		return (error);
	}

	timer_count = HZ * new_count;

	return (sizeof(new_count));
}

/***********************************************************************
F* Function:     static int wdt_chain_ioctl (struct inode *node, struct file *filp,
			                      unsigned int cmd, unsigned long arg) P*A*Z*
 *
P* Parameters:   struct inode *inode
P*                - Passed by the kernel, inode of the device file being
P*                  operated on
P*               struct file *file
P*                - Passed by the kernel, but not used
P*               unsigned int cmd
P*                - ioctl command number
P*               unsigned long arg
P*                - Pointer to arguments cast to unsigned long.
P*                  The actual parameter depends on the command, see
P*                  wdt_mpc8xx(4).
P*
P* Returnvalue:  int
P*                 - 0 => success
P*                  <0 Errorcondition, which can be
P*                  -EINTR  the call was interrupted
P*                  -EFAULT the pointer passed as arg is invalid
P*                  -EINVAL a parameter was invalid
 *
Z* Intention:    This is the entry point for the ioctl() commands.
Z*               For a detailed description see the man-page wdt_mpc8xx(4).
 *
D* Design:       Haider / wd@denx.de
C* Coding:       Haider / wd@denx.de
V* Verification: wd@denx.de / dzu@denx.de
 ***********************************************************************/
static int wdt_chain_ioctl(struct inode *node, struct file *filp,
			     unsigned int cmd, unsigned long arg)
{
	wdt_chain_param_t param;
	int chainid;
	int	res;

	switch (cmd) {
	case WDT_CHAIN_OPEN_ONLY:
		timeout_open_only = 1;
		break;
	case WDT_CHAIN_ALWAYS:
		timeout_open_only = 0;
		break;
	case WDT_CHAIN_REGISTER:
		if (copy_from_user(&param, (void *)arg, sizeof (param))) {
			return (-EFAULT);
		}
		res = register_mon_chain(&param, 1);
		return res;
		break;
	case WDT_CHAIN_RESET:
		if (copy_from_user(&chainid, (void *)arg, sizeof (chainid))) {
			return (-EFAULT);
		}
		return(wdt_chain_reset_mon_chain(chainid));
		break;
	case WDT_CHAIN_UNREGISTER:
		if (copy_from_user(&chainid, (void *)arg, sizeof (chainid))) {
			return (-EFAULT);
		}
		return(wdt_chain_unregister_mon_chain(chainid));
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#ifdef MODULE

/***********************************************************************
F* Function:     int init_module (void) P*A*Z*
 *
P* Parameters:   none
P*
P* Returnvalue:  int
P*                - see wdt_chain_init()
 *
Z* Intention:    This is a wrapper function for the module initialization
Z*               simply calling wdt_chain_init().
 *
D* Design:       Haider / wd@denx.de
C* Coding:       Haider / wd@denx.de
V* Verification: wd@denx.de / dzu@denx.de
 ***********************************************************************/
int init_module(void)
{
	debugk("%s: initialize WDT_CHAIN\n", __FUNCTION__);

	return wdt_chain_init();
}


/***********************************************************************
F* Function:     void cleanup_module (void) P*A*Z*
 *
P* Parameters:   none
P*
P* Returnvalue:  none
 *
Z* Intention:    Cleanup and shutdown the driver to allow unloading
Z*               of the module.
 *
D* Design:       Haider / wd@denx.de
C* Coding:       Haider / wd@denx.de
V* Verification: wd@denx.de / dzu@denx.de
 ***********************************************************************/
void cleanup_module(void)
{
	debugk("%s: cleanup WDT_CHAIN\n", __FUNCTION__);

	del_timer(&wd_timer);
	free_mon_list();

	misc_deregister(&wdt_miscdev);
}

#endif	/* MODULE */

/***********************************************************************
F* Function:     void wdt_chain_handler (unsigned long ptr) P*A*Z*
 *
P* Parameters:   unsigned long ptr
P*                - Parameter passed in from the timer invocation, ignored
P*
P* Returnvalue:  none
 *
Z* Intention:    This is the core functionality of the watchdog.  It is
Z*               called from the timer wd_timer and handles the necessary
Z*               processing, including resetting the hardware watchdog.
Z*               When chains are registered, they override the
Z*               default behaviour and are processed in process_mon_chains().
 *
D* Design:       Haider / wd@denx.de
C* Coding:       Haider / wd@denx.de
V* Verification: wd@denx.de / dzu@denx.de
 ***********************************************************************/
void wdt_chain_handler(unsigned long ptr)
{

	debugk("%s: timer_count=%d jiffies active chains: %d\n", __FUNCTION__, timer_count, mon_chains);

	if ((timer_count == 0) && enabled) {	/* try "graceful" shutdown */
		printk ("WDT_CHAIN: Reset system...\n");
		machine_restart(NULL);
	}

	if ((timer_count > 0) || (!enabled)) {

		/* execute WDT service sequence */
		wdt_hwl_reset ();

		wd_timer.expires = jiffies + timer_period;
		add_timer(&wd_timer);	/* ...re-activate timer */

		/*
		 * process the monitor list
		 */
		process_mon_chains();

		/*
		 * don't timeout if disabled
		 */
		if (!enabled)
			return;

		/*
		 * don't timeout if new interface is used or if device
		 * is not opened
		 */
		if ((timeout_open_only && (!device_open)) || mon_chains)
			return;

		/* decrement variable for timer-control */
		if (timer_count > timer_period)
			timer_count -= timer_period;
		else
			timer_count = 0;

		if (timer_count <= 0) {
			printk("WDT_CHAIN: watchdog about to expire\n");
		}
	}

	return;
}

/***********************************************************************
F* Function:     static int register_mon_chain(wdt_chain_param_t *param,
F*                                             int userproc) P*A*Z*
 *
P* Parameters:   wdt_chain_param_t *param
P*                - The parameters for the chain to be registered.
P*                  Unused stages should be cleared with 0's.
P*               int userproc
P*                - Flag whether we are called from user or kernel space
P*
P* Returnvalue:  int
P*                - 0  success
P*                  -EINVAL  invalid parameters
P*                  -ENOMEM  out of memory
 *
Z* Intention:    This is the main interface to register a watchdog chain
Z*               either from userspace throught ioctl() or from kernel
Z*               space through the wrapper function
Z*               wdt_mpc8x_register_mon_chain().
Z*               Re-registering an existing chain is explicitely ok, as
Z*               a restarted process has to go through this.  This
Z*               effectively resets the corresponding chain.
Z*               For a detailed description of the parameters, see the
Z*               wdt_mpc8xx(4) manpage.
 *
D* Design:       dzu@denx.de
C* Coding:       dzu@denx.de
V* Verification: wd@denx.de
 ***********************************************************************/
static int register_mon_chain(wdt_chain_param_t *param, int userproc)
{
	monitored_chain_t *entry;
	int result = 0, i;

	/* Before kmallocing storage we first check the parameters */
	for (i = 0; (i < 3) && (param->timer_count[i]); i++)
		if ((param->action[i] < WDT_CHAIN_ACTION_NO)||
		    (param->action[i] > WDT_CHAIN_ACTION_RESET))
			return(-EINVAL);

	debugk("%s: registering CHAIN monitor\n", __FUNCTION__);

	spin_lock(&mon_lock);

	if ((entry = find_mon_chain_by_chainid(param->chainid)) == NULL) {
		/* New chain-id so allocate list entry */
		entry = (monitored_chain_t *)kmalloc(sizeof(monitored_chain_t), GFP_KERNEL);
		if (entry == NULL) {
			result = -ENOMEM;
			goto out;
		}

		/* Copy request data to internal format */
		if (userproc)
			entry->pid = current->pid;
		else
			entry->pid = 0;
		entry->chainid = param->chainid;
		entry->signal = param->signal;
		for (i=0; i<2 ; i++) {
			if (param->action[i] != 0) {
				entry->timer_count[i] = param->timer_count[i];
				entry->action[i] = param->action[i];
			} else {
				/* Fill with stop entries */
				entry->timer_count[i] = 2;
				entry->action[i] = WDT_CHAIN_ACTION_RESET;
			}
		}
					
		/* This is a final stop entry */
		entry->timer_count[3] = 2;
		entry->action[3] = WDT_CHAIN_RESET;

		/* Initialize internal data */
		entry->escalation = 0;
		entry->expires = jiffies + HZ * entry->timer_count[0];
		insert_mon_chain(entry);
		mon_chains++;
	} else {
		/* Re-registering of active monitor */
		entry->pid=current->pid;
		entry->escalation = 0;
		entry->expires = jiffies + HZ * entry->timer_count[0];
		entry->escalation = 0;
		list_del(&entry->list);
		insert_mon_chain(entry);
	}

 out:
	spin_unlock(&mon_lock);

	return(result);
}

/*
 * The next three functions form the external interface for kernel modules
 */

/***********************************************************************
F* Function:     int wdt_chain_register_mon_chain(wdt_chain_param_t *param) P*A*Z*
 *
P* Parameters:   wdt_chain_param_t *param
P*
P* Returnvalue:  int
P*                - See description of register_mon_chain()
 *
Z* Intention:    This is only a wrapper function around register_mon_chain()
Z*               exported to the kernel name space.  It only augments the
Z*               parameter with a flag informing register_mon_chain() that
Z*               it was called through this interface and not from a
Z*               user space program.
 *
D* Design:       dzu@denx.de
C* Coding:       dzu@denx.de
V* Verification: wd@denx.de
 ***********************************************************************/
int wdt_chain_register_mon_chain(wdt_chain_param_t *param)
{
	return(register_mon_chain(param, 0));
}
EXPORT_SYMBOL(wdt_chain_register_mon_chain);

/***********************************************************************
F* Function:     int wdt_chain_unregister_mon_chain(unsigned int chainid) P*A*Z*
 *
P* Parameters:   unsigned int chainid
P*                - The id of the chain to unregister
P*
P* Returnvalue:  int
P*                - 0 The chain was unregistered successfully
P*                  -EINVAL  The chainid is unknown
 *
Z* Intention:    When the watchdog functionality is no longer needed,
Z*               chains can be unregistered through this call.
Z*               The function is called through the ioctl() mechanism
Z*               or directly from other kernel proper, as it is exported.
 *
D* Design:       dzu@denx.de
C* Coding:       dzu@denx.de
V* Verification: wd@denx.de
 ***********************************************************************/
int wdt_chain_unregister_mon_chain(unsigned int chainid)
{
	monitored_chain_t *entry;

	if ((entry = find_mon_chain_by_chainid(chainid)) == NULL)
		return(-EINVAL);

	debugk("%s: CHAIN unregistering monitor for id %d\n", __FUNCTION__, entry->chainid);

	spin_lock(&mon_lock);

	list_del(&entry->list);
	kfree(entry);
	mon_chains--;

	spin_unlock(&mon_lock);

	return(0);
}
EXPORT_SYMBOL(wdt_chain_unregister_mon_chain);

/***********************************************************************
F* Function:     int wdt_chain_reset_mon_chain(int chainid) P*A*Z*
 *
P* Parameters:   int chainid
P*                - The id of the chain to reset
P*
P* Returnvalue:  int
P*                - 0 The chain was reset suiccessfully
P*                 <0 Errorcondition, which can be
P*                  -EINVAL The supplied id is unknown.
 *
Z* Intention:    This function resets a chain to its initial state.
Z*               The function is called through the ioctl() mechanism
Z*               to reset or trigger a chain or directly from other
Z*               kernel proper, as it is exported.
 *
D* Design:       dzu@denx.de
C* Coding:       dzu@denx.de
V* Verification: wd@denx.de
 ***********************************************************************/
int wdt_chain_reset_mon_chain(int chainid)
{
	monitored_chain_t *entry;
	int result = 0;

	debugk("%s: CHAIN monitor reset for id %d\n", __FUNCTION__, chainid);

	spin_lock(&mon_lock);

	if ((entry = find_mon_chain_by_chainid(chainid)) == NULL) {
		result = -EINVAL;
		goto out;
	}
	entry->escalation = 0;
	entry->expires = jiffies + HZ * entry->timer_count[0];
	list_del(&entry->list);
	insert_mon_chain(entry);

 out:
	spin_unlock(&mon_lock);

	return(result);
}
EXPORT_SYMBOL(wdt_chain_reset_mon_chain);

#ifdef MODULE
/***********************************************************************
F* Function:     static void free_mon_list(void) P*A*Z*
 *
P* Parameters:   none
P*
P* Returnvalue:  none
 *
Z* Intention:    This function frees the entire kmalloc'ed list of
Z*               monitored chains in case the module is unloaded.
 *
D* Design:       dzu@denx.de
C* Coding:       dzu@denx.de
V* Verification: wd@denx.de
 ***********************************************************************/
static void free_mon_list(void)
{
	struct list_head *ptr, *n;
	monitored_chain_t *entry;

	debugk("%s: CHAIN freeing monitor list\n", __FUNCTION__);

	spin_lock(&mon_lock);

	for (ptr = mon_list.next, n = ptr->next; ptr != &mon_list; ptr = n) {
		entry = list_entry(ptr, monitored_chain_t, list);
		kfree(entry);
	}

	spin_unlock(&mon_lock);
}
#endif

/***********************************************************************
F* Function:     static int process_mon_chains(void) P*A*Z*
 *
P* Parameters:   none
P*
P* Returnvalue:  int
P*                - 0 if the function returns at all
 *
Z* Intention:    This is the core function of the chain functionality.
Z*               The list with the monitored chain is processed and
Z*               expired entries handled appropriately by stepping up
Z*               the escalation ladder.  The escalation actions are
Z*               triggered from here.
 *
D* Design:       dzu@denx.de
C* Coding:       dzu@denx.de
V* Verification: wd@denx.de
 ***********************************************************************/
static int process_mon_chains(void)
{
	struct list_head *ptr;
	monitored_chain_t *entry;
	int sig;

	spin_lock(&mon_lock);

	for (ptr = mon_list.next; ptr != &mon_list; ptr = ptr->next) {
		entry = list_entry(ptr, monitored_chain_t, list);
		if (entry->expires <= jiffies) {
			debugk("%s: WDT_CHAIN monitor expired for id %d\n", __FUNCTION__, entry->chainid);
			switch (entry->action[entry->escalation]) {
			case WDT_CHAIN_ACTION_SIGNAL:
				debugk("WDT_CHAIN: sending user signal for key %d...\n", entry->chainid);
				sig = (entry->signal) ? entry->signal : SIGTERM;
				if (entry->pid)
					kill_proc_info(sig, SEND_SIG_PRIV, entry->pid);
				break;

			case WDT_CHAIN_ACTION_KILL:
				debugk("WDT_CHAIN: sending KILL signal for key %d...\n", entry->chainid);
				if (entry->pid) {
					pid_t pid = entry->pid;
					/* Deregister the monitor chain in release ! */
					kill_proc_info(SIGKILL, SEND_SIG_PRIV, pid);
				}
				break;

			case WDT_CHAIN_ACTION_REBOOT:
				spin_unlock(&mon_lock);
				wdt_chain_unregister_mon_chain(entry->chainid);
				printk("WDT_CHAIN: Rebooting system for key %d...\n", entry->chainid);
				sys_reboot(LINUX_REBOOT_MAGIC1,
					   LINUX_REBOOT_MAGIC2,
					   LINUX_REBOOT_CMD_RESTART, NULL);
				break;

			case WDT_CHAIN_ACTION_RESET:
				printk("WDT_CHAIN: Resetting system for key %d...\n", entry->chainid);
				machine_restart(NULL);
				break;
				
			default:
				debugk("WDT_CHAIN: undefined action %d\n", entry->action[entry->escalation]);
				break;
			}
			entry->escalation++;
			entry->expires = jiffies + HZ * 
				entry->timer_count[entry->escalation];
			list_del(&entry->list);
			insert_mon_chain(entry);
		} else
			/* The list is sorted, so we can stop here */
			break;
	}
			
	spin_unlock(&mon_lock);

	return(0);
}

/***********************************************************************
F* Function:     static monitored_chain_t *find_mon_chain_by_chainid(unsigned int id) P*A*Z*
 *
P* Parameters:   unsigned int id
P*                - The ID of the chain to find
P*
P* Returnvalue:  monitored_chain_t *
P*                - The entry for the chain with id id, or NULL if not
P*                  found
 *
Z* Intention:    Find an entry in the list of monitored chains by
Z*               searching for a specified id.
 *
D* Design:       dzu@denx.de
C* Coding:       dzu@denx.de
V* Verification: wd@denx.de
 ***********************************************************************/
static monitored_chain_t *find_mon_chain_by_chainid(unsigned int id) 
{
	struct list_head *ptr;
	monitored_chain_t *entry;
	
	for (ptr = mon_list.next; ptr != &mon_list; ptr = ptr->next) {
		entry = list_entry(ptr, monitored_chain_t, list);
		if (entry->chainid == id)
			return entry;
	}
	return(NULL);
}

/***********************************************************************
F* Function:     static void insert_mon_chain(monitored_chain_t *new) P*A*Z*
 *
P* Parameters:   monitored_chain_t *new
P*                - The entry to insert into the list
P*
P* Returnvalue:  none
 *
Z* Intention:    Insert an entry for a monitor chain at the correct
Z*               position into the module-global list.  Keeping the
Z*               list sorted with respect to the expiratoin avoids
Z*               unneccessary processing.
 *
D* Design:       dzu@denx.de
C* Coding:       dzu@denx.de
V* Verification: wd@denx.de
 ***********************************************************************/
static void insert_mon_chain(monitored_chain_t *new)
{
	struct list_head *ptr;
	monitored_chain_t *entry;
	
	for (ptr = mon_list.next; ptr != &mon_list; ptr = ptr->next) {
		entry = list_entry(ptr, monitored_chain_t, list);
		if (entry->expires >= new->expires) {
			list_add(&new->list, ptr);
			return;
		}
	}
	list_add_tail(&new->list, &mon_list);
}
