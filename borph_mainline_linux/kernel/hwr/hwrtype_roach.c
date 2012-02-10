/*********************************************************************
 * $Id: 
 * File  : hwrtype_roach.c
 * Author: Hayden Kwok-Hay So, Brandon Hamilton
 * Date  : 
 * Description:
 *   Define hwrtype for roach evaluation board
 *********************************************************************/
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>    /* kmalloc/kfree            */
#include <linux/init.h>
#include <asm/uaccess.h>   /* copy_from_user */
#include <linux/ioport.h>  /* request_mem_region */

#include <linux/bof.h>
#include <linux/borph.h>
#define HDEBUG
#define HDBG_NAME "hwrtype_roach"
#ifdef CONFIG_MKD
# define HDBG_LVL mkd_info->dbg_lvl
#else
# define HDBG_LVL 9
#endif
#include <linux/hdebug.h>

#ifdef CONFIG_RESOURCES_64BIT
#define RESOURCE_SUFFIX(a) U64_C(a)
#else
#define RESOURCE_SUFFIX(a) U32_C(a)
#endif

/* Some device specific definitions */
#define ROACH_CPLD_BASE   RESOURCE_SUFFIX(0x1C0000000)
#define ROACH_SMAP_BASE   RESOURCE_SUFFIX(0x1C0100000)
#define ROACH_EBC_BASE    RESOURCE_SUFFIX(0x1D0000000)

#define ROACH_CPLD_LENGTH 0x000000400
#define ROACH_SMAP_LENGTH 0x000100000
#define ROACH_EBC_LENGTH  0x008000000

#define CPLD_SM_STATUS  0x8
#define CPLD_SM_OREGS   0x9
#define CPLD_SM_DATA    0xa
#define CPLD_SM_CTRL    0xb

#define CPLD_SM_DONE    0x2
#define CPLD_SM_INIT_N  0x1

#define SM_INITB_WAIT 100000
#define SM_DONE_WAIT  100000


#define SMAP_READW \
        (readw((void *)(roach_ebc->smap_virt)))

#define SMAP_WRITEW(value) \
	(writew(value, (void *)(roach_ebc->smap_virt)))

#define SMAP_WRITEL(value) \
	(writel(value, (void *)(roach_ebc->smap_virt)))

#define FPGA_READW(offset) \
        (readw((void *)(roach_ebc->ebc_virt + offset)))

#define FPGA_WRITEW(offset, value) \
	(writew(value, (void *)(roach_ebc->ebc_virt + offset)))

#define FPGA_WRITEL(offset, value) \
	(writel(value, (void *)(roach_ebc->ebc_virt + offset)))

/*
#define CPLD_CFG_OUT(offset, value) \
	(out_le32(roach_ebc->cpld_virt + offset, value))

#define CPLD_CFG_IN(offset) \
	(in_le32(roach_ebc->cpld_virt + offset))
*/

#define CPLD_CFG_OUT(offset, value) \
	(iowrite8(value, roach_ebc->cpld_virt + offset))

#define CPLD_CFG_IN(offset) \
	(ioread8(roach_ebc->cpld_virt + offset))

/*
 * A static buffer for data transfer.  It should be expanded to a 
 * kmem_cache when higher performance is needed.  (Right now, there
 * can only be one ioreg performing I/O at a time.
 */
static buf_t* roach_page;
static struct hwr_iobuf* iobuf;

/* Mutex Semaphore to ensure proper access to page buffer */
static DECLARE_MUTEX(roach_mutex);

typedef struct ebc_map {
    resource_size_t cpld_base;    /* Base address of memory region   */
    void *cpld_virt;              /* Virtual base address of region  */
    resource_size_t smap_base;   
    void *smap_virt;
    resource_size_t ebc_base;   
    void *ebc_virt;
} roach_ebc_map_t;

static roach_ebc_map_t* roach_ebc;


/*****************************************************************
 * functions definitions
 *****************************************************************/
static ssize_t roach_send_iobuf (struct hwr_iobuf* iobuf)
{
	/* int packet_length;

        PDEBUG(9, "SEND: Sending iobuf to ROACH [CMD = %d]\n", iobuf->cmd);

	roach_page[0] = iobuf->cmd;
	packet_length = 1;

	switch (iobuf->cmd)
	{
		case PP_CMD_WRITE:
			roach_page[1] = (iobuf->location >> 16) & 0xFF;
			roach_page[2] = (iobuf->location >> 8) & 0xFF;
			roach_page[3] = iobuf->location & 0xFF;
			roach_page[4] = (iobuf->offset >> 24) & 0xFF;
			roach_page[5] = (iobuf->offset >> 16) & 0xFF;
			roach_page[6] = (iobuf->offset >> 8) & 0xFF;
			roach_page[7] = iobuf->offset & 0xFF;
			roach_page[8]  = (iobuf->size >> 24) & 0xFF;
			roach_page[9]  = (iobuf->size >> 16) & 0xFF;
			roach_page[10] = (iobuf->size >> 8) & 0xFF;
			roach_page[11] = iobuf->size & 0xFF;
			packet_length = 12 + iobuf->size;
			break;
		case PP_CMD_READACK:
		case PP_CMD_WRITEACK:
			// Used by mkd thread - still need to implement 
			break;
		case PP_CMD_GREET:		
                       	roach_page[1] = 0xFF;
			roach_page[2] = 0xAA;
			roach_page[3] = 0x55;
			if (iobuf->size > 0) {
				roach_page[4] = (iobuf->size >> 8) & 0xFF;
				roach_page[5] = iobuf->size & 0xFF;
				memmove(roach_page + 6, roach_page + 12, iobuf->size);
				packet_length = 6 + iobuf->size; 
			}
			else {
				packet_length = 4;
			}
			break;
		case PP_CMD_BYE:
			roach_page[1] = 0x55;
			roach_page[2] = 0xAA;
			roach_page[3] = 0xFF;
			packet_length = 4;
			break;
		case PP_CMD_EXIT:
			break;
		default:
			packet_length = 0;
	}

	*/

	unsigned short *src;
	int i;

	PDEBUG(9, "Writing IOREG (size = %d bytes) to location 0x%x\n", iobuf->size, (unsigned int) (roach_ebc->ebc_virt + iobuf->location));

	src = (unsigned short*) iobuf->data;	
	for (i = 0; i < iobuf->size; i +=2)
	{
		FPGA_WRITEW(iobuf->location + i, *src);
		src++; 
	}

	//PDEBUG(9, "Writing to location 0x%x: 0x%x\n", roach_ebc->fpga_virt + iobuf->location, rrd);
	
	return iobuf->size; // HHH
}

static ssize_t roach_recv_iobuf (struct hwr_iobuf* iobuf)
{
	/* PDEBUG(9, "RECV: Sending request iobuf to ROACH [CMD = %d]\n", iobuf->cmd);

	roach_page[0] = iobuf->cmd;

	switch (iobuf->cmd)
	{
		case PP_CMD_READ:
			roach_page[1] = (iobuf->location >> 16) & 0xFF;
			roach_page[2] = (iobuf->location >> 8) & 0xFF;
			roach_page[3] = iobuf->location & 0xFF;
			roach_page[4] = (iobuf->offset >> 24) & 0xFF;
			roach_page[5] = (iobuf->offset >> 16) & 0xFF;
			roach_page[6] = (iobuf->offset >> 8) & 0xFF;
			roach_page[7] = iobuf->offset & 0xFF;
			roach_page[8]  = (iobuf->size >> 24) & 0xFF;
			roach_page[9]  = (iobuf->size >> 16) & 0xFF;
			roach_page[10] = (iobuf->size >> 8) & 0xFF;
			roach_page[11] = iobuf->size & 0xFF;
			break;
		case PP_CMD_READACK:
		case PP_CMD_WRITEACK:
			break;
		default:
			break;
	}
	*/

	unsigned short *dst;
	int i;

	PDEBUG(9, "Reading IOREG (size = %d bytes) from location 0x%x\n", iobuf->size,  (unsigned int) (roach_ebc->ebc_virt + iobuf->location));

	dst = (unsigned short*) iobuf->data;	
	for (i = 0; i < iobuf->size; i +=2)
	{
		*dst = FPGA_READW(iobuf->location + i);
		dst++; 
	}

	//PDEBUG(9, "Reading from location 0x%x: 0x%x\n", roach_ebc->fpga_virt + iobuf->location, wrd);
	
	return iobuf->size;
}

static struct hwr_iobuf* roach_get_iobuf(void)
{
        PDEBUG(9, "Locking IOBUF\n");

	/* If there is different buffer for differ hwr, should
	 * deferentiate them here using *reg* */

	if (down_interruptible(&roach_mutex)) {
	        /* signal received, semaphore not acquired ... */
		return NULL;
	}

	iobuf->cmd = 0;
	iobuf->size = PAGE_SIZE - 12;
	return iobuf; //HHH
}

static ssize_t roach_put_iobuf (struct hwr_iobuf* iobuf)
{
        PDEBUG(9, "Unlocking IOBUF\n");
	up(&roach_mutex);
	return 0; //HHH
}

static int roach_configure(struct hwr_addr* addr, struct file* file, uint32_t offset, uint32_t len)
{
	int i;
	int count;
	int retval = -EIO;
	volatile unsigned short *src;

	PDEBUG(9, "Configuring ROACH fpga %u from (offset %u, len %u) of %s\n",
	       addr->addr, offset, len, file->f_dentry->d_name.name);

	if (addr->addr != 0) {
		PDEBUG(9, "Invalid FPGA #%d for ROACH\n", addr->addr); /* Roach only has 1 HWR */
		goto out; 
	}

	if (down_interruptible(&roach_mutex)) {
	        /* signal received, semaphore not acquired ... */
		goto out;
	}

	/* Disable the init_n output */
	CPLD_CFG_OUT(CPLD_SM_CTRL, 0x00);
	wmb();

	/* Set Write mode, and enable init_n and prog_b pins  */ 
	CPLD_CFG_OUT(CPLD_SM_OREGS, 0x03);
	wmb();

	for (i=0; i < 32; i++) { /* Delay for at least 350ns  */
		/* Set Write mode, and disable init_n and prog_b pins */
		CPLD_CFG_OUT(CPLD_SM_OREGS, 0x00);
	}
	wmb();	
	
	/* Set Write mode, and enable init_n and prog_b pins    */
	CPLD_CFG_OUT(CPLD_SM_OREGS, 0x03);
	wmb();

	/* Poll until init_n is enabled */
	for (i=0; i < SM_INITB_WAIT + 1; i++) {
		if (CPLD_CFG_IN(CPLD_SM_STATUS) & CPLD_SM_INIT_N) {
			break;
		}
		if (i == SM_INITB_WAIT) {
			PDEBUG(9, "SelectMap - Init_n pin has not been asserted\n");
			goto out_free_mutex;
		}
	}
   
	count = 0;

	/* Read bitstream from file and write it out over SelectMap */
	while (len > 0) {
		count = min(PAGE_SIZE, len);
		retval = kernel_read(file, offset, roach_page, count);
		if (retval < 0) {
			goto out_free_mutex;
		}
		if (retval != count) {
			printk("kernel_read returns less than requested...\n");
			count = retval;
		}

		len -= count;
		offset += count;

		i = 0; retval = -EIO;

		src = (unsigned short *)(roach_page);  		
		while(count > 0) {
			SMAP_WRITEW(*src);
			src++;
			count -= 2;
		}
	}

	
	/* Poll until done pin is enabled */
	for (i=0; i < SM_INITB_WAIT + 1; i++) {
    		if (CPLD_CFG_IN(CPLD_SM_STATUS) & CPLD_SM_DONE) {
      			break;
    		}
    		if (i == SM_DONE_WAIT) {
			PDEBUG(9, "SelectMap - Done pin has not been asserted\n");
			goto out_free_mutex;
		}
	}

	PDEBUG(9, "ROACH Virtex-5 configuration completed successfully\n");
	retval = 0;

out_free_mutex:
	up(&roach_mutex);
out:
	return retval;	
}

static int roach_unconfigure(struct hwr_addr* addr)
{
	PDEBUG(9, "Unconfigure ROACH fpga %u\n", addr->addr);
	return 0;
}

struct phyhwr* roach_reserve_hwr(struct hwr_addr* a)
{
	struct phyhwr* ret;
	if (a && a->class != HAC_ROACH) {
                return NULL;
        }
	
	/* safety check */
	if (a->addr >= 1)
		return NULL;

	ret = phyhwrs[a->class][a->addr];
	if (!atomic_inc_and_test(&ret->count)) {
		atomic_dec(&ret->count);
		/* was being used */
		return NULL;
	}
	/* count is now a usage count */
	atomic_inc(&ret->count);
	return ret;
}

void roach_release_hwr(struct hwr_addr* a)
{
	struct phyhwr* hwr;
	if (a && a->class != HAC_ROACH)
		return;

	/* safety check */
	if (a->addr >= 1)
		return;
	
	hwr = phyhwrs[a->class][a->addr];
	if (atomic_dec_and_test(&hwr->count)) {
		hwr->task = NULL;
		atomic_set(&hwr->count, -1);
	}
}

static struct hwr_operations roach_hwr_operations = {
	.configure = roach_configure,
	.unconfigure = roach_unconfigure,
	.reserve_hwr = roach_reserve_hwr,
	.release_hwr = roach_release_hwr,
	.get_iobuf = roach_get_iobuf,
	.put_iobuf = roach_put_iobuf,
	.send_iobuf = roach_send_iobuf,
	.recv_iobuf = roach_recv_iobuf
};

static struct hwrtype hwrtype_roach = {
	name: "roach",
	type: HAC_ROACH,
	count: ATOMIC_INIT(0),
        num_devs: 1,
	hwr_ops: &roach_hwr_operations,
};

static int __init hwrtype_roach_init(void)
{
	int retval = 0;
	int i;

	if ((retval = register_hwrtype(&hwrtype_roach)) < 0) {
		printk("Error registering hwrtype\n");
		goto out;
	} else {
		printk("hwrtype_roach version CVS-$Revision: 1.1 $ registered\n");
	}

	/* initialize hwr */
        atomic_set(&(phyhwrs[HAC_ROACH][0])->count, -1);
	      	
	/* initialize iobuf memory */
	roach_page = (buf_t*)__get_free_page(GFP_KERNEL);
	iobuf = (struct hwr_iobuf*) kmalloc(sizeof(struct hwr_iobuf), GFP_KERNEL);
	if (iobuf) {
		iobuf->data = roach_page + 12;
		iobuf->size = PAGE_SIZE - 12;
	} else {
		printk("failed getting memory for roach iobuf\n");
		retval = -ENOMEM;
		goto out;
	}

	/* initialise EBC IO Memory */
	if (!(roach_ebc = kmalloc(sizeof(roach_ebc_map_t), GFP_KERNEL))) {
		printk("failed getting memory for roach ebc\n");
		retval = -ENOMEM;
		goto out;
	}

	roach_ebc->cpld_base = 0;

	if (!request_mem_region(ROACH_CPLD_BASE, ROACH_CPLD_LENGTH, "roach-cpld")) {
		printk(KERN_ERR "%s: memory range 0x%08llx to 0x%08llx is in use\n",
			        "roach-cpld",
			        (unsigned long long) ROACH_CPLD_BASE,
			        (unsigned long long)(ROACH_CPLD_BASE + ROACH_CPLD_LENGTH));
		goto out_freemem;
	}

	roach_ebc->cpld_base = ROACH_CPLD_BASE;
  printk(KERN_ERR "borph: about to map cpld at 0x%llx\n", ROACH_CPLD_BASE);
	roach_ebc->cpld_virt = ioremap(roach_ebc->cpld_base, ROACH_CPLD_LENGTH);
  if(!roach_ebc->cpld_virt){
    printk(KERN_ERR "borph: unable to map cpld\n");
  }

	PDEBUG(9, "new I/O memory range 0x%08llx to 0x%08llx allocated (virt:0x%p)\n",
		       (unsigned long long) roach_ebc->cpld_base,
		       (unsigned long long) (roach_ebc->cpld_base + ROACH_CPLD_LENGTH),
		       roach_ebc->cpld_virt);

	roach_ebc->smap_base = 0;

	if (!request_mem_region(ROACH_SMAP_BASE, ROACH_SMAP_LENGTH, "roach-selectmap")) {
		printk(KERN_ERR "%s: memory range 0x%08llx to 0x%08llx is in use\n",
			        "roach-selectmap",
			        (unsigned long long) ROACH_SMAP_BASE,
			        (unsigned long long)(ROACH_SMAP_BASE + ROACH_SMAP_LENGTH));
		goto out_freemem;
	}

	roach_ebc->smap_base = ROACH_SMAP_BASE;
	roach_ebc->smap_virt = ioremap(ROACH_SMAP_BASE,ROACH_SMAP_LENGTH);

	PDEBUG(9, "new I/O memory range 0x%08llx to 0x%08llx allocated (virt:0x%p)\n",
		       (unsigned long long) roach_ebc->smap_base,
		       (unsigned long long) (roach_ebc->smap_base + ROACH_SMAP_LENGTH),
		       roach_ebc->smap_virt);

	roach_ebc->ebc_base = 0;

	if (!request_mem_region(ROACH_EBC_BASE, ROACH_EBC_LENGTH, "roach-ebc")) {
		printk(KERN_ERR "%s: memory range 0x%08llx to 0x%08llx is in use\n",
			        "roach-fpga",
			        (unsigned long long) ROACH_EBC_BASE,
			        (unsigned long long)(ROACH_EBC_BASE + ROACH_EBC_LENGTH));
		goto out_freemem;
	}

	roach_ebc->ebc_base = ROACH_EBC_BASE;
	roach_ebc->ebc_virt = ioremap(ROACH_EBC_BASE,ROACH_EBC_LENGTH);

	PDEBUG(9, "new I/O memory range 0x%08llx to 0x%08llx allocated (virt:0x%p)\n",
		       (unsigned long long) roach_ebc->ebc_base,
		       (unsigned long long) (roach_ebc->ebc_base + ROACH_EBC_LENGTH),
		       roach_ebc->ebc_virt);


        // Check for roach CPLD
        PDEBUG(9, "Reading ROACH CPLD: ");
	
	for (i = 0; i < 32; i++)
	  PDEBUG(9, " 0x%02x ", CPLD_CFG_IN(i));
      
        PDEBUG(9, "\n");

	return 0;

out_freemem:
	kfree(roach_ebc);
	roach_ebc = NULL;
out:
	return retval;
}

static void __exit hwrtype_roach_exit(void)
{
	/* free ebc memory used */
	if (roach_ebc) {
		if (roach_ebc->cpld_base) {
			iounmap(roach_ebc->cpld_virt);
			release_mem_region(roach_ebc->cpld_base, ROACH_CPLD_LENGTH);
		}
		kfree(roach_ebc);
		roach_ebc = NULL;
	}

	/* Free iobuf */
	if (iobuf) {
		kfree(iobuf);
	}

	if (roach_page) {
		free_page((unsigned long) roach_page);
	}

	if (unregister_hwrtype(&hwrtype_roach)) {
		printk("Error unregistering hwrtyp\n");
	} else {
		printk("hwrtype_roach CVS-$Revision: 1.1 $ unregistered\n");
	}
}

module_init(hwrtype_roach_init);
module_exit(hwrtype_roach_exit);

MODULE_AUTHOR("Hayden So");
MODULE_DESCRIPTION("Add hwrtype roach to program FPGA on sequoia as hw process");
MODULE_LICENSE("GPL");
