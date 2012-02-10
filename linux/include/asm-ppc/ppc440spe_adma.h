/*
 * 2006-2007 (C) DENX Software Engineering.
 *
 * Author: Yuri Tikhonov <yur@emcraft.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of
 * any kind, whether express or implied.
 */

#ifndef PPC440SPE_ADMA_H
#define PPC440SPE_ADMA_H

#include <linux/types.h>
#include <asm/ppc440spe_dma.h>
#include <asm/ppc440spe_xor.h>

#define SPE_ADMA_SLOT_SIZE      sizeof(struct spe_adma_desc_slot)
#define SPE_ADMA_THRESHOLD      5

#define PPC440SPE_DMA0_ID       0
#define PPC440SPE_DMA1_ID       1
#define PPC440SPE_XOR_ID        2

#define SPE_DESC_INT		(1<<1)
#define SPE_DESC_PROCESSED	(1<<2)

#define SPE_ADMA_XOR_MAX_BYTE_COUNT (1 << 31) /* this is the XOR_CBBCR width */
#define SPE_ADMA_ZERO_SUM_MAX_BYTE_COUNT SPE_ADMA_XOR_MAX_BYTE_COUNT

#undef ADMA_LL_DEBUG

/**
 * struct spe_adma_device - internal representation of an ADMA device
 * @pdev: Platform device
 * @id: HW ADMA Device selector
 * @dma_desc_pool: base of DMA descriptor region (DMA address)
 * @dma_desc_pool_virt: base of DMA descriptor region (CPU address)
 * @common: embedded struct dma_device
 */
struct spe_adma_device {
	struct platform_device *pdev;
	void *dma_desc_pool_virt;

	int id;
	dma_addr_t dma_desc_pool;
	struct dma_device common;
};

/**
 * struct spe_adma_device - internal representation of an ADMA device
 * @lock: serializes enqueue/dequeue operations to the slot pool
 * @device: parent device
 * @chain: device chain view of the descriptors
 * @common: common dmaengine channel object members
 * @all_slots: complete domain of slots usable by the channel
 * @pending: allows batching of hardware operations
 * @result_accumulator: allows zero result sums of buffers > the hw maximum
 * @zero_sum_group: flag to the clean up routine to collect zero sum results
 * @completed_cookie: identifier for the most recently completed operation
 * @slots_allocated: records the actual size of the descriptor slot pool
 */
struct spe_adma_chan {
	spinlock_t lock;
	struct spe_adma_device *device;
	struct timer_list cleanup_watchdog;
	struct list_head chain;
	struct dma_chan common;
	struct list_head all_slots;
	struct spe_adma_desc_slot *last_used;
	int pending;
	u8 result_accumulator;
	u8 zero_sum_group;
	dma_cookie_t completed_cookie;
	int slots_allocated;
};

struct spe_adma_desc_slot {
	dma_addr_t phys;
	struct spe_adma_desc_slot *group_head, *hw_next;
	struct dma_async_tx_descriptor async_tx;
	struct list_head slot_node;
	struct list_head chain_node; /* node in channel ops list */
	struct list_head group_list; /* list */
	unsigned int unmap_len;
	unsigned int unmap_src_cnt;
	dma_cookie_t cookie;
	void *hw_desc;
	u16 stride;
	u16 idx;
	u16 slot_cnt;
	u8 src_cnt;
	u8 slots_per_op;
	unsigned long flags;
	union {
		u32 *xor_check_result;
		u32 *crc32_result;
	};
};

struct spe_adma_platform_data {
	int hw_id;
	unsigned long capabilities;
	size_t pool_size;
};

static u32 xor_refetch = 0;
static struct spe_adma_desc_slot *last_sub[2] = { NULL, NULL };

#ifdef ADMA_LL_DEBUG
static void print_dma_desc (struct spe_adma_desc_slot *desc)
{
	dma_cdb_t *p = desc->hw_desc;

	printk(	"**************************\n"
		"%s: CDB at %p (phys %x)\n"
		"DMA OpCode=0x%x\n"
		"Upper Half of SG1 Address=0x%x\n"
		"Lower Half of SG1 Address=0x%x\n"
		"SG (Scatter/Gather) Count=%x\n"
		"Upper Half of SG2 Address=0x%x\n"
		"Lower Half of SG2 Address=0x%x\n"
		"Upper Half of SG3 Address=0x%x\n"
		"Lower Half of SG3 Address=0x%x\n",
		__FUNCTION__, p, desc->phys,
		cpu_to_le32(p->opc),
		cpu_to_le32(p->sg1u), cpu_to_le32(p->sg1l),
		cpu_to_le32(p->cnt),
		cpu_to_le32(p->sg2u), cpu_to_le32(p->sg2l),
		cpu_to_le32(p->sg3u), cpu_to_le32(p->sg3l)
	);
}


static void print_xor_desc (struct spe_adma_desc_slot *desc)
{
	xor_cb_t *p = desc->hw_desc;
	int i;

	printk( "**************************\n"
		"%s(%p) [phys %x]\n"
		"XOR0_CBCR=%x; XOR0_CBBCR=%x; XOR0_CBSR=%x;\n"
		"XOR0_CBTAH=%x; XOR0_CBTAL=%x; XOR0_CBLAL=%x;\n",
		__FUNCTION__, p, (u32)(desc->phys),
		p->cbc,  p->cbbc, p->cbs,
		p->cbtah, p->cbtal, p->cblal
	);
	for (i=0; i < 16; i++) {
		printk("Operand[%d]=%x; ", i, p->ops[i]);
		if (i && !(i%3))
			printk("\n");
	}
}

static void print_xor_chain (xor_cb_t *p)
{
	int i;

	do {
	        printk( "####### \n"
	                "%s(%p) [phys %x]\n"
        	        "XOR0_CBCR=%x; XOR0_CBBCR=%x; XOR0_CBSR=%x;\n"
                	"XOR0_CBTAH=%x; XOR0_CBTAL=%x; XOR0_CBLAL=%x;\n",
	                __FUNCTION__, p, (u32)__pa(p),
        	        p->cbc,  p->cbbc, p->cbs,
                	p->cbtah, p->cbtal, p->cblal
        	);
        	for (i=0; i < 16; i++) {
	                printk("Operand[%d]=%x; ", i, p->ops[i]);
                	if (i && !(i%3))
        	                printk("\n");
	        }

		if (!p->cblal)
			break;
		p = __va(p->cblal);
	} while (p);
}

static void print_xor_regs (struct spe_adma_chan *spe_chan)
{
       volatile xor_regs_t *p = (xor_regs_t *)spe_chan->device->pdev->resource[0].start;

	printk("------ regs -------- \n");
        printk( "\tcbcr=%x; cbbcr=%x; cbsr=%x;\n"
        	"\tcblalr=%x;crsr=%x;crrr=%x;\n"
                "\tccbalr=%x;ier=%x;sr=%x\n"
                "\tplbr=%x;cbtalr=%x\n"
		"\top1=%x;op2=%x;op3=%x\n",
                in_be32(&p->cbcr), in_be32(&p->cbbcr),in_be32(&p->cbsr),
                in_be32(&p->cblalr),in_be32(&p->crsr),in_be32(&p->crrr),
                in_be32(&p->ccbalr),in_be32(&p->ier),in_be32(&p->sr),
                in_be32(&p->plbr),in_be32(&p->cbtalr),
		p->op_ar[0][1], p->op_ar[1][1], p->op_ar[2][1]);
}
#endif

static inline int spe_chan_interrupt_slot_count (int *slots_per_op, struct spe_adma_chan *chan)
{
	*slots_per_op = 1;
	return *slots_per_op;
}

static inline void spe_desc_init_interrupt (struct spe_adma_desc_slot *desc, struct spe_adma_chan *chan)
{
	xor_cb_t *p;

	switch (chan->device->id) {
	        case PPC440SPE_DMA0_ID:
        	case PPC440SPE_DMA1_ID:
			printk("%s is not supported for chan %d\n", __FUNCTION__, 
				chan->device->id);
	                break;
        	case PPC440SPE_XOR_ID:
			p = desc->hw_desc;
			memset (desc->hw_desc, 0, sizeof(xor_cb_t));
        		p->cbc = XOR_CBCR_CBCE_BIT; /* NOP */
			break;
	}
}

static inline void spe_adma_device_clear_eot_status (struct spe_adma_chan *chan)
{
	volatile dma_regs_t *dma_reg;
	volatile xor_regs_t *xor_reg;
	u32 rv;

	switch (chan->device->id) {
        case PPC440SPE_DMA0_ID:
        case PPC440SPE_DMA1_ID:
		/* read FIFO to ack */
		dma_reg = (dma_regs_t *)chan->device->pdev->resource[0].start;
		rv = le32_to_cpu(dma_reg->csfpl);
		if (!rv) {
			printk ("%s: CSFPL is NULL\n", __FUNCTION__);
		}
		break;
        case PPC440SPE_XOR_ID:
		/* reset status bit to ack*/
		xor_reg = (xor_regs_t *)chan->device->pdev->resource[0].start;
		rv = in_be32(&xor_reg->sr);
		/* clear status */
		out_be32(&xor_reg->sr, rv);

                if (!(xor_reg->sr & XOR_SR_XCP_BIT) && xor_refetch) {
			xor_reg->crsr = XOR_CRSR_RCBE_BIT;
                        xor_refetch = 0;
                }

		break;
	}
}

static inline u32 spe_adma_get_max_xor (void)
{
	return 16;
}

static inline u32 spe_chan_get_current_descriptor(struct spe_adma_chan *chan)
{
	int id = chan->device->id;
	volatile dma_regs_t *dma_reg;
	volatile xor_regs_t *xor_reg;

	switch (id) {
		case PPC440SPE_DMA0_ID:
		case PPC440SPE_DMA1_ID:
			dma_reg = (dma_regs_t *)chan->device->pdev->resource[0].start;
			return (le32_to_cpu(dma_reg->acpl)) & (~0xF);
		case PPC440SPE_XOR_ID:
			xor_reg = (xor_regs_t *)chan->device->pdev->resource[0].start;
			return xor_reg->ccbalr;
		default:
			BUG();
	}
	return 0;
}

static inline void spe_desc_init_null_xor(struct spe_adma_desc_slot *desc,
                               int src_cnt, int unknown_param)
{
	xor_cb_t *hw_desc = desc->hw_desc;

	desc->src_cnt = 0;
	hw_desc->cbc = src_cnt; /* NOP ? */
	hw_desc->cblal = 0;
}

static inline void spe_chan_set_next_descriptor(struct spe_adma_chan *chan,
						struct spe_adma_desc_slot *next_desc)
{
	int id = chan->device->id;
	volatile xor_regs_t *xor_reg;
	unsigned long flags;

	switch (id) {
		case PPC440SPE_XOR_ID:
			xor_reg = (xor_regs_t *)chan->device->pdev->resource[0].start;

			/* Set Link Address and mark that it's valid */
			local_irq_save(flags);
			while (xor_reg->sr & XOR_SR_XCP_BIT);
			xor_reg->cblalr = next_desc->phys;
			local_irq_restore(flags);
			break;
	}
}

static inline int spe_chan_is_busy(struct spe_adma_chan *chan)
{
	int id = chan->device->id, busy;
	volatile xor_regs_t *xor_reg;
	volatile dma_regs_t *dma_reg;

	switch (id) {
		case PPC440SPE_DMA0_ID:
		case PPC440SPE_DMA1_ID:
			dma_reg = (dma_regs_t *)chan->device->pdev->resource[0].start;
			/*  if command FIFO's head and tail pointers are equal - 
			 * channel is free
			 */
			busy = (dma_reg->cpfhp != dma_reg->cpftp) ? 1 : 0;
			break;
		case PPC440SPE_XOR_ID:
			xor_reg = (xor_regs_t *)chan->device->pdev->resource[0].start;
			busy = (xor_reg->sr & XOR_SR_XCP_BIT) ? 1 : 0;
			break;
		default:
			busy = 0;
			BUG();
	}

	return busy;
}

static inline int spe_desc_is_aligned(struct spe_adma_desc_slot *desc,
					int num_slots)
{
	return (desc->idx & (num_slots - 1)) ? 0 : 1;
}

/* to do: support large (i.e. > hw max) buffer sizes */
static inline int spe_chan_memcpy_slot_count(size_t len, int *slots_per_op)
{
	*slots_per_op = 1;
	return 1;
}

static inline int ppc440spe_xor_slot_count(size_t len, int src_cnt,
					int *slots_per_op)
{
	/* Each XOR descriptor provides up to 16 source operands */
	*slots_per_op = (src_cnt + 15)/16;
	return *slots_per_op;
}

static inline int spe_chan_xor_slot_count(size_t len, int src_cnt,
						int *slots_per_op)
{
	/* Number of slots depends on
	 *	- the number of operators
	 *	- the operator width (len)
	 *  the maximum <len> may be 4K since the StripeHead size is PAGE_SIZE, so
	 * if we'll use this driver for RAID purposes only we'll assume this maximum
	 */
	int slot_cnt = ppc440spe_xor_slot_count(len, src_cnt, slots_per_op);

	if (likely(len <= SPE_ADMA_XOR_MAX_BYTE_COUNT))
		return slot_cnt;

	printk("%s: len %d > max %d !!\n", __FUNCTION__, len, SPE_ADMA_XOR_MAX_BYTE_COUNT);
	BUG();
	return slot_cnt;
}

static inline u32 spe_desc_get_dest_addr(struct spe_adma_desc_slot *desc,
					struct spe_adma_chan *chan)
{
	dma_cdb_t *dma_hw_desc;
	xor_cb_t *xor_hw_desc;

	switch (chan->device->id) {
		case PPC440SPE_DMA0_ID:
		case PPC440SPE_DMA1_ID:
			dma_hw_desc = desc->hw_desc;
			return le32_to_cpu(dma_hw_desc->sg2l);
		case PPC440SPE_XOR_ID:
			xor_hw_desc = desc->hw_desc;
			return xor_hw_desc->cbtal;
		default:
			BUG();
	}
	return 0;
}

static inline u32 spe_desc_get_byte_count(struct spe_adma_desc_slot *desc,
					struct spe_adma_chan *chan)
{
        dma_cdb_t *dma_hw_desc;
        xor_cb_t *xor_hw_desc;

	switch (chan->device->id) {
		case PPC440SPE_DMA0_ID:
		case PPC440SPE_DMA1_ID:
			dma_hw_desc = desc->hw_desc;
			return le32_to_cpu(dma_hw_desc->cnt);
		case PPC440SPE_XOR_ID:
			xor_hw_desc = desc->hw_desc;
			return xor_hw_desc->cbbc;
		default:
			BUG();
	}
	return 0;
}

static inline u32 spe_desc_get_src_addr(struct spe_adma_desc_slot *desc,
					struct spe_adma_chan *chan,
					int src_idx)
{
        dma_cdb_t *dma_hw_desc;
        xor_cb_t *xor_hw_desc;

	switch (chan->device->id) {
		case PPC440SPE_DMA0_ID:
		case PPC440SPE_DMA1_ID:
			dma_hw_desc = desc->hw_desc;
			return le32_to_cpu(dma_hw_desc->sg1l);
		case PPC440SPE_XOR_ID:
			xor_hw_desc = desc->hw_desc;
			return xor_hw_desc->ops[src_idx];
		default:
			BUG();
	}
	return 0;
}

static inline void spe_xor_desc_set_src_addr(xor_cb_t *hw_desc,
					int src_idx, dma_addr_t addr)
{
	out_be32(&hw_desc->ops[src_idx], addr);
}

static inline void spe_desc_init_memcpy(struct spe_adma_desc_slot *desc,
				int int_en)
{
	dma_cdb_t *hw_desc = desc->hw_desc;

	memset (desc->hw_desc, 0, sizeof(dma_cdb_t));

	if (int_en)
		desc->flags |= SPE_DESC_INT;
	else
		desc->flags &= ~SPE_DESC_INT;

	desc->src_cnt = 1;
	hw_desc->opc = cpu_to_le32(1<<24);
}

static inline void spe_desc_init_xor(struct spe_adma_desc_slot *desc,
				int src_cnt,
				int int_en)
{
	xor_cb_t *hw_desc;

	memset (desc->hw_desc, 0, sizeof(xor_cb_t));

	desc->src_cnt = src_cnt;
	hw_desc = desc->hw_desc;
	hw_desc->cbc = XOR_CBCR_TGT_BIT | src_cnt;
	if (int_en)
		hw_desc->cbc |= XOR_CBCR_CBCE_BIT;
}

static inline void spe_desc_set_byte_count(struct spe_adma_desc_slot *desc,
					struct spe_adma_chan *chan,
					u32 byte_count)
{
	dma_cdb_t *dma_hw_desc;
	xor_cb_t *xor_hw_desc;

	switch (chan->device->id) {
		case PPC440SPE_DMA0_ID:
		case PPC440SPE_DMA1_ID:
			dma_hw_desc = desc->hw_desc;
			dma_hw_desc->cnt = cpu_to_le32(byte_count);
			break;
		case PPC440SPE_XOR_ID:
			xor_hw_desc = desc->hw_desc;
			xor_hw_desc->cbbc = byte_count;
			break;
		default:
			BUG();
	}
}

static inline void spe_desc_set_dest_addr(struct spe_adma_desc_slot *desc,
					struct spe_adma_chan *chan,
					dma_addr_t addr)
{
	dma_cdb_t *dma_hw_descr;
	xor_cb_t *xor_hw_descr;

	switch (chan->device->id) {
		case PPC440SPE_DMA0_ID:
		case PPC440SPE_DMA1_ID:
			dma_hw_descr = desc->hw_desc;
			dma_hw_descr->sg2l = cpu_to_le32(addr);
			break;
		case PPC440SPE_XOR_ID:
			xor_hw_descr = desc->hw_desc;
			xor_hw_descr->cbtal = addr;
			break;
		default:
			BUG();
	}
}

static inline void spe_desc_set_memcpy_src_addr(struct spe_adma_desc_slot *desc,
					dma_addr_t addr, int slot_cnt,
					int slots_per_op)
{
	dma_cdb_t *hw_desc = desc->hw_desc;
	hw_desc->sg1l = cpu_to_le32(addr);
}

static inline void spe_desc_set_xor_src_addr(struct spe_adma_desc_slot *desc,
					int src_idx, dma_addr_t addr, int slot_cnt,
					int slots_per_op)
{
	xor_cb_t *hw_desc = desc->hw_desc;

	if (unlikely(slot_cnt != 1)) {
		printk("%s: slot cnt = %d !!! \n", __FUNCTION__, slot_cnt);
		BUG();
	}

	hw_desc->ops[src_idx] = addr;
}

static inline void spe_desc_set_next_desc(struct spe_adma_desc_slot *prev_desc,
					struct spe_adma_chan *chan,
					struct spe_adma_desc_slot *next_desc)
{
	volatile xor_cb_t *xor_hw_desc;
	volatile xor_regs_t *xor_reg;
	unsigned long flags;

	if (!prev_desc)
		return;

	prev_desc->hw_next = next_desc;

	switch (chan->device->id) {
	case PPC440SPE_DMA0_ID:
	case PPC440SPE_DMA1_ID:
		break;
	case PPC440SPE_XOR_ID:

                next_desc->flags |= (1<<16);
                next_desc->flags &= ~(1<<17);

		/* bind descriptor to the chain */
		xor_reg = (xor_regs_t *)chan->device->pdev->resource[0].start;

		/* modify link fields */
		local_irq_save(flags);

		xor_hw_desc = next_desc->hw_desc;
		xor_hw_desc->cblal = 0;
		xor_hw_desc->cbc &= ~XOR_CBCR_LNK_BIT;

		xor_hw_desc = prev_desc->hw_desc;
		xor_hw_desc->cbs = 0;
		xor_hw_desc->cblal = next_desc->phys;
		xor_hw_desc->cbc |= XOR_CBCR_LNK_BIT;

		local_irq_restore(flags);

		break;
	default:
		BUG();
	}
}

static inline u32 spe_desc_get_next_desc(struct spe_adma_desc_slot *desc,
					struct spe_adma_chan *chan)
{
	volatile xor_cb_t *xor_hw_desc;

	switch (chan->device->id) {
		case PPC440SPE_DMA0_ID:
		case PPC440SPE_DMA1_ID:
			if (desc->hw_next)
				return desc->hw_next->phys; 
			return 0;
		case PPC440SPE_XOR_ID:
			xor_hw_desc = desc->hw_desc;
			return xor_hw_desc->cblal;
		default:
			BUG();
	}

	return 0;
}

static inline void spe_chan_append(struct spe_adma_chan *chan)
{
        volatile dma_regs_t *dma_reg;
        volatile xor_regs_t *xor_reg;
	struct spe_adma_desc_slot *iter;
	int id = chan->device->id;
	u32 cur_desc;
	unsigned long flags;

	switch (id) {
		case PPC440SPE_DMA0_ID:
		case PPC440SPE_DMA1_ID:
			dma_reg = (dma_regs_t *)chan->device->pdev->resource[0].start;
			cur_desc = spe_chan_get_current_descriptor(chan);
			if (likely(cur_desc)) {
				/* flush descriptors from queue to fifo */
				iter = last_sub[chan->device->id];
				if (!iter->hw_next)
					return;

				local_irq_save(flags);
				list_for_each_entry_continue(iter, &chan->chain, chain_node) {
					cur_desc = iter->phys;
					if (!list_empty(&iter->async_tx.depend_list)) {
						iter->flags |= SPE_DESC_INT;
					}						

					out_le32 (&dma_reg->cpfpl, cur_desc);  
					if (!iter->hw_next)
						break;
				}
				last_sub[chan->device->id] = iter;
				local_irq_restore(flags);
			} else {
				/* first peer */
				cur_desc = chan->last_used->phys;
				last_sub[chan->device->id] = chan->last_used;
				if (!(chan->last_used->flags & SPE_DESC_INT))
					cur_desc |= 1 << 3;
				out_le32 (&dma_reg->cpfpl, cur_desc);
			}
			break;
		case PPC440SPE_XOR_ID:
			xor_reg = (xor_regs_t *)chan->device->pdev->resource[0].start;
			local_irq_save(flags);

			/* update current descriptor and refetch link */
			if (!(xor_reg->sr & XOR_SR_XCP_BIT)) {
				xor_reg->crsr = XOR_CRSR_RCBE_BIT;
			} else {
				xor_refetch = 1;
			}

			local_irq_restore(flags);
			break;
		default:
			BUG();
	}
}

static inline void spe_chan_disable(struct spe_adma_chan *chan)
{
	int id = chan->device->id;
	volatile xor_regs_t *xor_reg;

	switch (id) {
		case PPC440SPE_DMA0_ID:
		case PPC440SPE_DMA1_ID:
			break;
		case PPC440SPE_XOR_ID:
			xor_reg = (xor_regs_t *)chan->device->pdev->resource[0].start;
			xor_reg->crsr = XOR_CRSR_PAUS_BIT;

			break;
		default:
			BUG();
	}
}

static inline void spe_chan_enable(struct spe_adma_chan *chan)
{
	int id = chan->device->id;
	volatile xor_regs_t *xor_reg;
	unsigned long flags;

	switch (id) {
		case PPC440SPE_DMA0_ID:
		case PPC440SPE_DMA1_ID:
			/* always enable, do nothing */
			break;
		case PPC440SPE_XOR_ID:
			/* drain write buffer */
			xor_reg = (xor_regs_t *)chan->device->pdev->resource[0].start;

			local_irq_save(flags);
			xor_reg->crrr = XOR_CRSR_PAUS_BIT;
			/* fetch descriptor pointed in <link> */
			xor_reg->crrr = XOR_CRSR_64BA_BIT;
			xor_reg->crsr = XOR_CRSR_XAE_BIT;
			local_irq_restore(flags);

			break;
		default:
			BUG();
	}
}

#endif /* PPC440SPE_ADMA_H */
