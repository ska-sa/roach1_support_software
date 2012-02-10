/*
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/pnp.h>
#include <linux/highmem.h>
#include <linux/mmc/host.h>
#include <linux/scatterlist.h>
#include <linux/mmc/mmc.h>
#include <linux/crc7.h>

#include <asm/io.h>
#include <asm/dma.h>

#define DRIVER_NAME "irq_tester"

#define DBG(x...) \
  pr_debug(DRIVER_NAME ": " x)
#define DBGF(f, x...) \
  pr_debug(DRIVER_NAME " [%s()]: " f, __func__ , ##x)

static unsigned int param_foo = 0x666;

struct irq_host
{
  unsigned char src;
  struct tasklet_struct time_tasklet;
  char *cpld_base;
};

struct irq_host host_local;

irqreturn_t test_interrupt(int irq, void *dev_id)
{
   struct irq_host *host = (struct irq_host*)dev_id;
   /* clear existing irqs */
   host->src = in_8((void*)(host_local.cpld_base + 0x1f));
   out_8((void*)(host_local.cpld_base + 0x1f), 0x0);
   tasklet_schedule(&host->time_tasklet);
   return IRQ_HANDLED;
}

static void time_tasklet(unsigned long param)
{
  unsigned char src = ((struct irq_host*)(param))->src;
  printk("irq: src = %d\n", src);
  return;
}
 

/*****************************************************************************\
 *                                                                           *
 * Driver init and remove                                                    *
 *                                                                           *
\*****************************************************************************/



#define ROACH_CPLD_BASE   0x1C0001000
#define ROACH_CPLD_LENGTH 0x000001000
#define PPC_440EPX_IRQ_5  64

static int __devinit irqtest_probe(struct platform_device *dev)
{
  int ret=0;
  if (!request_mem_region(ROACH_CPLD_BASE, ROACH_CPLD_LENGTH, "roach-cpld-mmc")) {
    printk(KERN_ERR "%s: memory range 0x%08x to 0x%08x is in use\n",
                    "roach-cpld",
                    (resource_size_t) ROACH_CPLD_BASE, 
                    (resource_size_t)(ROACH_CPLD_BASE + ROACH_CPLD_LENGTH));                        
    return ENOMEM;
  }        
  host_local.cpld_base = ioremap64(ROACH_CPLD_BASE, ROACH_CPLD_LENGTH);

  if (!(host_local.cpld_base)) {
    release_mem_region(ROACH_CPLD_BASE, ROACH_CPLD_LENGTH);
    return ENOMEM;
  }

  /* clear existing irqs */
  out_8((void*)(host_local.cpld_base + 0x1f), 0);
  /* enable all irqs */
  out_8((void*)(host_local.cpld_base + 0x1e), 0xf);

  tasklet_init(&(host_local.time_tasklet), time_tasklet, (unsigned long)(&host_local));
  ret=request_irq(PPC_440EPX_IRQ_5, test_interrupt, 0, DRIVER_NAME, (void*)(&host_local));

  if (ret)
    return ret;

  return 0;
}

static int __devexit irqtest_remove(struct platform_device *dev)
{
  free_irq(PPC_440EPX_IRQ_5, (void*)(&host_local));
  tasklet_kill(&(host_local.time_tasklet));
  iounmap(host_local.cpld_base);
  release_mem_region(ROACH_CPLD_BASE, ROACH_CPLD_LENGTH);
  return 0;
}

static struct platform_device *irqtest_device;

static struct platform_driver irqtest_driver = {
  .probe    = irqtest_probe,
  .remove    = __devexit_p(irqtest_remove),

  .suspend  = NULL,
  .resume    = NULL,
  .driver    = {
    .name  = DRIVER_NAME,
    .owner  = THIS_MODULE,
  },
};

/*
 * Module loading/unloading
 */

static int __init irqtest_drv_init(void)
{
  int result;

  printk(KERN_INFO DRIVER_NAME ": irq tester\n");

  result = platform_driver_register(&irqtest_driver);
  if (result < 0)
    return result;

  irqtest_device = platform_device_alloc(DRIVER_NAME, -1);
  if (!irqtest_device) {
    platform_driver_unregister(&irqtest_driver);
    return -ENOMEM;
  }

  result = platform_device_add(irqtest_device);
  if (result) {
    platform_device_put(irqtest_device);
    platform_driver_unregister(&irqtest_driver);
    return result;
  }

  return 0;
}

static void __exit irqtest_drv_exit(void)
{
  platform_device_unregister(irqtest_device);
  platform_driver_unregister(&irqtest_driver);
  DBG("unloaded\n");
}

module_init(irqtest_drv_init);
module_exit(irqtest_drv_exit);
module_param_named(foo, param_foo, uint, 0444);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shanly Rajan and David George");
MODULE_DESCRIPTION("IRQ Tester");

MODULE_PARM_DESC(foo, "A dummy parameter");
