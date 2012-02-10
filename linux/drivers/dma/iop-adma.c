/*
 * Copyright(c) 2006 Intel Corporation. All rights reserved.
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
 * This driver supports the asynchrounous DMA copy and RAID engines available
 * on the Intel Xscale(R) family of I/O Processors (IOP 32x, 33x, 134x)
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/async_tx.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <asm/arch/adma.h>
#include <asm/memory.h>

#define to_iop_adma_chan(chan) container_of(chan, struct iop_adma_chan, common)
#define to_iop_adma_device(dev) container_of(dev, struct iop_adma_device, common)
#define to_iop_adma_slot(lh) container_of(lh, struct iop_adma_desc_slot, slot_node)
#define tx_to_iop_adma_slot(tx) container_of(tx, struct iop_adma_desc_slot, async_tx)

#define IOP_ADMA_DEBUG 0
#define PRINTK(x...) ((void)(IOP_ADMA_DEBUG && printk(x)))

/**
 * iop_adma_free_slots - flags descriptor slots for reuse
 * @slot: Slot to free
 * Caller must hold &iop_chan->lock while calling this function
 */
static inline void iop_adma_free_slots(struct iop_adma_desc_slot *slot)
{
	int stride = slot->stride;

	while (stride--) {
		slot->stride = 0;
		slot = list_entry(slot->slot_node.next,
				struct iop_adma_desc_slot,
				slot_node);
	}
}

static inline dma_cookie_t
iop_adma_run_tx_complete_actions(struct iop_adma_desc_slot *desc,
	struct iop_adma_chan *iop_chan, dma_cookie_t cookie)
{
	BUG_ON(desc->async_tx.cookie < 0);
	spin_lock_bh(&desc->async_tx.lock);
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
			struct iop_adma_desc_slot *unmap = desc->group_head;
			struct device *dev =
				&iop_chan->device->pdev->dev;
			u32 len = unmap->unmap_len;
			u32 src_cnt = unmap->unmap_src_cnt;
			dma_addr_t addr = iop_desc_get_dest_addr(unmap,
				iop_chan);

			dma_unmap_page(dev, addr, len, DMA_FROM_DEVICE);
			while(src_cnt--) {
				addr = iop_desc_get_src_addr(unmap,
							iop_chan,
							src_cnt);
				dma_unmap_page(dev, addr, len,
					DMA_TO_DEVICE);
			}
			desc->group_head = NULL;
		}
	}

	/* run dependent operations */
	async_tx_run_dependencies(&desc->async_tx, &iop_chan->common);
	spin_unlock_bh(&desc->async_tx.lock);

	return cookie;
}

static inline int
iop_adma_clean_slot(struct iop_adma_desc_slot *desc,
	struct iop_adma_chan *iop_chan)
{
	/* the client is allowed to attach dependent operations
	 * until 'ack' is set
	 */
	if (!desc->async_tx.ack)
		return 0;

	/* leave the last descriptor in the chain
	 * so we can append to it
	 */
	if (desc->chain_node.next == &iop_chan->chain)
		return 1;

	PRINTK("\tfree slot: %d stride: %d\n", desc->idx, desc->stride);

	list_del(&desc->chain_node);
	iop_adma_free_slots(desc);

	return 0;
}

static void __iop_adma_slot_cleanup(struct iop_adma_chan *iop_chan)
{
	struct iop_adma_desc_slot *iter, *_iter, *group_start = NULL;
	dma_cookie_t cookie = 0;
	u32 current_desc = iop_chan_get_current_descriptor(iop_chan);
	int busy = iop_chan_is_busy(iop_chan);
	int seen_current = 0, slot_cnt = 0, slots_per_op = 0;

	PRINTK("iop adma%d: %s\n", iop_chan->device->id, __FUNCTION__);
	/* free completed slots from the chain starting with
	 * the oldest descriptor
	 */
	list_for_each_entry_safe(iter, _iter, &iop_chan->chain,
					chain_node) {
		PRINTK("\tcookie: %d slot: %d busy: %d "
			"this_desc: %#x next_desc: %#x ack: %d\n",
			iter->async_tx.cookie, iter->idx, busy, iter->phys,
			iop_desc_get_next_desc(iter, iop_chan),
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
			if (busy || iop_desc_get_next_desc(iter, iop_chan))
				break;
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
			struct iop_adma_desc_slot *grp_iter, *_grp_iter;
			int end_of_chain = 0;
			PRINTK("\tgroup end\n");

			/* collect the total results */
			if (group_start->xor_check_result) {
				u32 zero_sum_result = 0;
				slot_cnt = group_start->slot_cnt;
				grp_iter = group_start;

				list_for_each_entry_from(grp_iter,
					&iop_chan->chain, chain_node) {
					zero_sum_result |=
						iop_desc_get_zero_result(grp_iter);
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
				&iop_chan->chain, chain_node) {
				cookie = iop_adma_run_tx_complete_actions(
					grp_iter, iop_chan, cookie);

				slot_cnt -= slots_per_op;
				end_of_chain = iop_adma_clean_slot(grp_iter,
					iop_chan);

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

		/* write back zero sum results (single descriptor case) */
		if (iter->xor_check_result && iter->async_tx.cookie)
			*iter->xor_check_result = iop_desc_get_zero_result(iter);

		cookie = iop_adma_run_tx_complete_actions(iter, iop_chan, cookie);
		if (iop_adma_clean_slot(iter, iop_chan))
			break;
	}

	BUG_ON(!seen_current);

	iop_chan_idle(busy, iop_chan);

	if (cookie > 0) {
		iop_chan->completed_cookie = cookie;
		PRINTK("\tcompleted cookie %d\n", cookie);
	}
}

static inline void
iop_adma_slot_cleanup(struct iop_adma_chan *iop_chan)
{
	spin_lock_bh(&iop_chan->lock);
	__iop_adma_slot_cleanup(iop_chan);
	spin_unlock_bh(&iop_chan->lock);
}

static struct iop_adma_chan *iop_adma_chan_array[3];
static void iop_adma0_task(unsigned long data)
{
	__iop_adma_slot_cleanup(iop_adma_chan_array[0]);
}

static void iop_adma1_task(unsigned long data)
{
	__iop_adma_slot_cleanup(iop_adma_chan_array[1]);
}

static void iop_adma2_task(unsigned long data)
{
	__iop_adma_slot_cleanup(iop_adma_chan_array[2]);
}

DECLARE_TASKLET(iop_adma0_tasklet, iop_adma0_task, 0);
DECLARE_TASKLET(iop_adma1_tasklet, iop_adma1_task, 0);
DECLARE_TASKLET(iop_adma2_tasklet, iop_adma2_task, 0);
struct tasklet_struct *iop_adma_tasklet[] = {
	&iop_adma0_tasklet,
	&iop_adma1_tasklet,
	&iop_adma2_tasklet,
};

static struct iop_adma_desc_slot *
__iop_adma_alloc_slots(struct iop_adma_chan *iop_chan, int num_slots,
			int slots_per_op, int recurse)
{
	struct iop_adma_desc_slot *iter = NULL, *alloc_start = NULL;
	struct iop_adma_desc_slot *last_used = NULL, *last_op_head = NULL;
	struct list_head chain = LIST_HEAD_INIT(chain);
	int i;

	/* start search from the last allocated descrtiptor
	 * if a contiguous allocation can not be found start searching
	 * from the beginning of the list
	 */
	for (i = 0; i < 2; i++) {
		int slots_found = 0;
		if (i == 0)
			iter = iop_chan->last_used;
		else {
			iter = list_entry(&iop_chan->all_slots,
				struct iop_adma_desc_slot,
				slot_node);
		}

		list_for_each_entry_continue(iter, &iop_chan->all_slots, slot_node) {
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
				if (iop_desc_is_aligned(iter, slots_per_op))
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
					PRINTK("iop adma%d: allocated slot: %d "
						"(desc %p phys: %#x) stride %d\n",
						iop_chan->device->id,
						iter->idx, iter->hw_desc, iter->phys,
						slots_per_op);

					/* pre-ack all but the last descriptor */
					if (num_slots != slots_per_op)
						iter->async_tx.ack = 1;
					else
						iter->async_tx.ack = 0;

					list_add_tail(&iter->chain_node, &chain);
					last_op_head = iter;
					iter->async_tx.cookie = 0;
					iter->slot_cnt = num_slots;
					iter->slots_per_op = slots_per_op;
					iter->xor_check_result = NULL;
					for (i = 0; i < slots_per_op; i++) {
						iter->stride = slots_per_op - i;
						last_used = iter;
						iter = list_entry(iter->slot_node.next,
								struct iop_adma_desc_slot,
								slot_node);
					}
					num_slots -= slots_per_op;
				}
				last_op_head->group_head = alloc_start;
				last_op_head->async_tx.cookie = -EBUSY;
				list_splice(&chain, &last_op_head->group_list);
				iop_chan->last_used = last_used;
				return last_op_head;
			}
		}
	}

	/* try to free some slots if the allocation fails */
	tasklet_schedule(iop_adma_tasklet[iop_chan->device->id]);
	return NULL;
}

static struct iop_adma_desc_slot *
iop_adma_alloc_slots(struct iop_adma_chan *iop_chan,
			int num_slots,
			int slots_per_op)
{
	return __iop_adma_alloc_slots(iop_chan, num_slots, slots_per_op, 1);
}

static void iop_chan_start_null_memcpy(struct iop_adma_chan *iop_chan);
static void iop_chan_start_null_xor(struct iop_adma_chan *iop_chan);

/* returns the actual number of allocated descriptors */
static int iop_adma_alloc_chan_resources(struct dma_chan *chan)
{
	struct iop_adma_chan *iop_chan = to_iop_adma_chan(chan);
	struct iop_adma_desc_slot *slot = NULL;
	char *hw_desc;
	int i;
	int init = iop_chan->slots_allocated ? 0 : 1;
	struct iop_adma_platform_data *plat_data;

	plat_data = iop_chan->device->pdev->dev.platform_data;

	spin_lock_bh(&iop_chan->lock);
	/* Allocate descriptor slots */
	i = iop_chan->slots_allocated;
	for (; i < (plat_data->pool_size/IOP_ADMA_SLOT_SIZE); i++) {
		slot = kzalloc(sizeof(*slot), GFP_KERNEL);
		if (!slot) {
			printk(KERN_INFO "IOP ADMA Channel only initialized"
				" %d descriptor slots", i--);
			break;
		}
		hw_desc = (char *) iop_chan->device->dma_desc_pool_virt;
		slot->hw_desc = (void *) &hw_desc[i * IOP_ADMA_SLOT_SIZE];

		dma_async_tx_descriptor_init(&slot->async_tx, chan);
		INIT_LIST_HEAD(&slot->chain_node);
		INIT_LIST_HEAD(&slot->slot_node);
		INIT_LIST_HEAD(&slot->group_list);
		hw_desc = (char *) iop_chan->device->dma_desc_pool;
		slot->phys = (dma_addr_t) &hw_desc[i * IOP_ADMA_SLOT_SIZE];
		slot->idx = i;
		list_add_tail(&slot->slot_node, &iop_chan->all_slots);
	}
	if (i && !iop_chan->last_used)
		iop_chan->last_used = list_entry(iop_chan->all_slots.next,
					struct iop_adma_desc_slot,
					slot_node);

	iop_chan->slots_allocated = i;
	PRINTK("iop adma%d: allocated %d descriptor slots last_used: %p\n",
		iop_chan->device->id, i, iop_chan->last_used);
	spin_unlock_bh(&iop_chan->lock);

	/* initialize the channel and the chain with a null operation */
	if (init) {
		if (test_bit(DMA_MEMCPY,
			&iop_chan->device->common.capabilities))
			iop_chan_start_null_memcpy(iop_chan);
		else if (test_bit(DMA_XOR,
			&iop_chan->device->common.capabilities))
			iop_chan_start_null_xor(iop_chan);
		else
			BUG();
	}

	return (i > 0) ? i : -ENOMEM;
}

static inline dma_cookie_t
iop_desc_assign_cookie(struct iop_adma_chan *iop_chan,
	struct iop_adma_desc_slot *desc)
{
	dma_cookie_t cookie = iop_chan->common.cookie;
	cookie++;
	if (cookie < 0)
		cookie = 1;
	iop_chan->common.cookie = desc->async_tx.cookie = cookie;
	return cookie;
}

static inline void iop_adma_check_threshold(struct iop_adma_chan *iop_chan)
{
	PRINTK("iop adma%d: pending: %d\n", iop_chan->device->id,
		iop_chan->pending);

	if (iop_chan->pending >= IOP_ADMA_THRESHOLD) {
		iop_chan->pending = 0;
		iop_chan_append(iop_chan);
	}
}

static dma_cookie_t
iop_adma_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct iop_adma_desc_slot *sw_desc = tx_to_iop_adma_slot(tx);
	struct iop_adma_chan *iop_chan = to_iop_adma_chan(tx->chan);
	struct iop_adma_desc_slot *group_start, *old_chain_tail;
	int slot_cnt;
	int slots_per_op;
	dma_cookie_t cookie;

	group_start = sw_desc->group_head;
	slot_cnt = group_start->slot_cnt;
	slots_per_op = group_start->slots_per_op;

	spin_lock_bh(&iop_chan->lock);
	cookie = iop_desc_assign_cookie(iop_chan, sw_desc);

	old_chain_tail = list_entry(iop_chan->chain.prev,
		struct iop_adma_desc_slot, chain_node);
	list_splice_init(&sw_desc->group_list, &old_chain_tail->chain_node);

	/* fix up the hardware chain */
	iop_desc_set_next_desc(old_chain_tail, iop_chan, group_start->phys);

	/* increment the pending count by the number of operations */
	iop_chan->pending += slot_cnt / slots_per_op;
	iop_adma_check_threshold(iop_chan);
	spin_unlock_bh(&iop_chan->lock);

	PRINTK("iop adma%d: %s cookie: %d slot: %d\n", iop_chan->device->id,
		__FUNCTION__, sw_desc->async_tx.cookie, sw_desc->idx);

	return cookie;
}

struct dma_async_tx_descriptor *
iop_adma_prep_dma_interrupt(struct dma_chan *chan)
{
	struct iop_adma_chan *iop_chan = to_iop_adma_chan(chan);
	struct iop_adma_desc_slot *sw_desc, *group_start;
	int slot_cnt, slots_per_op;

	PRINTK("iop adma%d: %s\n", iop_chan->device->id, __FUNCTION__);

	spin_lock_bh(&iop_chan->lock);
	slot_cnt = iop_chan_interrupt_slot_count(&slots_per_op, iop_chan);
	sw_desc = iop_adma_alloc_slots(iop_chan, slot_cnt, slots_per_op);
	if (sw_desc) {
		group_start = sw_desc->group_head;
		iop_desc_init_interrupt(group_start, iop_chan);
		sw_desc->async_tx.type = DMA_INTERRUPT;
	}
	spin_unlock_bh(&iop_chan->lock);

	return sw_desc ? &sw_desc->async_tx : NULL;
}

struct dma_async_tx_descriptor *
iop_adma_prep_dma_memcpy(struct dma_chan *chan, size_t len, int int_en)
{
	struct iop_adma_chan *iop_chan = to_iop_adma_chan(chan);
	struct iop_adma_desc_slot *sw_desc, *group_start;
	int slot_cnt, slots_per_op;

	if (unlikely(!len))
		return NULL;
	BUG_ON(unlikely(len > IOP_ADMA_MAX_BYTE_COUNT));

	PRINTK("iop adma%d: %s len: %u\n",
	iop_chan->device->id, __FUNCTION__, len);

	spin_lock_bh(&iop_chan->lock);
	slot_cnt = iop_chan_memcpy_slot_count(len, &slots_per_op);
	sw_desc = iop_adma_alloc_slots(iop_chan, slot_cnt, slots_per_op);
	if (sw_desc) {
		group_start = sw_desc->group_head;
		iop_desc_init_memcpy(group_start, int_en);
		iop_desc_set_byte_count(group_start, iop_chan, len);
		sw_desc->unmap_src_cnt = 1;
		sw_desc->unmap_len = len;
		sw_desc->async_tx.type = DMA_MEMCPY;
	}
	spin_unlock_bh(&iop_chan->lock);

	return sw_desc ? &sw_desc->async_tx : NULL;
}

struct dma_async_tx_descriptor *
iop_adma_prep_dma_memset(struct dma_chan *chan, int value, size_t len,
	int int_en)
{
	struct iop_adma_chan *iop_chan = to_iop_adma_chan(chan);
	struct iop_adma_desc_slot *sw_desc, *group_start;
	int slot_cnt, slots_per_op;

	if (unlikely(!len))
		return NULL;
	BUG_ON(unlikely(len > IOP_ADMA_MAX_BYTE_COUNT));

	PRINTK("iop adma%d: %s len: %u\n",
	iop_chan->device->id, __FUNCTION__, len);

	spin_lock_bh(&iop_chan->lock);
	slot_cnt = iop_chan_memset_slot_count(len, &slots_per_op);
	sw_desc = iop_adma_alloc_slots(iop_chan, slot_cnt, slots_per_op);
	if (sw_desc) {
		group_start = sw_desc->group_head;
		iop_desc_init_memset(group_start, int_en);
		iop_desc_set_byte_count(group_start, iop_chan, len);
		iop_desc_set_block_fill_val(group_start, value);
		sw_desc->unmap_src_cnt = 1;
		sw_desc->unmap_len = len;
		sw_desc->async_tx.type = DMA_MEMSET;
	}
	spin_unlock_bh(&iop_chan->lock);

	return sw_desc ? &sw_desc->async_tx : NULL;
}

struct dma_async_tx_descriptor *
iop_adma_prep_dma_xor(struct dma_chan *chan, unsigned int src_cnt, size_t len,
	int int_en)
{
	struct iop_adma_chan *iop_chan = to_iop_adma_chan(chan);
	struct iop_adma_desc_slot *sw_desc, *group_start;
	int slot_cnt, slots_per_op;

	if (unlikely(!len))
		return NULL;
	BUG_ON(unlikely(len > IOP_ADMA_XOR_MAX_BYTE_COUNT));

	PRINTK("iop adma%d: %s src_cnt: %d len: %u int_en: %d\n",
	iop_chan->device->id, __FUNCTION__, src_cnt, len, int_en);

	spin_lock_bh(&iop_chan->lock);
	slot_cnt = iop_chan_xor_slot_count(len, src_cnt, &slots_per_op);
	sw_desc = iop_adma_alloc_slots(iop_chan, slot_cnt, slots_per_op);
	if (sw_desc) {
		group_start = sw_desc->group_head;
		iop_desc_init_xor(group_start, src_cnt, int_en);
		iop_desc_set_byte_count(group_start, iop_chan, len);
		sw_desc->unmap_src_cnt = src_cnt;
		sw_desc->unmap_len = len;
		sw_desc->async_tx.type = DMA_XOR;
	}
	spin_unlock_bh(&iop_chan->lock);

	return sw_desc ? &sw_desc->async_tx : NULL;
}

struct dma_async_tx_descriptor *
iop_adma_prep_dma_zero_sum(struct dma_chan *chan, unsigned int src_cnt,
	size_t len, u32 *result, int int_en)
{
	struct iop_adma_chan *iop_chan = to_iop_adma_chan(chan);
	struct iop_adma_desc_slot *sw_desc, *group_start;
	int slot_cnt, slots_per_op;

	if (unlikely(!len))
		return NULL;

	PRINTK("iop adma%d: %s src_cnt: %d len: %u\n",
	iop_chan->device->id, __FUNCTION__, src_cnt, len);

	spin_lock_bh(&iop_chan->lock);
	slot_cnt = iop_chan_zero_sum_slot_count(len, src_cnt, &slots_per_op);
	sw_desc = iop_adma_alloc_slots(iop_chan, slot_cnt, slots_per_op);
	if (sw_desc) {
		group_start = sw_desc->group_head;
		iop_desc_init_zero_sum(group_start, src_cnt, slot_cnt,
			slots_per_op, int_en);
		iop_desc_set_zero_sum_byte_count(group_start, len, slots_per_op);
		group_start->xor_check_result = result;
		PRINTK("\t%s: group_start->xor_check_result: %p\n",
			__FUNCTION__, group_start->xor_check_result);
		sw_desc->unmap_src_cnt = src_cnt;
		sw_desc->unmap_len = len;
		sw_desc->async_tx.type = DMA_ZERO_SUM;
	}
	spin_unlock_bh(&iop_chan->lock);

	return sw_desc ? &sw_desc->async_tx : NULL;
}

static void
iop_adma_set_dest(dma_addr_t addr, struct dma_async_tx_descriptor *tx,
	int index)
{
	struct iop_adma_desc_slot *sw_desc = tx_to_iop_adma_slot(tx);
	struct iop_adma_chan *iop_chan = to_iop_adma_chan(tx->chan);

	/* to do: support transfers lengths > IOP_ADMA_MAX_BYTE_COUNT */
	iop_desc_set_dest_addr(sw_desc->group_head, iop_chan, addr);
}

static void
iop_adma_set_src(dma_addr_t addr, struct dma_async_tx_descriptor *tx,
	int index)
{
	struct iop_adma_desc_slot *sw_desc = tx_to_iop_adma_slot(tx);
	struct iop_adma_desc_slot *group_start = sw_desc->group_head;

	switch (tx->type) {
	case DMA_MEMCPY:
		iop_desc_set_memcpy_src_addr(
			group_start,
			addr,
			group_start->slot_cnt,
			group_start->slots_per_op);
		break;
	case DMA_XOR:
		iop_desc_set_xor_src_addr(
			group_start,
			index,
			addr,
			group_start->slot_cnt,
			group_start->slots_per_op);
		break;
	case DMA_ZERO_SUM:
		iop_desc_set_zero_sum_src_addr(
			group_start,
			index,
			addr,
			group_start->slot_cnt,
			group_start->slots_per_op);
		break;
	/* todo: case DMA_PQ_XOR: */
	/* todo: case DMA_DUAL_XOR: */
	/* todo: case DMA_PQ_UPDATE: */
	/* todo: case DMA_PQ_ZERO_SUM: */
	/* todo: case DMA_MEMCPY_CRC32C: */
	case DMA_MEMSET:
	default:
		do {
			struct iop_adma_chan *iop_chan =
				to_iop_adma_chan(tx->chan);
			printk(KERN_ERR "iop adma%d: unsupport tx_type: %d\n",
				iop_chan->device->id, tx->type);
			BUG();
		} while (0);
	}
}

static inline void iop_adma_schedule_cleanup(unsigned long id)
{
	tasklet_schedule(iop_adma_tasklet[id]);
}

static void iop_adma_dependency_added(struct dma_chan *chan)
{
	struct iop_adma_chan *iop_chan = to_iop_adma_chan(chan);
	iop_adma_schedule_cleanup(iop_chan->device->id);
}

static void iop_adma_free_chan_resources(struct dma_chan *chan)
{
	struct iop_adma_chan *iop_chan = to_iop_adma_chan(chan);
	struct iop_adma_desc_slot *iter, *_iter;
	int in_use_descs = 0;

	iop_adma_slot_cleanup(iop_chan);

	spin_lock_bh(&iop_chan->lock);
	list_for_each_entry_safe(iter, _iter, &iop_chan->chain,
					chain_node) {
		in_use_descs++;
		list_del(&iter->chain_node);
	}
	list_for_each_entry_safe_reverse(iter, _iter, &iop_chan->all_slots, slot_node) {
		list_del(&iter->slot_node);
		kfree(iter);
		iop_chan->slots_allocated--;
	}
	iop_chan->last_used = NULL;

	PRINTK("iop adma%d %s slots_allocated %d\n", iop_chan->device->id,
		__FUNCTION__, iop_chan->slots_allocated);
	spin_unlock_bh(&iop_chan->lock);

	/* one is ok since we left it on there on purpose */
	if (in_use_descs > 1)
		printk(KERN_ERR "IOP: Freeing %d in use descriptors!\n",
			in_use_descs - 1);
}

/**
 * iop_adma_is_complete - poll the status of an ADMA transaction
 * @chan: ADMA channel handle
 * @cookie: ADMA transaction identifier
 */
static enum dma_status iop_adma_is_complete(struct dma_chan *chan,
                                            dma_cookie_t cookie,
                                            dma_cookie_t *done,
                                            dma_cookie_t *used)
{
	struct iop_adma_chan *iop_chan = to_iop_adma_chan(chan);
	dma_cookie_t last_used;
	dma_cookie_t last_complete;
	enum dma_status ret;

	last_used = chan->cookie;
	last_complete = iop_chan->completed_cookie;

	if (done)
		*done= last_complete;
	if (used)
		*used = last_used;

	ret = dma_async_is_complete(cookie, last_complete, last_used);
	if (ret == DMA_SUCCESS)
		return ret;

	iop_adma_slot_cleanup(iop_chan);

	last_used = chan->cookie;
	last_complete = iop_chan->completed_cookie;

	if (done)
		*done= last_complete;
	if (used)
		*used = last_used;

	return dma_async_is_complete(cookie, last_complete, last_used);
}

static irqreturn_t iop_adma_eot_handler(int irq, void *data)
{
	int id = *(int *) data;

	PRINTK("iop adma%d: %s\n", id, __FUNCTION__);

	tasklet_schedule(iop_adma_tasklet[id]);

	iop_adma_device_clear_eot_status(id);

	return IRQ_HANDLED;
}

static irqreturn_t iop_adma_eoc_handler(int irq, void *data)
{
	int id = *(int *) data;

	PRINTK("iop adma%d: %s\n", id, __FUNCTION__);

	tasklet_schedule(iop_adma_tasklet[id]);

	iop_adma_device_clear_eoc_status(id);

	return IRQ_HANDLED;
}

static irqreturn_t iop_adma_err_handler(int irq, void *data)
{
	int id = *(int *) data;
	unsigned long status = iop_device_get_status(id);

	printk(KERN_ERR "iop adma%d: error ( %s%s%s%s%s%s%s)\n", id,
		iop_is_err_int_parity(status, id) ? "int_parity " : "",
		iop_is_err_mcu_abort(status, id) ? "mcu_abort " : "",
		iop_is_err_int_tabort(status, id) ? "int_tabort " : "",
		iop_is_err_int_mabort(status, id) ? "int_mabort " : "",
		iop_is_err_pci_tabort(status, id) ? "pci_tabort " : "",
		iop_is_err_pci_mabort(status, id) ? "pci_mabort " : "",
		iop_is_err_split_tx(status, id) ? "split_tx " : "");

	iop_adma_device_clear_err_status(id);

	BUG();

	return IRQ_HANDLED;
}

static void iop_adma_issue_pending(struct dma_chan *chan)
{
	struct iop_adma_chan *iop_chan = to_iop_adma_chan(chan);

	if (iop_chan->pending) {
		iop_chan->pending = 0;
		iop_chan_append(iop_chan);
	}
}

static dma_addr_t iop_adma_map_page(struct dma_chan *chan, struct page *page,
					unsigned long offset, size_t size,
					int direction)
{
	struct iop_adma_chan *iop_chan = to_iop_adma_chan(chan);
	return dma_map_page(&iop_chan->device->pdev->dev, page, offset, size,
			direction);
}

static dma_addr_t iop_adma_map_single(struct dma_chan *chan, void *cpu_addr,
					size_t size, int direction)
{
	struct iop_adma_chan *iop_chan = to_iop_adma_chan(chan);
	return dma_map_single(&iop_chan->device->pdev->dev, cpu_addr, size,
			direction);
}

static void iop_adma_unmap_page(struct dma_chan *chan, dma_addr_t handle,
				size_t size, int direction)
{
	struct iop_adma_chan *iop_chan = to_iop_adma_chan(chan);
	dma_unmap_page(&iop_chan->device->pdev->dev, handle, size, direction);
}

static void iop_adma_unmap_single(struct dma_chan *chan, dma_addr_t handle,
				size_t size, int direction)
{
	struct iop_adma_chan *iop_chan = to_iop_adma_chan(chan);
	dma_unmap_single(&iop_chan->device->pdev->dev, handle, size, direction);
}

/*
 * Perform a transaction to verify the HW works.
 */
#define IOP_ADMA_TEST_SIZE 2000

static int __devinit iop_adma_memcpy_self_test(struct iop_adma_device *device)
{
	int i;
	void *src, *dest;
	dma_addr_t src_dma, dest_dma;
	struct dma_chan *dma_chan;
	dma_cookie_t cookie;
	struct dma_async_tx_descriptor *tx;
	int err = 0;
	struct iop_adma_chan *iop_chan;

	PRINTK("iop adma%d: %s\n", device->id, __FUNCTION__);

	src = kzalloc(sizeof(u8) * IOP_ADMA_TEST_SIZE, GFP_KERNEL);
	if (!src)
		return -ENOMEM;
	dest = kzalloc(sizeof(u8) * IOP_ADMA_TEST_SIZE, GFP_KERNEL);
	if (!dest) {
		kfree(src);
		return -ENOMEM;
	}

	/* Fill in src buffer */
	for (i = 0; i < IOP_ADMA_TEST_SIZE; i++)
		((u8 *) src)[i] = (u8)i;

	memset(dest, 0, IOP_ADMA_TEST_SIZE);

	/* Start copy, using first DMA channel */
	dma_chan = container_of(device->common.channels.next,
	                        struct dma_chan,
	                        device_node);
	if (iop_adma_alloc_chan_resources(dma_chan) < 1) {
		err = -ENODEV;
		goto out;
	}

	tx = iop_adma_prep_dma_memcpy(dma_chan, IOP_ADMA_TEST_SIZE, 1);
	dest_dma = iop_adma_map_single(dma_chan, dest, IOP_ADMA_TEST_SIZE, DMA_FROM_DEVICE);
	iop_adma_set_dest(dest_dma, tx, 0);
	src_dma = iop_adma_map_single(dma_chan, src, IOP_ADMA_TEST_SIZE, DMA_TO_DEVICE);
	iop_adma_set_src(src_dma, tx, 0);

	cookie = iop_adma_tx_submit(tx);
	iop_adma_issue_pending(dma_chan);
	async_tx_ack(tx);
	msleep(1);

	if (iop_adma_is_complete(dma_chan, cookie, NULL, NULL) != DMA_SUCCESS) {
		printk(KERN_ERR "iop adma%d: Self-test copy timed out, disabling\n",
			device->id);
		err = -ENODEV;
		goto free_resources;
	}

	iop_chan = to_iop_adma_chan(dma_chan);
	dma_sync_single_for_cpu(&iop_chan->device->pdev->dev, dest_dma,
		IOP_ADMA_TEST_SIZE, DMA_FROM_DEVICE);
	if (memcmp(src, dest, IOP_ADMA_TEST_SIZE)) {
		printk(KERN_ERR "iop adma%d: Self-test copy failed compare, disabling\n",
			device->id);
		err = -ENODEV;
		goto free_resources;
	}

free_resources:
	iop_adma_free_chan_resources(dma_chan);
out:
	kfree(src);
	kfree(dest);
	return err;
}

#define IOP_ADMA_NUM_SRC_TEST 4 /* must be <= 15 */
static int __devinit iop_adma_xor_zero_sum_self_test(struct iop_adma_device *device)
{
	int i, src_idx;
	struct page *dest;
	struct page *xor_srcs[IOP_ADMA_NUM_SRC_TEST];
	struct page *zero_sum_srcs[IOP_ADMA_NUM_SRC_TEST + 1];
	dma_addr_t dma_addr, dest_dma;
	struct dma_async_tx_descriptor *tx;
	struct dma_chan *dma_chan;
	dma_cookie_t cookie;
	u8 cmp_byte = 0;
	u32 cmp_word;
	u32 zero_sum_result;
	int err = 0;
	struct iop_adma_chan *iop_chan;

	PRINTK("iop adma%d: %s\n", device->id, __FUNCTION__);

	for (src_idx = 0; src_idx < IOP_ADMA_NUM_SRC_TEST; src_idx++) {
		xor_srcs[src_idx] = alloc_page(GFP_KERNEL);
		if (!xor_srcs[src_idx])
			while (src_idx--) {
				__free_page(xor_srcs[src_idx]);
				return -ENOMEM;
			}
	}

	dest = alloc_page(GFP_KERNEL);
	if (!dest)
		while (src_idx--) {
			__free_page(xor_srcs[src_idx]);
			return -ENOMEM;
		}

	/* Fill in src buffers */
	for (src_idx = 0; src_idx < IOP_ADMA_NUM_SRC_TEST; src_idx++) {
		u8 *ptr = page_address(xor_srcs[src_idx]);
		for (i = 0; i < PAGE_SIZE; i++)
			ptr[i] = (1 << src_idx);
	}

	for (src_idx = 0; src_idx < IOP_ADMA_NUM_SRC_TEST; src_idx++)
		cmp_byte ^= (u8) (1 << src_idx);

	cmp_word = (cmp_byte << 24) | (cmp_byte << 16) | (cmp_byte << 8) | cmp_byte;

	memset(page_address(dest), 0, PAGE_SIZE);

	dma_chan = container_of(device->common.channels.next,
	                        struct dma_chan,
	                        device_node);
	if (iop_adma_alloc_chan_resources(dma_chan) < 1) {
		err = -ENODEV;
		goto out;
	}

	/* test xor */
	tx = iop_adma_prep_dma_xor(dma_chan, IOP_ADMA_NUM_SRC_TEST, PAGE_SIZE, 1);
	dest_dma = iop_adma_map_page(dma_chan, dest, 0, PAGE_SIZE, DMA_FROM_DEVICE);
	iop_adma_set_dest(dest_dma, tx, 0);

	for (i = 0; i < IOP_ADMA_NUM_SRC_TEST; i++) {
		dma_addr = iop_adma_map_page(dma_chan, xor_srcs[i], 0,
			PAGE_SIZE, DMA_TO_DEVICE);
		iop_adma_set_src(dma_addr, tx, i);
	}

	cookie = iop_adma_tx_submit(tx);
	iop_adma_issue_pending(dma_chan);
	async_tx_ack(tx);
	msleep(8);

	if (iop_adma_is_complete(dma_chan, cookie, NULL, NULL) != DMA_SUCCESS) {
		printk(KERN_ERR "iop_adma: Self-test xor timed out, disabling\n");
		err = -ENODEV;
		goto free_resources;
	}

	iop_chan = to_iop_adma_chan(dma_chan);
	dma_sync_single_for_cpu(&iop_chan->device->pdev->dev, dest_dma,
		PAGE_SIZE, DMA_FROM_DEVICE);
	for (i = 0; i < (PAGE_SIZE / sizeof(u32)); i++) {
		u32 *ptr = page_address(dest);
		if (ptr[i] != cmp_word) {
			printk(KERN_ERR "iop_adma: Self-test xor failed compare, disabling\n");
			err = -ENODEV;
			goto free_resources;
		}
	}
	dma_sync_single_for_device(&iop_chan->device->pdev->dev, dest_dma,
		PAGE_SIZE, DMA_TO_DEVICE);

	/* skip zero sum if the capability is not present */
	if (!test_bit(DMA_ZERO_SUM, &dma_chan->device->capabilities))
		goto free_resources;

	/* zero sum the sources with the destintation page */
	for (i = 0; i < IOP_ADMA_NUM_SRC_TEST; i++)
		zero_sum_srcs[i] = xor_srcs[i];
	zero_sum_srcs[i] = dest;

	zero_sum_result = 1;

	tx = iop_adma_prep_dma_zero_sum(dma_chan, IOP_ADMA_NUM_SRC_TEST + 1,
		PAGE_SIZE, &zero_sum_result, 1);
	for (i = 0; i < IOP_ADMA_NUM_SRC_TEST + 1; i++) {
		dma_addr = iop_adma_map_page(dma_chan, zero_sum_srcs[i], 0,
			PAGE_SIZE, DMA_TO_DEVICE);
		iop_adma_set_src(dma_addr, tx, i);
	}

	cookie = iop_adma_tx_submit(tx);
	iop_adma_issue_pending(dma_chan);
	async_tx_ack(tx);
	msleep(8);

	if (iop_adma_is_complete(dma_chan, cookie, NULL, NULL) != DMA_SUCCESS) {
		printk(KERN_ERR "iop_adma: Self-test zero sum timed out, disabling\n");
		err = -ENODEV;
		goto free_resources;
	}

	if (zero_sum_result != 0) {
		printk(KERN_ERR "iop_adma: Self-test zero sum failed compare, disabling\n");
		err = -ENODEV;
		goto free_resources;
	}

	/* test memset */
	tx = iop_adma_prep_dma_memset(dma_chan, 0, PAGE_SIZE, 1);
	dma_addr = iop_adma_map_page(dma_chan, dest, 0, PAGE_SIZE, DMA_FROM_DEVICE);
	iop_adma_set_dest(dma_addr, tx, 0);

	cookie = iop_adma_tx_submit(tx);
	iop_adma_issue_pending(dma_chan);
	async_tx_ack(tx);
	msleep(8);

	if (iop_adma_is_complete(dma_chan, cookie, NULL, NULL) != DMA_SUCCESS) {
		printk(KERN_ERR "iop_adma: Self-test memset timed out, disabling\n");
		err = -ENODEV;
		goto free_resources;
	}

	for (i = 0; i < PAGE_SIZE/sizeof(u32); i++) {
		u32 *ptr = page_address(dest);
		if (ptr[i]) {
			printk(KERN_ERR "iop_adma: Self-test memset failed compare, disabling\n");
			err = -ENODEV;
			goto free_resources;
		}
	}

	/* test for non-zero parity sum */
	zero_sum_result = 0;
	tx = iop_adma_prep_dma_zero_sum(dma_chan, IOP_ADMA_NUM_SRC_TEST + 1,
		PAGE_SIZE, &zero_sum_result, 1);
	for (i = 0; i < IOP_ADMA_NUM_SRC_TEST + 1; i++) {
		dma_addr = iop_adma_map_page(dma_chan, zero_sum_srcs[i], 0,
			PAGE_SIZE, DMA_TO_DEVICE);
		iop_adma_set_src(dma_addr, tx, i);
	}

	cookie = iop_adma_tx_submit(tx);
	iop_adma_issue_pending(dma_chan);
	async_tx_ack(tx);
	msleep(8);

	if (iop_adma_is_complete(dma_chan, cookie, NULL, NULL) != DMA_SUCCESS) {
		printk(KERN_ERR "iop_adma: Self-test non-zero sum timed out, disabling\n");
		err = -ENODEV;
		goto free_resources;
	}

	if (zero_sum_result != 1) {
		printk(KERN_ERR "iop_adma: Self-test non-zero sum failed compare, disabling\n");
		err = -ENODEV;
		goto free_resources;
	}

free_resources:
	iop_adma_free_chan_resources(dma_chan);
out:
	src_idx = IOP_ADMA_NUM_SRC_TEST;
	while (src_idx--)
		__free_page(xor_srcs[src_idx]);
	__free_page(dest);
	return err;
}

static int __devexit iop_adma_remove(struct platform_device *dev)
{
	struct iop_adma_device *device = platform_get_drvdata(dev);
	struct dma_chan *chan, *_chan;
	struct iop_adma_chan *iop_chan;
	int i;
	struct iop_adma_platform_data *plat_data = dev->dev.platform_data;

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
		iop_chan = to_iop_adma_chan(chan);
		list_del(&chan->device_node);
		kfree(iop_chan);
	}
	kfree(device);

	return 0;
}

static int __devinit iop_adma_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret=0, irq_eot=0, irq_eoc=0, irq_err=0, irq, i;
	struct iop_adma_device *adev;
	struct iop_adma_chan *iop_chan;
	struct iop_adma_platform_data *plat_data = pdev->dev.platform_data;

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
	if ((adev->dma_desc_pool_virt = dma_alloc_writecombine(&pdev->dev,
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
	iop_adma_device_clear_err_status(adev->id);

	for (i = 0; i < 3; i++) {
		irq = platform_get_irq(pdev, i);
		if (irq < 0)
			ret = -ENXIO;
		else {
			switch (i) {
			case 0:
				irq_eot = irq;
				ret = request_irq(irq, iop_adma_eot_handler,
					 0, pdev->name, &adev->id);
				if (ret) {
					ret = -EIO;
					goto err_irq0;
				}
				break;
			case 1:
				irq_eoc = irq;
				ret = request_irq(irq, iop_adma_eoc_handler,
					0, pdev->name, &adev->id);
				if (ret) {
					ret = -EIO;
					goto err_irq1;
				}
				break;
			case 2:
				irq_err = irq;
				ret = request_irq(irq, iop_adma_err_handler,
					0, pdev->name, &adev->id);
				if (ret) {
					ret = -EIO;
					goto err_irq2;
				}
				break;
			}
		}
	}

	adev->pdev = pdev;
	platform_set_drvdata(pdev, adev);

	INIT_LIST_HEAD(&adev->common.channels);

	/* set base routines */
	adev->common.device_tx_submit = iop_adma_tx_submit;
	adev->common.device_set_dest = iop_adma_set_dest;
	adev->common.device_set_src = iop_adma_set_src;
	adev->common.device_alloc_chan_resources = iop_adma_alloc_chan_resources;
	adev->common.device_free_chan_resources = iop_adma_free_chan_resources;
	adev->common.device_is_tx_complete = iop_adma_is_complete;
	adev->common.device_issue_pending = iop_adma_issue_pending;
	adev->common.device_dependency_added = iop_adma_dependency_added;
	adev->common.map_page = iop_adma_map_page;
	adev->common.map_single = iop_adma_map_single;
	adev->common.unmap_page = iop_adma_unmap_page;
	adev->common.unmap_single = iop_adma_unmap_single;

	/* set prep routines based on capability */
	if (test_bit(DMA_MEMCPY, &adev->common.capabilities))
		adev->common.device_prep_dma_memcpy = iop_adma_prep_dma_memcpy;
	if (test_bit(DMA_MEMSET, &adev->common.capabilities))
		adev->common.device_prep_dma_memset = iop_adma_prep_dma_memset;
	if (test_bit(DMA_XOR, &adev->common.capabilities)) {
		adev->common.max_xor = iop_adma_get_max_xor();
		adev->common.device_prep_dma_xor = iop_adma_prep_dma_xor;
	}
	if (test_bit(DMA_ZERO_SUM, &adev->common.capabilities))
		adev->common.device_prep_dma_zero_sum =
			iop_adma_prep_dma_zero_sum;
	if (test_bit(DMA_INTERRUPT, &adev->common.capabilities))
		adev->common.device_prep_dma_interrupt =
			iop_adma_prep_dma_interrupt;

	if ((iop_chan = kzalloc(sizeof(*iop_chan), GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto err_chan_alloc;
	}

	iop_adma_chan_array[adev->id] = iop_chan;

	iop_chan->device = adev;
	spin_lock_init(&iop_chan->lock);
	init_timer(&iop_chan->cleanup_watchdog);
	iop_chan->cleanup_watchdog.data = adev->id;
	iop_chan->cleanup_watchdog.function = iop_adma_schedule_cleanup;
	INIT_LIST_HEAD(&iop_chan->chain);
	INIT_LIST_HEAD(&iop_chan->all_slots);
	INIT_RCU_HEAD(&iop_chan->common.rcu);
	iop_chan->common.device = &adev->common;
	list_add_tail(&iop_chan->common.device_node, &adev->common.channels);

	if (test_bit(DMA_MEMCPY, &adev->common.capabilities)) {
		ret = iop_adma_memcpy_self_test(adev);
		PRINTK("iop adma%d: memcpy self test returned %d\n", adev->id, ret);
		if (ret)
			goto err_self_test;
	}

	if (test_bit(DMA_XOR, &adev->common.capabilities) ||
		test_bit(DMA_MEMSET, &adev->common.capabilities)) {
		ret = iop_adma_xor_zero_sum_self_test(adev);
		PRINTK("iop adma%d: xor self test returned %d\n", adev->id, ret);
		if (ret)
			goto err_self_test;
	}

	printk(KERN_INFO "Intel(R) IOP ADMA Engine found [%d]: "
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

err_self_test:
	kfree(iop_chan);
err_chan_alloc:
err_irq2:
	free_irq(irq_eoc, adev);
err_irq1:
	free_irq(irq_eot, adev);
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

static void iop_chan_start_null_memcpy(struct iop_adma_chan *iop_chan)
{
	struct iop_adma_desc_slot *sw_desc, *group_start;
	dma_cookie_t cookie;
	int slot_cnt, slots_per_op;

	PRINTK("iop adma%d: %s\n", iop_chan->device->id, __FUNCTION__);

	spin_lock_bh(&iop_chan->lock);
	slot_cnt = iop_chan_memcpy_slot_count(0, &slots_per_op);
	sw_desc = iop_adma_alloc_slots(iop_chan, slot_cnt, slots_per_op);
	if (sw_desc) {
		group_start = sw_desc->group_head;

		list_splice_init(&sw_desc->group_list, &iop_chan->chain);
		sw_desc->async_tx.ack = 1;
		iop_desc_init_memcpy(group_start, 0);
		iop_desc_set_byte_count(group_start, iop_chan, 0);
		iop_desc_set_dest_addr(group_start, iop_chan, 0);
		iop_desc_set_memcpy_src_addr(group_start, 0, slot_cnt, slots_per_op);

		cookie = iop_chan->common.cookie;
		cookie++;
		if (cookie <= 1)
			cookie = 2;

		/* initialize the completed cookie to be less than
		 * the most recently used cookie
		 */
		iop_chan->completed_cookie = cookie - 1;
		iop_chan->common.cookie = sw_desc->async_tx.cookie = cookie;

		/* channel should not be busy */
		BUG_ON(iop_chan_is_busy(iop_chan));

		/* clear any prior error-status bits */
		iop_adma_device_clear_err_status(iop_chan->device->id);

		/* disable operation */
		iop_chan_disable(iop_chan);

		/* set the descriptor address */
		iop_chan_set_next_descriptor(iop_chan, sw_desc->phys);

		/* run the descriptor */
		iop_chan_enable(iop_chan);
	} else
		printk(KERN_ERR "iop adma%d failed to allocate null descriptor\n",
			iop_chan->device->id);
	spin_unlock_bh(&iop_chan->lock);
}

static void iop_chan_start_null_xor(struct iop_adma_chan *iop_chan)
{
	struct iop_adma_desc_slot *sw_desc, *group_start;
	dma_cookie_t cookie;
	int slot_cnt, slots_per_op;

	PRINTK("iop adma%d: %s\n", iop_chan->device->id, __FUNCTION__);

	spin_lock_bh(&iop_chan->lock);
	slot_cnt = iop_chan_xor_slot_count(0, 2, &slots_per_op);
	sw_desc = iop_adma_alloc_slots(iop_chan, slot_cnt, slots_per_op);
	if (sw_desc) {
		group_start = sw_desc->group_head;
		list_splice_init(&sw_desc->group_list, &iop_chan->chain);
		sw_desc->async_tx.ack = 1;
		iop_desc_init_null_xor(group_start, 2, 0);
		iop_desc_set_byte_count(group_start, iop_chan, 0);
		iop_desc_set_dest_addr(group_start, iop_chan, 0);
		iop_desc_set_xor_src_addr(group_start, 0, 0, slot_cnt, slots_per_op);
		iop_desc_set_xor_src_addr(group_start, 1, 0, slot_cnt, slots_per_op);

		cookie = iop_chan->common.cookie;
		cookie++;
		if (cookie <= 1)
			cookie = 2;

		/* initialize the completed cookie to be less than
		 * the most recently used cookie
		 */
		iop_chan->completed_cookie = cookie - 1;
		iop_chan->common.cookie = sw_desc->async_tx.cookie = cookie;

		/* channel should not be busy */
		BUG_ON(iop_chan_is_busy(iop_chan));

		/* clear any prior error-status bits */
		iop_adma_device_clear_err_status(iop_chan->device->id);

		/* disable operation */
		iop_chan_disable(iop_chan);

		/* set the descriptor address */
		iop_chan_set_next_descriptor(iop_chan, sw_desc->phys);

		/* run the descriptor */
		iop_chan_enable(iop_chan);
	} else
		printk(KERN_ERR "iop adma%d failed to allocate null descriptor\n",
			iop_chan->device->id);
	spin_unlock_bh(&iop_chan->lock);
}

static struct platform_driver iop_adma_driver = {
	.probe		= iop_adma_probe,
	.remove		= iop_adma_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "IOP-ADMA",
	},
};

static int __init iop_adma_init (void)
{
	/* it's currently unsafe to unload this module */
	/* if forced, worst case is that rmmod hangs */
	__unsafe(THIS_MODULE);

	return platform_driver_register(&iop_adma_driver);
}

static void __exit iop_adma_exit (void)
{
	platform_driver_unregister(&iop_adma_driver);
	return;
}

module_init(iop_adma_init);
module_exit(iop_adma_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("IOP ADMA Engine Driver");
MODULE_LICENSE("GPL");
