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
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/raid/xor.h>
#include <linux/async_tx.h>

#define ASYNC_TX_DEBUG 0
#define PRINTK(x...) ((void)(ASYNC_TX_DEBUG && printk(x)))

#ifdef CONFIG_DMA_ENGINE
static struct dma_client *async_api_client;
static struct async_channel_entry async_channel_directory[] = {
	[DMA_MEMCPY] = { .list =
		LIST_HEAD_INIT(async_channel_directory[DMA_MEMCPY].list), },
	[DMA_XOR] = { .list =
		LIST_HEAD_INIT(async_channel_directory[DMA_XOR].list), },
	[DMA_PQ_XOR] = { .list =
		LIST_HEAD_INIT(async_channel_directory[DMA_PQ_XOR].list), },
	[DMA_DUAL_XOR] = { .list =
		LIST_HEAD_INIT(async_channel_directory[DMA_DUAL_XOR].list), },
	[DMA_PQ_UPDATE] = { .list =
		LIST_HEAD_INIT(async_channel_directory[DMA_PQ_UPDATE].list), },
	[DMA_ZERO_SUM] = { .list =
		LIST_HEAD_INIT(async_channel_directory[DMA_ZERO_SUM].list), },
	[DMA_PQ_ZERO_SUM] = { .list =
		LIST_HEAD_INIT(async_channel_directory[DMA_PQ_ZERO_SUM].list), },
	[DMA_MEMSET] = { .list =
		LIST_HEAD_INIT(async_channel_directory[DMA_MEMSET].list), },
	[DMA_MEMCPY_CRC32C] = { .list =
		LIST_HEAD_INIT(async_channel_directory[DMA_MEMCPY_CRC32C].list), },
	[DMA_INTERRUPT] = { .list =
		LIST_HEAD_INIT(async_channel_directory[DMA_INTERRUPT].list), },
};

struct async_channel_entry async_tx_master_list = {
	.list = LIST_HEAD_INIT(async_tx_master_list.list),
};
EXPORT_SYMBOL_GPL(async_tx_master_list);

static void
free_dma_chan_ref(struct rcu_head *rcu)
{
	struct dma_chan_ref *ref;
	ref = container_of(rcu, struct dma_chan_ref, rcu);
	dma_chan_put(ref->chan);
	kfree(ref);
}

static inline void
init_dma_chan_ref(struct dma_chan_ref *ref, struct dma_chan *chan)
{
	INIT_LIST_HEAD(&ref->async_node);
	INIT_RCU_HEAD(&ref->rcu);
	ref->chan = chan;
}

static void
dma_channel_add_remove(struct dma_client *client,
	struct dma_chan *chan, enum dma_event event)
{
	unsigned long i, flags;
	struct dma_chan_ref *master_ref, *ref;
	struct async_channel_entry *channel_entry;

	switch (event) {
	case DMA_RESOURCE_ADDED:
		PRINTK("async_tx: dma resource added (capabilities: %#lx)\n",
			chan->device->capabilities);
		/* add the channel to the generic management list */
		master_ref = kmalloc(sizeof(*master_ref), GFP_KERNEL);
		if (master_ref) {
			/* keep a reference until async_tx is unloaded */
			dma_chan_get(chan);
			init_dma_chan_ref(master_ref, chan);
			spin_lock_irqsave(&async_tx_master_list.lock, flags);
			list_add_tail_rcu(&master_ref->async_node,
				&async_tx_master_list.list);
			spin_unlock_irqrestore(&async_tx_master_list.lock,
				flags);
		} else {
			printk(KERN_WARNING "async_tx: unable to create"
				" new master entry in response to"
				" a DMA_RESOURCE_ADDED event"
				" (-ENOMEM)\n");
			return;
		}

		/* add an entry for each capability of this channel */
		dma_async_for_each_tx_type(i) {
			if (test_bit(i, &chan->device->capabilities))
				ref = kmalloc(sizeof(*ref), GFP_KERNEL);
			else
				continue;

			if (ref) {
				channel_entry = &async_channel_directory[i];
				init_dma_chan_ref(ref, chan);
				spin_lock_irqsave(&channel_entry->lock, flags);
				atomic_inc(&channel_entry->version);
				list_add_tail_rcu(&ref->async_node,
					&channel_entry->list);
				spin_unlock_irqrestore(&channel_entry->lock,
					flags);
			} else {
				printk(KERN_WARNING "async_tx: unable to create"
					" new op-specific entry in response to"
					" a DMA_RESOURCE_ADDED event"
					" (-ENOMEM)\n");
				return;
			}
		}
		break;
	case DMA_RESOURCE_REMOVED:
		PRINTK("async_tx: dma resource removed (capabilities: %#lx)\n",
			chan->device->capabilities);
		dma_async_for_each_tx_type(i) {
			if (!test_bit(i, &chan->device->capabilities))
				continue;

			channel_entry = &async_channel_directory[i];

			spin_lock_irqsave(&channel_entry->lock, flags);
			list_for_each_entry_rcu(ref, &channel_entry->list,
				async_node) {
				if (ref->chan == chan) {
					atomic_inc(&channel_entry->version);
					list_del_rcu(&ref->async_node);
					call_rcu(&ref->rcu, free_dma_chan_ref);
					break;
				}
			}
			spin_unlock_irqrestore(&channel_entry->lock, flags);
		}
		break;
	case DMA_RESOURCE_SUSPEND:
	case DMA_RESOURCE_RESUME:
		printk(KERN_WARNING "async_tx: does not support dma channel"
			" suspend/resume\n");
		break;
	default:
		BUG();
	}
}

static int __init
async_tx_init(void)
{
	unsigned long i;
	struct async_channel_entry *channel_entry;
	int cpu;

	spin_lock_init(&async_tx_master_list.lock);

	dma_async_for_each_tx_type(i) {
		channel_entry = &async_channel_directory[i];
		spin_lock_init(&channel_entry->lock);
		channel_entry->local_iter = alloc_percpu(struct async_iter_percpu);
		if (!channel_entry->local_iter) {
			i++;
			goto err;
		}

		atomic_set(&channel_entry->version, 0);

		for_each_possible_cpu(cpu) {
			struct async_iter_percpu *local_iter =
				channel_entry->local_iter;
			per_cpu_ptr(local_iter, cpu)->iter = &channel_entry->list;
			per_cpu_ptr(local_iter, cpu)->local_version = 0;
		}
	}

	async_api_client = dma_async_client_register(dma_channel_add_remove);

	if (!async_api_client)
		goto err;

	dma_async_client_chan_request(async_api_client, DMA_CHAN_REQUEST_ALL);

	printk("async_tx: api initialized (async)\n");

	return 0;
err:
	printk("async_tx: initialization failure\n");

	while (--i >= 0)
		free_percpu(async_channel_directory[i].local_iter);

	return 1;
}

static void __exit async_tx_exit(void)
{
	unsigned long i, flags;
	struct async_channel_entry *channel_entry;
	struct dma_chan_ref *ref;

	if (async_api_client)
		dma_async_client_unregister(async_api_client);

	dma_async_for_each_tx_type(i) {
		channel_entry = &async_channel_directory[i];
		if (channel_entry->local_iter)
			free_percpu(channel_entry->local_iter);

		/* free all the per operation channel references */
		spin_lock_irqsave(&channel_entry->lock, flags);
		list_for_each_entry_rcu(ref, &channel_entry->list, async_node) {
			list_del_rcu(&ref->async_node);
			call_rcu(&ref->rcu, free_dma_chan_ref);
		}
		spin_unlock_irqrestore(&channel_entry->lock, flags);
	}

	/* free all the channels on the master list */
	spin_lock_irqsave(&async_tx_master_list.lock, flags);
	list_for_each_entry_rcu(ref, &async_tx_master_list.list, async_node) {
		dma_chan_put(ref->chan); /* permit backing devices to go away */
		list_del_rcu(&ref->async_node);
		call_rcu(&ref->rcu, free_dma_chan_ref);
	}
	spin_unlock_irqrestore(&async_tx_master_list.lock, flags);
}

/**
 * async_tx_find_channel - find a channel to carry out the operation or let
 *	the transaction execute synchronously
 * @depend_tx: transaction dependency
 * @tx_type: transaction type
 */
static struct dma_chan *
async_tx_find_channel(struct dma_async_tx_descriptor *depend_tx,
	enum dma_transaction_type tx_type)
{
	/* see if we can keep the chain on one channel */
	if (depend_tx &&
		test_bit(tx_type, &depend_tx->chan->device->capabilities))
		return depend_tx->chan;
	else {
		int cpu;
		struct async_channel_entry *channel_entry =
			&async_channel_directory[tx_type];
		struct async_iter_percpu *local_iter;
		struct list_head *iter;
		struct dma_chan *chan;

		rcu_read_lock();
		if (list_empty(&channel_entry->list)) {
			rcu_read_unlock();
			return NULL;
		}

		cpu = get_cpu();
		local_iter = per_cpu_ptr(channel_entry->local_iter, cpu);
		put_cpu();

		/* ensure the percpu place holder is pointing to a
		 * valid list entry and get the next channel in the
		 * round robin
		 */
		if (unlikely(local_iter->local_version !=
			atomic_read(&channel_entry->version))) {
			local_iter->local_version =
				atomic_read(&channel_entry->version);
			iter = channel_entry->list.next;
		} else {
			iter = local_iter->iter->next;
			/* wrap around detect */
			if (iter == &channel_entry->list)
				iter = iter->next;
		}

		/* if we are still pointing to the head then the list
		 * recently became empty
		 */
		if (iter == &channel_entry->list)
			chan = NULL;
		else {
			local_iter->iter = iter;
			chan = list_entry(iter, struct dma_chan_ref, async_node)->chan;
		}
		rcu_read_unlock();

		return chan;
	}
}
#else
static int __init async_tx_init(void)
{
	printk("async_tx: api initialized (sync-only)\n");
	return 0;
}

static void __exit async_tx_exit(void)
{
	do { } while (0);
}

static inline struct dma_chan *
async_tx_find_channel(struct dma_async_tx_descriptor *depend_tx,
	enum dma_transaction_type tx_type)
{
	return NULL;
}
#endif

static inline void
async_tx_submit(struct dma_chan *chan, struct dma_async_tx_descriptor *tx,
	enum async_tx_flags flags, struct dma_async_tx_descriptor *depend_tx,
	dma_async_tx_callback callback, void *callback_param)
{
	tx->callback = callback;
	tx->callback_param = callback_param;

	/* set this new tx to run after depend_tx if:
	 * 1/ a dependency exists (depend_tx is !NULL)
	 * 2/ the tx can not be submitted to the current channel
	 */
	if (depend_tx && depend_tx->chan != chan) {
		/* if ack is already set then we cannot be sure
		 * we are referring to the correct operation
		 */
		BUG_ON(depend_tx->ack);

		tx->parent = depend_tx;
		spin_lock_bh(&depend_tx->lock);
		list_add_tail(&tx->depend_node, &depend_tx->depend_list);
		if (depend_tx->cookie == 0) {
			struct dma_chan *dep_chan = depend_tx->chan;
			struct dma_device *dep_dev = dep_chan->device;
			dep_dev->device_dependency_added(dep_chan);
		}
		spin_unlock_bh(&depend_tx->lock);
	} else {
		tx->parent = NULL;
		chan->device->device_tx_submit(tx);
	}

	if (flags & ASYNC_TX_ACK)
		async_tx_ack(tx);

	if (depend_tx && (flags & ASYNC_TX_DEP_ACK))
		async_tx_ack(depend_tx);
}

/**
 * sync_epilog - actions to take if an operation is run synchronously
 * @flags: async_tx flags
 * @depend_tx: transaction depends on depend_tx
 * @callback: function to call when the transaction completes
 * @callback_param: parameter to pass to the callback routine
 */
static inline void
sync_epilog(unsigned long flags, struct dma_async_tx_descriptor *depend_tx,
	dma_async_tx_callback callback, void *callback_param)
{
	if (callback)
		callback(callback_param);

	if (depend_tx && (flags & ASYNC_TX_DEP_ACK))
		async_tx_ack(depend_tx);
}

static inline void
do_async_xor(struct dma_async_tx_descriptor *tx, struct dma_device *device,
	struct dma_chan *chan, struct page *dest, struct page **src_list,
	unsigned int offset, unsigned int src_cnt, size_t len,
	enum async_tx_flags flags, struct dma_async_tx_descriptor *depend_tx,
	dma_async_tx_callback callback, void *callback_param)
{
	dma_addr_t dma_addr;
	enum dma_data_direction dir;
	int i;

	PRINTK("%s: len: %u\n", __FUNCTION__, len);

	dir = (flags & ASYNC_TX_ASSUME_COHERENT) ?
		DMA_NONE : DMA_FROM_DEVICE;

	dma_addr = device->map_page(chan, dest, offset, len, dir);
     	device->device_set_dest(dma_addr, tx, 0);

	dir = (flags & ASYNC_TX_ASSUME_COHERENT) ?
		DMA_NONE : DMA_TO_DEVICE;

	for (i = 0; i < src_cnt; i++) {
		dma_addr = device->map_page(chan, src_list[i],
			offset, len, dir);
	     	device->device_set_src(dma_addr, tx, i);
	}

	async_tx_submit(chan, tx, flags, depend_tx, callback,
		callback_param);
}

static inline void
do_sync_xor(struct page *dest, struct page **src_list, unsigned int offset,
	unsigned int src_cnt, size_t len, enum async_tx_flags flags,
	struct dma_async_tx_descriptor *depend_tx,
	dma_async_tx_callback callback, void *callback_param)
{
	void *_dest;
	int i;

	PRINTK("%s: len: %u\n", __FUNCTION__, len);

	/* reuse the 'src_list' array to convert to buffer pointers */
	for (i = 0; i < src_cnt; i++)
		src_list[i] = (struct page *)
			(page_address(src_list[i]) + offset);

	/* set destination address */
	_dest = page_address(dest) + offset;

	if (flags & ASYNC_TX_XOR_ZERO_DST)
		memset(_dest, 0, len);

	xor_block(src_cnt, len, _dest,
		(void **) src_list);

	sync_epilog(flags, depend_tx, callback, callback_param);
}

/**
 * async_xor - attempt to xor a set of blocks with a dma engine.
 *	xor_block always uses the dest as a source so the ASYNC_TX_XOR_ZERO_DST
 *	flag must be set to not include dest data in the calculation.  The
 *	assumption with dma eninges is that they only use the destination
 *	buffer as a source when it is explicity specified in the source list.
 * @dest: destination page
 * @src_list: array of source pages (if the dest is also a source it must be
 *	at index zero).  The contents of this array may be overwritten.
 * @offset: offset in pages to start transaction
 * @src_cnt: number of source pages
 * @len: length in bytes
 * @flags: ASYNC_TX_XOR_ZERO_DST, ASYNC_TX_XOR_DROP_DEST,
 	ASYNC_TX_ASSUME_COHERENT, ASYNC_TX_ACK, ASYNC_TX_DEP_ACK
 * @depend_tx: xor depends on the result of this transaction.
 * @callback: function to call when the xor completes
 * @callback_param: parameter to pass to the callback routine
 */
struct dma_async_tx_descriptor *
async_xor(struct page *dest, struct page **src_list, unsigned int offset,
	unsigned int src_cnt, size_t len, enum async_tx_flags flags,
	struct dma_async_tx_descriptor *depend_tx,
	dma_async_tx_callback callback, void *callback_param)
{
	struct dma_chan *chan = async_tx_find_channel(depend_tx, DMA_XOR);
	struct dma_device *device = chan ? chan->device : NULL;
	struct dma_async_tx_descriptor *tx = NULL;
	dma_async_tx_callback _callback;
	void *_callback_param;
	unsigned long local_flags;
	int xor_src_cnt;
	int i = 0, src_off = 0, int_en;

	BUG_ON(src_cnt <= 1);

	while (src_cnt) {
		local_flags = flags;
		if (device) { /* run the xor asynchronously */
			xor_src_cnt = min(src_cnt, device->max_xor);
			/* if we are submitting additional xors
			 * only set the callback on the last transaction
			 */
			if (src_cnt > xor_src_cnt) {
				local_flags &= ~(ASYNC_TX_ACK | ASYNC_TX_INT_EN);
				_callback = NULL;
				_callback_param = NULL;
			} else {
				_callback = callback;
				_callback_param = callback_param;
			}

			int_en = (local_flags & ASYNC_TX_INT_EN) ? 1 : 0;

			tx = device->device_prep_dma_xor(
				chan, xor_src_cnt, len, int_en);

			if (tx) {
				do_async_xor(tx, device, chan, dest,
				&src_list[src_off], offset, xor_src_cnt, len,
				local_flags, depend_tx, _callback,
				_callback_param);
			} else /* fall through */
				goto xor_sync;
		} else { /* run the xor synchronously */
xor_sync:
			/* in the sync case the dest is an implied source
			 * (assumes the dest is at the src_off index)
			 */
			if (flags & ASYNC_TX_XOR_DROP_DST) {
				src_cnt--;
				src_off++;
			}

			/* process up to 'MAX_XOR_BLOCKS' sources */
			xor_src_cnt = min(src_cnt, (unsigned int) MAX_XOR_BLOCKS);

			/* if we are submitting additional xors
			 * only set the callback on the last transaction
			 */
			if (src_cnt > xor_src_cnt) {
				local_flags &= ~(ASYNC_TX_ACK | ASYNC_TX_INT_EN);
				_callback = NULL;
				_callback_param = NULL;
			} else {
				_callback = callback;
				_callback_param = callback_param;
			}

			/* wait for any prerequisite operations */
			if (depend_tx) {
				/* if ack is already set then we cannot be sure
				 * we are referring to the correct operation
				 */
				BUG_ON(depend_tx->ack);
				if (dma_wait_for_async_tx(depend_tx) == DMA_ERROR)
					panic("%s: DMA_ERROR waiting for depend_tx\n",
						__FUNCTION__);
			}

			do_sync_xor(dest, &src_list[src_off], offset,
				xor_src_cnt, len, local_flags, depend_tx,
				_callback, _callback_param);
		}

		/* the previous tx is hidden from the client,
		 * so ack it
		 */
		if (i && depend_tx)
			async_tx_ack(depend_tx);

		depend_tx = tx;

		if (src_cnt > xor_src_cnt) {
			/* drop completed sources */
			src_cnt -= xor_src_cnt;
			src_off += xor_src_cnt;

			/* unconditionally preserve the destination */
			flags &= ~ASYNC_TX_XOR_ZERO_DST;

			/* use the intermediate result a source, but remember
			 * it's dropped, because it's implied, in the sync case
			 */
			src_list[--src_off] = dest;
			src_cnt++;
			flags |= ASYNC_TX_XOR_DROP_DST;
		} else
			src_cnt = 0;
		i++;
	}

	return tx;
}

static int page_is_zero(struct page *p, size_t len)
{
	char *a = page_address(p);
	return ((*(u32*)a) == 0 &&
		memcmp(a, a+4, len-4)==0);
}

/**
 * async_xor_zero_sum - attempt a xor parity check with a dma engine.
 * @dest: destination page used if the xor is performed synchronously
 * @src_list: array of source pages.  The dest page must be listed as a source
 * 	at index zero.  The contents of this array may be overwritten.
 * @offset: offset in pages to start transaction
 * @src_cnt: number of source pages
 * @len: length in bytes
 * @result: 0 if sum == 0 else non-zero
 * @flags: ASYNC_TX_ASSUME_COHERENT, ASYNC_TX_ACK
 * @depend_tx: xor depends on the result of this transaction.
 * @callback: function to call when the xor completes
 * @callback_param: parameter to pass to the callback routine
 */
struct dma_async_tx_descriptor *
async_xor_zero_sum(struct page *dest, struct page **src_list,
	unsigned int offset, unsigned int src_cnt, size_t len,
	u32 *result, enum async_tx_flags flags,
	struct dma_async_tx_descriptor *depend_tx,
	dma_async_tx_callback callback, void *callback_param)
{
	struct dma_chan *chan = async_tx_find_channel(depend_tx, DMA_ZERO_SUM);
	struct dma_device *device = chan ? chan->device : NULL;
	int int_en = (flags & ASYNC_TX_INT_EN) ? 1 : 0;
	struct dma_async_tx_descriptor *tx = device ?
		device->device_prep_dma_zero_sum(chan, src_cnt, len, result,
			int_en) : NULL;
	int i;

	if (tx) {
		dma_addr_t dma_addr;
		enum dma_data_direction dir;

		PRINTK("%s: (async) len: %u\n", __FUNCTION__, len);

		dir = (flags & ASYNC_TX_ASSUME_COHERENT) ?
			DMA_NONE : DMA_TO_DEVICE;

		for (i = 0; i < src_cnt; i++) {
			dma_addr = device->map_page(chan, src_list[i],
				offset, len, dir);
		     	device->device_set_src(dma_addr, tx, i);
		}

		async_tx_submit(chan, tx, flags, depend_tx, callback,
			callback_param);
	} else {
		unsigned long xor_flags = flags;

		PRINTK("%s: (sync) len: %u\n", __FUNCTION__, len);

		xor_flags |= ASYNC_TX_XOR_DROP_DST;
		xor_flags &= ~ASYNC_TX_ACK;

		tx = async_xor(dest, src_list, offset, src_cnt, len, xor_flags,
			depend_tx, NULL, NULL);

		if (tx) {
			if (dma_wait_for_async_tx(tx) == DMA_ERROR)
				panic("%s: DMA_ERROR waiting for tx\n",
					__FUNCTION__);
			async_tx_ack(tx);
		}

		*result = page_is_zero(dest, len) ? 0 : 1;

		tx = NULL;

		sync_epilog(flags, depend_tx, callback, callback_param);
	}

	return tx;
}

/**
 * async_memcpy - attempt to copy memory with a dma engine.
 * @dest: destination page
 * @src: src page
 * @offset: offset in pages to start transaction
 * @len: length in bytes
 * @flags: ASYNC_TX_ASSUME_COHERENT, ASYNC_TX_ACK, ASYNC_TX_DEP_ACK,
 *	ASYNC_TX_KMAP_SRC, ASYNC_TX_KMAP_DST
 * @depend_tx: memcpy depends on the result of this transaction
 * @callback: function to call when the memcpy completes
 * @callback_param: parameter to pass to the callback routine
 */
struct dma_async_tx_descriptor *
async_memcpy(struct page *dest, struct page *src, unsigned int dest_offset,
	unsigned int src_offset, size_t len, enum async_tx_flags flags,
	struct dma_async_tx_descriptor *depend_tx,
	dma_async_tx_callback callback, void *callback_param)
{
	struct dma_chan *chan = async_tx_find_channel(depend_tx, DMA_MEMCPY);
	struct dma_device *device = chan ? chan->device : NULL;
	int int_en = (flags & ASYNC_TX_INT_EN) ? 1 : 0;
	struct dma_async_tx_descriptor *tx = device ?
		device->device_prep_dma_memcpy(chan, len,
		int_en) : NULL;

	if (tx) { /* run the memcpy asynchronously */
		dma_addr_t dma_addr;
		enum dma_data_direction dir;

		PRINTK("%s: (async) len: %u\n", __FUNCTION__, len);

		dir = (flags & ASYNC_TX_ASSUME_COHERENT) ?
			DMA_NONE : DMA_FROM_DEVICE;

		dma_addr = device->map_page(chan, dest, dest_offset, len, dir);
	     	device->device_set_dest(dma_addr, tx, 0);

		dir = (flags & ASYNC_TX_ASSUME_COHERENT) ?
			DMA_NONE : DMA_TO_DEVICE;

		dma_addr = device->map_page(chan, src, src_offset, len, dir);
	     	device->device_set_src(dma_addr, tx, 0);

		async_tx_submit(chan, tx, flags, depend_tx, callback,
			callback_param);
	} else { /* run the memcpy synchronously */
		void *dest_buf, *src_buf;
		PRINTK("%s: (sync) len: %u\n", __FUNCTION__, len);

		/* wait for any prerequisite operations */
		if (depend_tx) {
			/* if ack is already set then we cannot be sure
			 * we are referring to the correct operation
			 */
			BUG_ON(depend_tx->ack);
			if (dma_wait_for_async_tx(depend_tx) == DMA_ERROR)
				panic("%s: DMA_ERROR waiting for depend_tx\n",
					__FUNCTION__);
		}

		if (flags & ASYNC_TX_KMAP_DST)
			dest_buf = kmap_atomic(dest, KM_USER0) + dest_offset;
		else
			dest_buf = page_address(dest) + dest_offset;

		if (flags & ASYNC_TX_KMAP_SRC)
			src_buf = kmap_atomic(src, KM_USER0) + src_offset;
		else
			src_buf = page_address(src) + src_offset;

		memcpy(dest_buf, src_buf, len);

		if (flags & ASYNC_TX_KMAP_DST)
			kunmap_atomic(dest_buf, KM_USER0);

		if (flags & ASYNC_TX_KMAP_SRC)
			kunmap_atomic(src_buf, KM_USER0);

		sync_epilog(flags, depend_tx, callback, callback_param);
	}

	return tx;
}

/**
 * async_memset - attempt to fill memory with a dma engine.
 * @dest: destination page
 * @val: fill value
 * @offset: offset in pages to start transaction
 * @len: length in bytes
 * @flags: ASYNC_TX_ASSUME_COHERENT, ASYNC_TX_ACK
 * @depend_tx: memset depends on the result of this transaction
 * @callback: function to call when the memcpy completes
 * @callback_param: parameter to pass to the callback routine
 */
struct dma_async_tx_descriptor *
async_memset(struct page *dest, int val, unsigned int offset,
	size_t len, enum async_tx_flags flags,
	struct dma_async_tx_descriptor *depend_tx,
	dma_async_tx_callback callback, void *callback_param)
{
	struct dma_chan *chan = async_tx_find_channel(depend_tx, DMA_MEMSET);
	struct dma_device *device = chan ? chan->device : NULL;
	int int_en = (flags & ASYNC_TX_INT_EN) ? 1 : 0;
	struct dma_async_tx_descriptor *tx = device ?
		device->device_prep_dma_memset(chan, val, len,
			int_en) : NULL;

	if (tx) { /* run the memset asynchronously */
		dma_addr_t dma_addr;
		enum dma_data_direction dir;

		PRINTK("%s: (async) len: %u\n", __FUNCTION__, len);
		dir = (flags & ASYNC_TX_ASSUME_COHERENT) ?
			DMA_NONE : DMA_FROM_DEVICE;

		dma_addr = device->map_page(chan, dest, offset, len, dir);
	     	device->device_set_dest(dma_addr, tx, 0);

		async_tx_submit(chan, tx, flags, depend_tx, callback,
			callback_param);
	} else { /* run the memset synchronously */
		void *dest_buf;
		PRINTK("%s: (sync) len: %u\n", __FUNCTION__, len);

		dest_buf = (void *) (((char *) page_address(dest)) + offset);

		/* wait for any prerequisite operations */
		if (depend_tx) {
			/* if ack is already set then we cannot be sure
			 * we are referring to the correct operation
			 */
			BUG_ON(depend_tx->ack);
			if (dma_wait_for_async_tx(depend_tx) == DMA_ERROR)
				panic("%s: DMA_ERROR waiting for depend_tx\n",
					__FUNCTION__);
		}

		memset(dest_buf, val, len);

		sync_epilog(flags, depend_tx, callback, callback_param);
	}

	return tx;
}

/**
 * async_interrupt - cause an interrupt to asynchrounously flush pending
 *	completion callbacks, or schedule new callback.  Note: this rouine
 *	assumes that all dma channels have the DMA_INTERRUPT capability
 * @flags: ASYNC_TX_DEP_ACK
 * @depend_tx: interrupt depends the result of this transaction
 * @callback: function to call after the interrupt fires
 * @callback_param: parameter to pass to the callback routine
 */
struct dma_async_tx_descriptor *
async_interrupt(enum async_tx_flags flags,
	struct dma_async_tx_descriptor *depend_tx,
	dma_async_tx_callback callback, void *callback_param)
{
	struct dma_chan *chan = async_tx_find_channel(depend_tx, DMA_INTERRUPT);
	struct dma_device *device = chan ? chan->device : NULL;
	struct dma_async_tx_descriptor *tx = device ?
		device->device_prep_dma_interrupt(chan) : NULL;

	if (tx) {
		PRINTK("%s: (async)\n", __FUNCTION__);

		async_tx_submit(chan, tx, flags, depend_tx, callback,
			callback_param);
	} else {
		PRINTK("%s: (sync)\n", __FUNCTION__);

		/* wait for any prerequisite operations */
		if (depend_tx) {
			/* if ack is already set then we cannot be sure
			 * we are referring to the correct operation
			 */
			BUG_ON(depend_tx->ack);
			if (dma_wait_for_async_tx(depend_tx) == DMA_ERROR)
				panic("%s: DMA_ERROR waiting for depend_tx\n",
					__FUNCTION__);
		}

		sync_epilog(flags, depend_tx, callback, callback_param);
	}

	return tx;
}

/**
 * async_interrupt_cond - same as async_interrupt except that this will only be
 *	if next_op must be run on a different channel.  Note: this rouine
 *	assumes that all dma channels have the DMA_INTERRUPT capability
 * @next_op: the next operation type to be submitted
 * @flags: ASYNC_TX_DEP_ACK
 * @depend_tx: interrupt depends the result of this transaction
 */
struct dma_async_tx_descriptor *
async_interrupt_cond(enum dma_transaction_type next_op,
	enum async_tx_flags flags, struct dma_async_tx_descriptor *depend_tx)
{
	int chan_switch = depend_tx ?
		!test_bit(next_op, &depend_tx->chan->device->capabilities) : 0;
	struct dma_chan *chan = chan_switch ? depend_tx->chan : NULL;
	struct dma_device *device = chan ? chan->device : NULL;
	struct dma_async_tx_descriptor *tx = device ?
		device->device_prep_dma_interrupt(chan) : NULL;

	if (!chan_switch)
		return depend_tx;
	else if (tx) {
		PRINTK("%s: (async)\n", __FUNCTION__);

		async_tx_submit(chan, tx, flags, depend_tx, NULL, NULL);
	} else {
		PRINTK("%s: (sync)\n", __FUNCTION__);

		/* wait for any prerequisite operations */
		if (depend_tx) {
			/* if ack is already set then we cannot be sure
			 * we are referring to the correct operation
			 */
			BUG_ON(depend_tx->ack);
			if (dma_wait_for_async_tx(depend_tx) == DMA_ERROR)
				panic("%s: DMA_ERROR waiting for depend_tx\n",
					__FUNCTION__);
		}

		sync_epilog(flags, depend_tx, NULL, NULL);
	}

	return tx;
}

module_init(async_tx_init);
module_exit(async_tx_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Asynchronous Bulk Memory Transactions API");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL_GPL(async_interrupt);
EXPORT_SYMBOL_GPL(async_interrupt_cond);
EXPORT_SYMBOL_GPL(async_memcpy);
EXPORT_SYMBOL_GPL(async_memset);
EXPORT_SYMBOL_GPL(async_xor);
EXPORT_SYMBOL_GPL(async_xor_zero_sum);
EXPORT_SYMBOL_GPL(async_tx_issue_pending_all);
EXPORT_SYMBOL_GPL(async_tx_ack);
EXPORT_SYMBOL_GPL(dma_wait_for_async_tx);
EXPORT_SYMBOL_GPL(async_tx_run_dependencies);
