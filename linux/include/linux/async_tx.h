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
#include <linux/dmaengine.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

struct dma_chan_ref {
	struct dma_chan *chan;
	struct list_head async_node;
	struct rcu_head rcu;
};

struct async_iter_percpu {
	struct list_head *iter;
	unsigned long local_version;
};

struct async_channel_entry {
	struct list_head list;
	spinlock_t lock;
	struct async_iter_percpu *local_iter;
	atomic_t version;
};

/**
 * async_tx_flags - modifiers for the async_* calls
 * @ASYNC_TX_XOR_ZERO_DST: for synchronous xor: zero the destination
 *	asynchronous assumes a pre-zeroed destination
 * @ASYNC_TX_XOR_DROP_DST: for synchronous xor: drop source index zero (dest)
 *	the dest is an implicit source to the synchronous routine
 * @ASYNC_TX_ASSUME_COHERENT: skip cache maintenance operations
 * @ASYNC_TX_ACK: immediately ack the descriptor, preclude setting up a
 *	dependency chain
 * @ASYNC_TX_DEP_ACK: ack the dependency
 * @ASYNC_TX_INT_EN: have the dma engine trigger an interrupt on completion
 * @ASYNC_TX_KMAP_SRC: take an atomic mapping (KM_USER0) on the source page(s)
 *	if the transaction is to be performed synchronously
 * @ASYNC_TX_KMAP_DST: take an atomic mapping (KM_USER0) on the dest page(s)
 *	if the transaction is to be performed synchronously
 */
enum async_tx_flags {
	ASYNC_TX_XOR_ZERO_DST	 = (1 << 0),
	ASYNC_TX_XOR_DROP_DST	 = (1 << 1),
	ASYNC_TX_ASSUME_COHERENT = (1 << 2),
	ASYNC_TX_ACK		 = (1 << 3),
	ASYNC_TX_DEP_ACK	 = (1 << 4),
	ASYNC_TX_INT_EN		 = (1 << 5),
	ASYNC_TX_KMAP_SRC	 = (1 << 6),
	ASYNC_TX_KMAP_DST	 = (1 << 7),
};

#ifdef CONFIG_DMA_ENGINE
static inline enum dma_status
dma_wait_for_async_tx(struct dma_async_tx_descriptor *tx)
{
	enum dma_status status;
	struct dma_async_tx_descriptor *iter;

	if (!tx)
		return DMA_SUCCESS;

	/* poll through the dependency chain, return when tx is complete */
	do {
		iter = tx;
		while (iter->cookie == -EBUSY)
			iter = iter->parent;

		status = dma_sync_wait(iter->chan, iter->cookie);
	} while (status == DMA_IN_PROGRESS || (iter != tx));

	return status;
}

extern struct async_channel_entry async_tx_master_list;
static inline void async_tx_issue_pending_all(void)
{
	struct dma_chan_ref *ref;

	rcu_read_lock();
	list_for_each_entry_rcu(ref, &async_tx_master_list.list, async_node)
		ref->chan->device->device_issue_pending(ref->chan);
	rcu_read_unlock();
}

static inline void
async_tx_run_dependencies(struct dma_async_tx_descriptor *tx,
	struct dma_chan *host_chan)
{
	struct dma_async_tx_descriptor *dep_tx, *_dep_tx;
	struct dma_device *dev;
	struct dma_chan *chan;

	list_for_each_entry_safe(dep_tx, _dep_tx, &tx->depend_list,
		depend_node) {
		chan = dep_tx->chan;
		dev = chan->device;
		/* we can't depend on ourselves */
		BUG_ON(chan == host_chan);
		list_del(&dep_tx->depend_node);
		dev->device_tx_submit(dep_tx);

		/* we need to poke the engine as client code does not
		 * know about dependency submission events
		 */
		dev->device_issue_pending(chan);
	}
}
#else
static inline void
async_tx_run_dependencies(struct dma_async_tx_descriptor *tx,
	struct dma_chan *host_chan)
{
	do { } while (0);
}

static inline enum dma_status
dma_wait_for_async_tx(struct dma_async_tx_descriptor *tx)
{
	return DMA_SUCCESS;
}

static inline void async_tx_issue_pending_all(void)
{
	do { } while (0);
}
#endif

static inline void
async_tx_ack(struct dma_async_tx_descriptor *tx)
{
	tx->ack = 1;
}

struct dma_async_tx_descriptor *
async_xor(struct page *dest, struct page **src_list, unsigned int offset,
	unsigned int src_cnt, size_t len, enum async_tx_flags flags,
	struct dma_async_tx_descriptor *depend_tx,
	dma_async_tx_callback callback, void *callback_param);
struct dma_async_tx_descriptor *
async_xor_zero_sum(struct page *dest, struct page **src_list,
	unsigned int offset, unsigned int src_cnt, size_t len,
	u32 *result, enum async_tx_flags flags,
	struct dma_async_tx_descriptor *depend_tx,
	dma_async_tx_callback callback, void *callback_param);
struct dma_async_tx_descriptor *
async_memcpy(struct page *dest, struct page *src, unsigned int dest_offset,
	unsigned int src_offset, size_t len, enum async_tx_flags flags,
	struct dma_async_tx_descriptor *depend_tx,
	dma_async_tx_callback callback, void *callback_param);
struct dma_async_tx_descriptor *
async_memset(struct page *dest, int val, unsigned int offset,
	size_t len, enum async_tx_flags flags,
	struct dma_async_tx_descriptor *depend_tx,
	dma_async_tx_callback callback, void *callback_param);
struct dma_async_tx_descriptor *
async_interrupt(enum async_tx_flags flags,
	struct dma_async_tx_descriptor *depend_tx,
	dma_async_tx_callback callback, void *callback_param);
struct dma_async_tx_descriptor *
async_interrupt_cond(enum dma_transaction_type next_op,
	enum async_tx_flags flags,
	struct dma_async_tx_descriptor *depend_tx);
