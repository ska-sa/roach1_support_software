/*
 * Copyright(c) 2006 DENX Engineering. All rights reserved.
 *
 * Author: Yuri Tikhonov <yur@emcraft.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called COPYING.
 */

/*
 *  This driver supports the asynchrounous DMA copy and RAID engines available
 * on the AMCC PPC440SPe Processors.
 *  Based on the Intel Xscale(R) family of I/O Processors (SPE 32x, 33x, 134x)
 * ADMA driver.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/async_tx.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <asm/ppc440spe_adma.h>

#define to_spe_adma_chan(chan) container_of(chan, struct spe_adma_chan, common)
#define to_spe_adma_device(dev) container_of(dev, struct spe_adma_device, common)
#define to_spe_adma_slot(lh) container_of(lh, struct spe_adma_desc_slot, slot_node)
#define tx_to_spe_adma_slot(tx) container_of(tx, struct spe_adma_desc_slot, async_tx)

#define SPE_ADMA_MAX_BYTE_COUNT		0xFFFFFF

#define SPE_ADMA_DEBUG 0
#define PRINTK(x...) ((void)(SPE_ADMA_DEBUG && printk(x)))

/**
 * spe_adma_free_slots - flags descriptor slots for reuse
 * @slot: Slot to free
 * Caller must hold &spe_chan->lock while calling this function
 */
static inline void spe_adma_free_slots(struct spe_adma_desc_slot *slot)
{
	int stride = slot->stride;

	while (stride--) {
		slot->stride = 0;
		slot = list_entry(slot->slot_node.next,
				struct spe_adma_desc_slot,
				slot_node);
	}
}

static inline dma_cookie_t
spe_adma_run_tx_complete_actions(struct spe_adma_desc_slot *desc,
	struct spe_adma_chan *spe_chan, dma_cookie_t cookie)
{

	BUG_ON(desc->async_tx.cookie < 0);

	if (desc->async_tx.cookie > 0) {
		cookie = desc->async_tx.cookie;
		desc->async_tx.cookie = 0; 

		/* call the callback (must not sleep or submit new
		 * operations to this channel)
		 */
		if (desc->async_tx.callback)
			desc->async_tx.callback(
				desc->async_tx.callback_param);

		/* unmap dma addresses
		 * (unmap_single vs unmap_page?)
		 */
		if (desc->group_head && desc->async_tx.type != DMA_INTERRUPT) {
			struct spe_adma_desc_slot *unmap = desc->group_head;
			u32 src_cnt = unmap->unmap_src_cnt;
			dma_addr_t addr = spe_desc_get_dest_addr(unmap,
				spe_chan);

			dma_unmap_page(&spe_chan->device->pdev->dev, addr, 
					unmap->unmap_len, DMA_FROM_DEVICE);
			while(src_cnt--) {
				addr = spe_desc_get_src_addr(unmap,
							spe_chan,
							src_cnt);
				dma_unmap_page(&spe_chan->device->pdev->dev, addr, 
					unmap->unmap_len, DMA_TO_DEVICE);
			}
			desc->group_head = NULL;
		}
	}

	/* run dependent operations */
	async_tx_run_dependencies(&desc->async_tx, &spe_chan->common);

	return cookie;
}

static inline int
spe_adma_clean_slot(struct spe_adma_desc_slot *desc,
	struct spe_adma_chan *spe_chan)
{
	/* the client is allowed to attach dependent operations
	 * until 'ack' is set
	 */
	if (!desc->async_tx.ack)
		return 0;

	/* leave the last descriptor in the chain
	 * so we can append to it
	 */
	if (desc->chain_node.next == &spe_chan->chain || 
			desc->phys == spe_chan_get_current_descriptor(spe_chan))
		return 1;

	PRINTK("\tfree slot %x: %d stride: %d\n", desc->phys, desc->idx, desc->stride);

	list_del(&desc->chain_node);

	spe_adma_free_slots(desc);

	return 0;
}

int spe_check_stride (struct dma_async_tx_descriptor *tx)
{
	struct spe_adma_desc_slot *p = tx_to_spe_adma_slot(tx);

	return p->stride;
}

static void __spe_adma_slot_cleanup(struct spe_adma_chan *spe_chan)
{
	struct spe_adma_desc_slot *iter, *_iter, *group_start = NULL;
	dma_cookie_t cookie = 0;
	u32 current_desc = spe_chan_get_current_descriptor(spe_chan);
	int busy = spe_chan_is_busy(spe_chan);
	int seen_current = 0, slot_cnt = 0, slots_per_op = 0;

	PRINTK ("spe adma%d: %s\n", spe_chan->device->id, __FUNCTION__);

	/* free completed slots from the chain starting with
	 * the oldest descriptor
	 */
	list_for_each_entry_safe(iter, _iter, &spe_chan->chain,
					chain_node) {
		PRINTK ("\tcookie: %d slot: %d busy: %d "
			"this_desc: %#x next_desc: %#x cur: %#x ack: %d\n",
			iter->async_tx.cookie, iter->idx, busy, iter->phys,
			spe_desc_get_next_desc(iter, spe_chan),
			current_desc,
			iter->async_tx.ack);

		/* do not advance past the current descriptor loaded into the
		 * hardware channel, subsequent descriptors are either in process
		 * or have not been submitted
		 */
		if (seen_current)
			break;

		/* stop the search if we reach the current descriptor and the
		 * channel is busy, or if it appears that the current descriptor
		 * needs to be re-read (i.e. has been appended to)
		 */
		if (iter->phys == current_desc) {
			BUG_ON(seen_current++);
			if (busy || spe_desc_get_next_desc(iter, spe_chan)) {
				spe_adma_run_tx_complete_actions(iter, spe_chan, cookie);
				break;
			}
		}

		/* detect the start of a group transaction */
		if (!slot_cnt && !slots_per_op) {
			slot_cnt = iter->slot_cnt;
			slots_per_op = iter->slots_per_op;
			if (slot_cnt <= slots_per_op) {
				slot_cnt = 0;
				slots_per_op = 0;
			}
		}

		if (slot_cnt) {
			PRINTK("\tgroup++\n");
			if (!group_start)
				group_start = iter;
			slot_cnt -= slots_per_op;
		}

		/* all the members of a group are complete */
		if (slots_per_op != 0 && slot_cnt == 0) {
			struct spe_adma_desc_slot *grp_iter, *_grp_iter;
			int end_of_chain = 0;
			PRINTK("\tgroup end\n");

			/* collect the total results */
			if (group_start->xor_check_result) {
				u32 zero_sum_result = 0;
				slot_cnt = group_start->slot_cnt;
				grp_iter = group_start;

				list_for_each_entry_from(grp_iter,
					&spe_chan->chain, chain_node) {
					PRINTK("\titer%d result: %d\n", grp_iter->idx,
						zero_sum_result);
					slot_cnt -= slots_per_op;
					if (slot_cnt == 0)
						break;
				}
				PRINTK("\tgroup_start->xor_check_result: %p\n",
					group_start->xor_check_result);
				*group_start->xor_check_result = zero_sum_result;
			}

			/* clean up the group */
			slot_cnt = group_start->slot_cnt;
			grp_iter = group_start;
			list_for_each_entry_safe_from(grp_iter, _grp_iter,
				&spe_chan->chain, chain_node) {

				cookie = spe_adma_run_tx_complete_actions(
					grp_iter, spe_chan, cookie);

				slot_cnt -= slots_per_op;
				end_of_chain = spe_adma_clean_slot(grp_iter,
					spe_chan);

				if (slot_cnt == 0 || end_of_chain)
					break;
			}

			/* the group should be complete at this point */
			BUG_ON(slot_cnt);

			slots_per_op = 0;
			group_start = NULL;
			if (end_of_chain)
				break;
			else
				continue;
		} else if (slots_per_op) /* wait for group completion */
			continue;

		cookie = spe_adma_run_tx_complete_actions(iter, spe_chan, cookie);

		if (spe_adma_clean_slot(iter, spe_chan))
			break;
	}

	if (!seen_current) {
		BUG();
	}

	if (cookie > 0) {
		spe_chan->completed_cookie = cookie;
		PRINTK("\tcompleted cookie %d\n", cookie);
	}
}

static inline void
spe_adma_slot_cleanup(struct spe_adma_chan *spe_chan)
{
	spin_lock_bh(&spe_chan->lock);
	__spe_adma_slot_cleanup(spe_chan);
	spin_unlock_bh(&spe_chan->lock);
}

static struct spe_adma_chan *spe_adma_chan_array[3];
static void spe_adma0_task(unsigned long data)
{
	__spe_adma_slot_cleanup(spe_adma_chan_array[0]);
}

static void spe_adma1_task(unsigned long data)
{
	__spe_adma_slot_cleanup(spe_adma_chan_array[1]);
}

static void spe_adma2_task(unsigned long data)
{
	__spe_adma_slot_cleanup(spe_adma_chan_array[2]);
}

DECLARE_TASKLET(spe_adma0_tasklet, spe_adma0_task, 0);
DECLARE_TASKLET(spe_adma1_tasklet, spe_adma1_task, 0);
DECLARE_TASKLET(spe_adma2_tasklet, spe_adma2_task, 0);
struct tasklet_struct *spe_adma_tasklet[] = {
	&spe_adma0_tasklet,
	&spe_adma1_tasklet,
	&spe_adma2_tasklet,
};

static struct spe_adma_desc_slot *
__spe_adma_alloc_slots(struct spe_adma_chan *spe_chan, int num_slots,
			int slots_per_op, int recurse)
{
	struct spe_adma_desc_slot *iter = NULL, *alloc_start = NULL;
	struct spe_adma_desc_slot *last_used = NULL, *last_op_head = NULL;
	struct list_head chain = LIST_HEAD_INIT(chain);
	int i;

	/* start search from the last allocated descrtiptor
	 * if a contiguous allocation can not be found start searching
	 * from the beginning of the list
	 */

	for (i = 0; i < 2; i++) {
		int slots_found = 0;
		if (i == 0)
			iter = spe_chan->last_used;
		else {
			iter = list_entry(&spe_chan->all_slots,
				struct spe_adma_desc_slot,
				slot_node);
		}

		list_for_each_entry_continue(iter, &spe_chan->all_slots, slot_node) {
			if (iter->stride) {
				/* give up after finding the first busy slot
				 * on the second pass through the list
				 */
				if (i == 1)
					break;

				slots_found = 0;
				continue;
			}

			/* start the allocation if the slot is correctly aligned */
			if (!slots_found++) {
				if (spe_desc_is_aligned(iter, slots_per_op))
					alloc_start = iter;
				else {
					slots_found = 0;
					continue;
				}
			}

			if (slots_found == num_slots) {
				iter = alloc_start;
				i = 0;
				while (num_slots) {

					/* pre-ack all but the last descriptor */
					if (num_slots != slots_per_op)
						iter->async_tx.ack = 1;
					else
						iter->async_tx.ack = 0;

                                        PRINTK ("spe adma%d: allocated slot: %d "
                                                "(desc %p phys: %#x) stride %d"
						",ack = %d\n",
                                                spe_chan->device->id,
                                                iter->idx, iter->hw_desc, iter->phys,
                                                slots_per_op, iter->async_tx.ack);

					list_add_tail(&iter->chain_node, &chain);
					last_op_head = iter;
					iter->async_tx.cookie = 0;
					iter->hw_next = NULL;
					iter->flags = 0;
					iter->slot_cnt = num_slots;
					iter->slots_per_op = slots_per_op;
					iter->xor_check_result = NULL;
					for (i = 0; i < slots_per_op; i++) {
						iter->stride = slots_per_op - i;
						last_used = iter;
						iter = list_entry(iter->slot_node.next,
								struct spe_adma_desc_slot,
								slot_node);
					}
					num_slots -= slots_per_op;
				}
				last_op_head->group_head = alloc_start;
				last_op_head->async_tx.cookie = -EBUSY;
				list_splice(&chain, &last_op_head->group_list);
				spe_chan->last_used = last_used;
				return last_op_head;
			}
		}
	}

	/* try to free some slots if the allocation fails */
	tasklet_schedule(spe_adma_tasklet[spe_chan->device->id]);
	return NULL;
}

static struct spe_adma_desc_slot *
spe_adma_alloc_slots(struct spe_adma_chan *spe_chan,
			int num_slots,
			int slots_per_op)
{
	return __spe_adma_alloc_slots(spe_chan, num_slots, slots_per_op, 1);
}

static void spe_chan_start_null_xor(struct spe_adma_chan *spe_chan);

/* returns the actual number of allocated descriptors */
static int spe_adma_alloc_chan_resources(struct dma_chan *chan)
{
	struct spe_adma_chan *spe_chan = to_spe_adma_chan(chan);
	struct spe_adma_desc_slot *slot = NULL;
	char *hw_desc;
	int i, db_sz;
	int init = spe_chan->slots_allocated ? 0 : 1;
	struct spe_adma_platform_data *plat_data;

	chan->chan_id = spe_chan->device->id;
	plat_data = spe_chan->device->pdev->dev.platform_data;

	spin_lock_bh(&spe_chan->lock);
	/* Allocate descriptor slots */
	i = spe_chan->slots_allocated;
	if (spe_chan->device->id != PPC440SPE_XOR_ID)
		db_sz = sizeof (dma_cdb_t);
	else
		db_sz = sizeof (xor_cb_t);

	for (; i < (plat_data->pool_size/db_sz); i++) {
		slot = kzalloc(sizeof(struct spe_adma_desc_slot), GFP_KERNEL);
		if (!slot) {
			printk(KERN_INFO "SPE ADMA Channel only initialized"
				" %d descriptor slots", i--);
			break;
		}

		hw_desc = (char *) spe_chan->device->dma_desc_pool_virt;
		slot->hw_desc = (void *) &hw_desc[i * db_sz];
		dma_async_tx_descriptor_init(&slot->async_tx, chan);
		INIT_LIST_HEAD(&slot->chain_node);
		INIT_LIST_HEAD(&slot->slot_node);
		INIT_LIST_HEAD(&slot->group_list);
		hw_desc = (char *) spe_chan->device->dma_desc_pool;
		slot->phys = (dma_addr_t) &hw_desc[i * db_sz];
		slot->idx = i;
		list_add_tail(&slot->slot_node, &spe_chan->all_slots);
	}

	if (i && !spe_chan->last_used)
		spe_chan->last_used = list_entry(spe_chan->all_slots.next,
					struct spe_adma_desc_slot,
					slot_node);

	spe_chan->slots_allocated = i;
	PRINTK("spe adma%d: allocated %d descriptor slots last_used: %p\n",
		spe_chan->device->id, i, spe_chan->last_used);
	spin_unlock_bh(&spe_chan->lock);

	/* initialize the channel and the chain with a null operation */
	if (init) {
		if (test_bit(DMA_XOR,
			&spe_chan->device->common.capabilities))
			spe_chan_start_null_xor(spe_chan);
	}

	return (i > 0) ? i : -ENOMEM;
}

static inline dma_cookie_t
spe_desc_assign_cookie(struct spe_adma_chan *spe_chan,
	struct spe_adma_desc_slot *desc)
{
	dma_cookie_t cookie = spe_chan->common.cookie;
	cookie++;
	if (cookie < 0)
		cookie = 1;
	spe_chan->common.cookie = desc->async_tx.cookie = cookie;
	return cookie;
}

static inline void spe_adma_check_threshold(struct spe_adma_chan *spe_chan)
{
	PRINTK("spe adma%d: pending: %d\n", spe_chan->device->id,
		spe_chan->pending);

	if (spe_chan->pending >= SPE_ADMA_THRESHOLD) {
		spe_chan->pending = 0;
		spe_chan_append(spe_chan);
	}
}


static dma_cookie_t
spe_adma_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct spe_adma_desc_slot *sw_desc = tx_to_spe_adma_slot(tx);
	struct spe_adma_chan *spe_chan = to_spe_adma_chan(tx->chan);
	struct spe_adma_desc_slot *group_start, *old_chain_tail;
	int slot_cnt;
	int slots_per_op;
	dma_cookie_t cookie;

	group_start = sw_desc->group_head;
	slot_cnt = group_start->slot_cnt;
	slots_per_op = group_start->slots_per_op;

	spin_lock_bh(&spe_chan->lock);

	cookie = spe_desc_assign_cookie(spe_chan, sw_desc);

	old_chain_tail = list_entry(spe_chan->chain.prev,
		struct spe_adma_desc_slot, chain_node);
	list_splice_init(&sw_desc->group_list, &old_chain_tail->chain_node);

	/* fix up the hardware chain */
	spe_desc_set_next_desc(old_chain_tail, spe_chan, group_start);

	/* increment the pending count by the number of operations */
	spe_chan->pending += slot_cnt / slots_per_op;
	spe_adma_check_threshold(spe_chan);
	spin_unlock_bh(&spe_chan->lock);

	PRINTK("spe adma%d: %s cookie: %d slot: %d tx %p\n", spe_chan->device->id,
		__FUNCTION__, sw_desc->async_tx.cookie, sw_desc->idx, sw_desc);

	return cookie;
}

struct dma_async_tx_descriptor *
spe_adma_prep_dma_interrupt(struct dma_chan *chan)
{
	struct spe_adma_chan *spe_chan = to_spe_adma_chan(chan);
	struct spe_adma_desc_slot *sw_desc, *group_start;
	int slot_cnt, slots_per_op = 0;

	PRINTK("*** spe adma%d: %s\n", spe_chan->device->id, __FUNCTION__);
	spin_lock_bh(&spe_chan->lock);
	slot_cnt = spe_chan_interrupt_slot_count(&slots_per_op, spe_chan);
	sw_desc = spe_adma_alloc_slots(spe_chan, slot_cnt, slots_per_op);
	if (sw_desc) {
		group_start = sw_desc->group_head;
		spe_desc_init_interrupt(group_start, spe_chan);
		sw_desc->async_tx.type = DMA_INTERRUPT;
	}
	spin_unlock_bh(&spe_chan->lock);

	return sw_desc ? &sw_desc->async_tx : NULL;
}

struct dma_async_tx_descriptor *
spe_adma_prep_dma_memcpy(struct dma_chan *chan, size_t len, int int_en)
{
	struct spe_adma_chan *spe_chan = to_spe_adma_chan(chan);
	struct spe_adma_desc_slot *sw_desc, *group_start;
	int slot_cnt, slots_per_op;
	if (unlikely(!len))
		return NULL;
	BUG_ON(unlikely(len > SPE_ADMA_MAX_BYTE_COUNT));

	spin_lock_bh(&spe_chan->lock);

        PRINTK("spe adma%d: %s len: %u int_en %d\n",
        spe_chan->device->id, __FUNCTION__, len, int_en);

	slot_cnt = spe_chan_memcpy_slot_count(len, &slots_per_op);
	sw_desc = spe_adma_alloc_slots(spe_chan, slot_cnt, slots_per_op);
	if (sw_desc) {
		group_start = sw_desc->group_head;
		spe_desc_init_memcpy(group_start, int_en);
		spe_desc_set_byte_count(group_start, spe_chan, len);
		sw_desc->unmap_src_cnt = 1;
		sw_desc->unmap_len = len;
		sw_desc->async_tx.type = DMA_MEMCPY;
	} 
	spin_unlock_bh(&spe_chan->lock);

	return sw_desc ? &sw_desc->async_tx : NULL;
}

struct dma_async_tx_descriptor *
spe_adma_prep_dma_xor(struct dma_chan *chan, unsigned int src_cnt, size_t len,
	int int_en)
{
	struct spe_adma_chan *spe_chan = to_spe_adma_chan(chan);
	struct spe_adma_desc_slot *sw_desc, *group_start;
	int slot_cnt, slots_per_op;
	if (unlikely(!len))
		return NULL;
	BUG_ON(unlikely(len > SPE_ADMA_XOR_MAX_BYTE_COUNT));

	PRINTK("spe adma%d: %s src_cnt: %d len: %u int_en: %d\n",
	spe_chan->device->id, __FUNCTION__, src_cnt, len, int_en);

	spin_lock_bh(&spe_chan->lock);
	slot_cnt = spe_chan_xor_slot_count(len, src_cnt, &slots_per_op);
	sw_desc = spe_adma_alloc_slots(spe_chan, slot_cnt, slots_per_op);
	if (sw_desc) {
		group_start = sw_desc->group_head;
		spe_desc_init_xor(group_start, src_cnt, int_en);
		spe_desc_set_byte_count(group_start, spe_chan, len);
		sw_desc->unmap_src_cnt = src_cnt;
		sw_desc->unmap_len = len;
		sw_desc->async_tx.type = DMA_XOR;
	}
	spin_unlock_bh(&spe_chan->lock);

	return sw_desc ? &sw_desc->async_tx : NULL;
}

static void
spe_adma_set_dest(dma_addr_t addr, struct dma_async_tx_descriptor *tx,
	int index)
{
	struct spe_adma_desc_slot *sw_desc = tx_to_spe_adma_slot(tx);
	struct spe_adma_chan *spe_chan = to_spe_adma_chan(tx->chan);

	/* to do: support transfers lengths > SPE_ADMA_MAX_BYTE_COUNT */
	spe_desc_set_dest_addr(sw_desc->group_head, spe_chan, addr);
}

static void
spe_adma_set_src(dma_addr_t addr, struct dma_async_tx_descriptor *tx,
	int index)
{
	struct spe_adma_desc_slot *sw_desc = tx_to_spe_adma_slot(tx);
	struct spe_adma_desc_slot *group_start = sw_desc->group_head;

	switch (tx->type) {
	case DMA_MEMCPY:
		spe_desc_set_memcpy_src_addr(
			group_start,
			addr,
			group_start->slot_cnt,
			group_start->slots_per_op);
		break;
	case DMA_XOR:
		spe_desc_set_xor_src_addr(
			group_start,
			index,
			addr,
			group_start->slot_cnt,
			group_start->slots_per_op);
		break;
	/* todo: case DMA_ZERO_SUM: */
	/* todo: case DMA_PQ_XOR: */
	/* todo: case DMA_DUAL_XOR: */
	/* todo: case DMA_PQ_UPDATE: */
	/* todo: case DMA_PQ_ZERO_SUM: */
	/* todo: case DMA_MEMCPY_CRC32C: */
	case DMA_MEMSET:
	default:
		do {
			struct spe_adma_chan *spe_chan =
				to_spe_adma_chan(tx->chan);
			printk(KERN_ERR "spe adma%d: unsupport tx_type: %d\n",
				spe_chan->device->id, tx->type);
			BUG();
		} while (0);
	}
}

static inline void spe_adma_schedule_cleanup(unsigned long id)
{
	tasklet_schedule(spe_adma_tasklet[id]);
}

static void spe_adma_dependency_added(struct dma_chan *chan)
{
	struct spe_adma_chan *spe_chan = to_spe_adma_chan(chan);

	spe_adma_schedule_cleanup(spe_chan->device->id);
}

static void spe_adma_free_chan_resources(struct dma_chan *chan)
{
	struct spe_adma_chan *spe_chan = to_spe_adma_chan(chan);
	struct spe_adma_desc_slot *iter, *_iter;
	int in_use_descs = 0;

	spe_adma_slot_cleanup(spe_chan);

	spin_lock_bh(&spe_chan->lock);
	list_for_each_entry_safe(iter, _iter, &spe_chan->chain,
					chain_node) {
		in_use_descs++;
		list_del(&iter->chain_node);
	}
	list_for_each_entry_safe_reverse(iter, _iter, &spe_chan->all_slots, slot_node) {
		list_del(&iter->slot_node);
		kfree(iter);
		spe_chan->slots_allocated--;
	}
	spe_chan->last_used = NULL;

	PRINTK("spe adma%d %s slots_allocated %d\n", spe_chan->device->id,
		__FUNCTION__, spe_chan->slots_allocated);
	spin_unlock_bh(&spe_chan->lock);

	/* one is ok since we left it on there on purpose */
	if (in_use_descs > 1)
		printk(KERN_ERR "SPE: Freeing %d in use descriptors!\n",
			in_use_descs - 1);
}

/**
 * spe_adma_is_complete - poll the status of an ADMA transaction
 * @chan: ADMA channel handle
 * @cookie: ADMA transaction identifier
 */
static enum dma_status spe_adma_is_complete(struct dma_chan *chan,
                                            dma_cookie_t cookie,
                                            dma_cookie_t *done,
                                            dma_cookie_t *used)
{
	struct spe_adma_chan *spe_chan = to_spe_adma_chan(chan);
	dma_cookie_t last_used;
	dma_cookie_t last_complete;
	enum dma_status ret;

	last_used = chan->cookie;
	last_complete = spe_chan->completed_cookie;

	if (done)
		*done= last_complete;
	if (used)
		*used = last_used;

	ret = dma_async_is_complete(cookie, last_complete, last_used);
	if (ret == DMA_SUCCESS)
		return ret;

	spe_adma_slot_cleanup(spe_chan);

	last_used = chan->cookie;
	last_complete = spe_chan->completed_cookie;

	if (done)
		*done= last_complete;
	if (used)
		*used = last_used;

	return dma_async_is_complete(cookie, last_complete, last_used);
}

/*
 * End of transfer interrupt
 */
static irqreturn_t spe_adma_eot_handler(int irq, void *data)
{
	int id = *(int *) data;

	PRINTK("spe adma%d: %s\n", id, __FUNCTION__);

	tasklet_schedule(spe_adma_tasklet[id]);
	spe_adma_device_clear_eot_status(spe_adma_chan_array[id]);

	return IRQ_HANDLED;
}

static void spe_adma_issue_pending(struct dma_chan *chan)
{
	struct spe_adma_chan *spe_chan = to_spe_adma_chan(chan);

	PRINTK("spe adma%d: %s %d \n", spe_chan->device->id, __FUNCTION__,
			spe_chan->pending);

	if (spe_chan->pending) {
        	spe_chan->pending = 0;
                spe_chan_append(spe_chan);
	}
}

void spe_block_ch (struct dma_chan *chan)
{
	struct spe_adma_chan *spe_chan = to_spe_adma_chan(chan);

	spin_lock_bh(&spe_chan->lock);
}

void spe_unblock_ch (struct dma_chan *chan)
{
	struct spe_adma_chan *spe_chan = to_spe_adma_chan(chan);

	spin_unlock_bh(&spe_chan->lock);
}

static dma_addr_t spe_adma_map_page(struct dma_chan *chan, struct page *page,
					unsigned long offset, size_t size,
					int direction)
{
	struct spe_adma_chan *spe_chan = to_spe_adma_chan(chan);
	return dma_map_page(&spe_chan->device->pdev->dev, page, offset, size,
			direction);
}

static dma_addr_t spe_adma_map_single(struct dma_chan *chan, void *cpu_addr,
					size_t size, int direction)
{
	struct spe_adma_chan *spe_chan = to_spe_adma_chan(chan);
	return dma_map_single(&spe_chan->device->pdev->dev, cpu_addr, size,
			direction);
}

static void spe_adma_unmap_page(struct dma_chan *chan, dma_addr_t handle,
				size_t size, int direction)
{
	struct spe_adma_chan *spe_chan = to_spe_adma_chan(chan);
	dma_unmap_page(&spe_chan->device->pdev->dev, handle, size, direction);
}

static void spe_adma_unmap_single(struct dma_chan *chan, dma_addr_t handle,
				size_t size, int direction)
{
	struct spe_adma_chan *spe_chan = to_spe_adma_chan(chan);
	dma_unmap_single(&spe_chan->device->pdev->dev, handle, size, direction);
}

static int __devexit spe_adma_remove(struct platform_device *dev)
{
	struct spe_adma_device *device = platform_get_drvdata(dev);
	struct dma_chan *chan, *_chan;
	struct spe_adma_chan *spe_chan;
	int i;
	struct spe_adma_platform_data *plat_data = dev->dev.platform_data;

	PRINTK("%s\n", __FUNCTION__);

	dma_async_device_unregister(&device->common);

	for (i = 0; i < 3; i++) {
		unsigned int irq;
		irq = platform_get_irq(dev, i);
		free_irq(irq, device);
	}

	dma_free_coherent(&dev->dev, plat_data->pool_size,
			device->dma_desc_pool_virt, device->dma_desc_pool);

	do {
		struct resource *res;
		res = platform_get_resource(dev, IORESOURCE_MEM, 0);
		release_mem_region(res->start, res->end - res->start);
	} while (0);

	list_for_each_entry_safe(chan, _chan, &device->common.channels,
				device_node) {
		spe_chan = to_spe_adma_chan(chan);
		list_del(&chan->device_node);
		kfree(spe_chan);
	}
	kfree(device);

	return 0;
}

static int __devinit spe_adma_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret=0, irq_eot=0, irq;
	struct spe_adma_device *adev;
	struct spe_adma_chan *spe_chan;
	struct spe_adma_platform_data *plat_data = pdev->dev.platform_data;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	if (!request_mem_region(res->start, res->end - res->start, pdev->name))
		return -EBUSY;

	if ((adev = kzalloc(sizeof(*adev), GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto err_adev_alloc;
	}

	/* allocate coherent memory for hardware descriptors
	 * note: writecombine gives slightly better performance, but
	 * requires that we explicitly drain the write buffer
	 */
	if ((adev->dma_desc_pool_virt = dma_alloc_coherent(&pdev->dev,
					plat_data->pool_size,
					&adev->dma_desc_pool,
					GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto err_dma_alloc;
	}

	PRINTK("%s: allocted descriptor pool virt %p phys %p\n",
	__FUNCTION__, adev->dma_desc_pool_virt, (void *) adev->dma_desc_pool);

	adev->id = plat_data->hw_id;
	adev->common.capabilities = plat_data->capabilities;

	/* clear errors before enabling interrupts */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = -ENXIO;
	} else {
		irq_eot = irq;
		ret = request_irq(irq, spe_adma_eot_handler,
			0, pdev->name, &adev->id);
		if (ret) {
			ret = -EIO;
			goto err_irq0;
		}
	}

	adev->pdev = pdev;
	platform_set_drvdata(pdev, adev);

	INIT_LIST_HEAD(&adev->common.channels);

	/* set base routines */
	adev->common.device_tx_submit = spe_adma_tx_submit;
	adev->common.device_set_dest = spe_adma_set_dest;
	adev->common.device_set_src = spe_adma_set_src;
	adev->common.device_alloc_chan_resources = spe_adma_alloc_chan_resources;
	adev->common.device_free_chan_resources = spe_adma_free_chan_resources;
	adev->common.device_is_tx_complete = spe_adma_is_complete;
	adev->common.device_issue_pending = spe_adma_issue_pending;
	adev->common.device_dependency_added = spe_adma_dependency_added;

	adev->common.map_page = spe_adma_map_page;
	adev->common.map_single = spe_adma_map_single;
	adev->common.unmap_page = spe_adma_unmap_page;
	adev->common.unmap_single = spe_adma_unmap_single;

	/* set prep routines based on capability */
	if (test_bit(DMA_MEMCPY, &adev->common.capabilities))
		adev->common.device_prep_dma_memcpy = spe_adma_prep_dma_memcpy;
	if (test_bit(DMA_XOR, &adev->common.capabilities)) {
		adev->common.max_xor = spe_adma_get_max_xor();
		adev->common.device_prep_dma_xor = spe_adma_prep_dma_xor;
	}
	if (test_bit(DMA_INTERRUPT, &adev->common.capabilities))
		adev->common.device_prep_dma_interrupt =
			spe_adma_prep_dma_interrupt;

	if ((spe_chan = kzalloc(sizeof(struct spe_adma_chan), GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto err_chan_alloc;
	}

	spe_adma_chan_array[adev->id] = spe_chan;

	spe_chan->device = adev;
	spin_lock_init(&spe_chan->lock);
	init_timer(&spe_chan->cleanup_watchdog);
	spe_chan->cleanup_watchdog.data = adev->id;
	spe_chan->cleanup_watchdog.function = spe_adma_schedule_cleanup;
	INIT_LIST_HEAD(&spe_chan->chain);
	INIT_LIST_HEAD(&spe_chan->all_slots);
	INIT_RCU_HEAD(&spe_chan->common.rcu);
	spe_chan->common.device = &adev->common;
	list_add_tail(&spe_chan->common.device_node, &adev->common.channels);

	printk(KERN_INFO "Intel(R) SPE ADMA Engine found [%d]: "
	  "( %s%s%s%s%s%s%s%s%s%s)\n",
	  adev->id,
	  test_bit(DMA_PQ_XOR, &adev->common.capabilities) ? "pq_xor " : "",
	  test_bit(DMA_PQ_UPDATE, &adev->common.capabilities) ? "pq_update " : "",
	  test_bit(DMA_PQ_ZERO_SUM, &adev->common.capabilities) ? "pq_zero_sum " : "",
	  test_bit(DMA_XOR, &adev->common.capabilities) ? "xor " : "",
	  test_bit(DMA_DUAL_XOR, &adev->common.capabilities) ? "dual_xor " : "",
	  test_bit(DMA_ZERO_SUM, &adev->common.capabilities) ? "xor_zero_sum " : "",
	  test_bit(DMA_MEMSET, &adev->common.capabilities)  ? "memset " : "",
	  test_bit(DMA_MEMCPY_CRC32C, &adev->common.capabilities) ? "memcpy+crc " : "",
	  test_bit(DMA_MEMCPY, &adev->common.capabilities) ? "memcpy " : "",
	  test_bit(DMA_INTERRUPT, &adev->common.capabilities) ? "int " : "");

	dma_async_device_register(&adev->common);
	goto out;

err_chan_alloc:
err_irq0:
	dma_free_coherent(&adev->pdev->dev, plat_data->pool_size,
			adev->dma_desc_pool_virt, adev->dma_desc_pool);
err_dma_alloc:
	kfree(adev);
err_adev_alloc:
	release_mem_region(res->start, res->end - res->start);
out:
	return ret;
}

static char src1[16], src2[16], dst[16];

static void spe_chan_start_null_xor(struct spe_adma_chan *spe_chan)
{
	struct spe_adma_desc_slot *sw_desc, *group_start;
	dma_cookie_t cookie;
	int slot_cnt, slots_per_op;

	PRINTK("spe adma%d: %s\n", spe_chan->device->id, __FUNCTION__);

	spin_lock_bh(&spe_chan->lock);
	slot_cnt = spe_chan_xor_slot_count(0, 2, &slots_per_op);
	sw_desc = spe_adma_alloc_slots(spe_chan, slot_cnt, slots_per_op);
	if (sw_desc) {
		group_start = sw_desc->group_head;
		list_splice_init(&sw_desc->group_list, &spe_chan->chain);
		sw_desc->async_tx.ack = 1;
		spe_desc_init_null_xor(group_start, 2, 0);
		spe_desc_set_byte_count(group_start, spe_chan, 16);
		spe_desc_set_dest_addr(group_start, spe_chan, __pa(dst));
		spe_desc_set_xor_src_addr(group_start, 0, __pa(src1), 1, 1);
		spe_desc_set_xor_src_addr(group_start, 1, __pa(src2), 1, 1);

		cookie = spe_chan->common.cookie;
		cookie++;
		if (cookie <= 1)
			cookie = 2;

		/* initialize the completed cookie to be less than
		 * the most recently used cookie
		 */
		spe_chan->completed_cookie = cookie - 1;
		spe_chan->common.cookie = sw_desc->async_tx.cookie = cookie;

		/* channel should not be busy */
		BUG_ON(spe_chan_is_busy(spe_chan));

		/* disable operation */
		spe_chan_disable(spe_chan);

		/* set the descriptor address */
		spe_chan_set_next_descriptor(spe_chan, sw_desc);

		/* run the descriptor */
		spe_chan_enable(spe_chan);
	} else
		printk(KERN_ERR "spe adma%d failed to allocate null descriptor\n",
			spe_chan->device->id);
	spin_unlock_bh(&spe_chan->lock);
}

static struct platform_driver spe_adma_driver = {
	.probe		= spe_adma_probe,
	.remove		= spe_adma_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "PPC440SPE-ADMA",
	},
};

static int __init spe_adma_init (void)
{
	/* it's currently unsafe to unload this module */
	/* if forced, worst case is that rmmod hangs */
	__unsafe(THIS_MODULE);

	return platform_driver_register(&spe_adma_driver);
}

static void __exit spe_adma_exit (void)
{
	platform_driver_unregister(&spe_adma_driver);
	return;
}

module_init(spe_adma_init);
module_exit(spe_adma_exit);

MODULE_AUTHOR("Yuri Tikhonov <yur@emcraft.com>");
MODULE_DESCRIPTION("PPC440SPE ADMA Engine Driver");
MODULE_LICENSE("GPL");
