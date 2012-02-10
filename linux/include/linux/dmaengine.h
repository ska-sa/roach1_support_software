/*
 * Copyright(c) 2004 - 2006 Intel Corporation. All rights reserved.
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
#ifndef DMAENGINE_H
#define DMAENGINE_H

#include <linux/device.h>
#include <linux/uio.h>
#include <linux/kref.h>
#include <linux/completion.h>
#include <linux/rcupdate.h>
#include <linux/dma-mapping.h>

/**
 * enum dma_event - resource PNP/power managment events
 * @DMA_RESOURCE_SUSPEND: DMA device going into low power state
 * @DMA_RESOURCE_RESUME: DMA device returning to full power
 * @DMA_RESOURCE_ADDED: DMA device added to the system
 * @DMA_RESOURCE_REMOVED: DMA device removed from the system
 */
enum dma_event {
	DMA_RESOURCE_SUSPEND,
	DMA_RESOURCE_RESUME,
	DMA_RESOURCE_ADDED,
	DMA_RESOURCE_REMOVED,
};

/**
 * typedef dma_cookie_t - an opaque DMA cookie
 *
 * if dma_cookie_t is >0 it's a DMA request cookie, <0 it's an error code
 */
typedef s32 dma_cookie_t;

#define dma_submit_error(cookie) ((cookie) < 0 ? 1 : 0)

/**
 * enum dma_status - DMA transaction status
 * @DMA_SUCCESS: transaction completed successfully
 * @DMA_IN_PROGRESS: transaction not yet processed
 * @DMA_ERROR: transaction failed
 */
enum dma_status {
	DMA_SUCCESS,
	DMA_IN_PROGRESS,
	DMA_ERROR,
};

/**
 * enum dma_transaction_type - DMA transaction types/indexes
 */
enum dma_transaction_type {
	DMA_MEMCPY,
	DMA_XOR,
	DMA_PQ_XOR,
	DMA_DUAL_XOR,
	DMA_PQ_UPDATE,
	DMA_ZERO_SUM,
	DMA_PQ_ZERO_SUM,
	DMA_MEMSET,
	DMA_MEMCPY_CRC32C,
	DMA_INTERRUPT,
};

#define DMA_TX_TYPE_START (DMA_MEMCPY)
#define DMA_TX_TYPE_END (DMA_INTERRUPT)
#define dma_async_for_each_tx_type(index) for (\
	 index = (unsigned long) DMA_TX_TYPE_START;\
	 index <= (unsigned long) DMA_TX_TYPE_END;\
	 index++)

#define DMA_CHAN_REQUEST_ALL (-1)

/**
 * enum dma_capability - DMA transfer/transform capabilities
 */
enum dma_capability {
	DMA_CAP_MEMCPY	      = (1 << DMA_MEMCPY),
	DMA_CAP_XOR	      = (1 << DMA_XOR),
	DMA_CAP_PQ_XOR	      = (1 << DMA_PQ_XOR),
	DMA_CAP_DUAL_XOR      = (1 << DMA_DUAL_XOR),
	DMA_CAP_PQ_UPDATE     = (1 << DMA_PQ_UPDATE),
	DMA_CAP_ZERO_SUM      = (1 << DMA_ZERO_SUM),
	DMA_CAP_PQ_ZERO_SUM   = (1 << DMA_PQ_ZERO_SUM),
	DMA_CAP_MEMSET	      = (1 << DMA_MEMSET),
	DMA_CAP_MEMCPY_CRC32C = (1 << DMA_MEMCPY_CRC32C),
	DMA_CAP_INTERRUPT     = (1 << DMA_INTERRUPT),
};

/**
 * struct dma_chan_percpu - the per-CPU part of struct dma_chan
 * @refcount: local_t used for open-coded "bigref" counting
 * @memcpy_count: transaction counter
 * @bytes_transferred: byte counter
 */

struct dma_chan_percpu {
	local_t refcount;
	/* stats */
	unsigned long memcpy_count;
	unsigned long bytes_transferred;
};

/**
 * struct dma_chan - devices supply DMA channels, clients use them
 * @client: ptr to the client user of this chan, will be %NULL when unused
 * @device: ptr to the dma device who supplies this channel, always !%NULL
 * @cookie: last cookie value returned to client
 * @chan_id: channel ID for sysfs
 * @class_dev: class device for sysfs
 * @refcount: kref, used in "bigref" slow-mode
 * @slow_ref: indicates that the DMA channel is free
 * @rcu: the DMA channel's RCU head
 * @client_node: used to add this to the client chan list
 * @device_node: used to add this to the device chan list
 * @local: per-cpu pointer to a struct dma_chan_percpu
 */
struct dma_chan {
	struct dma_client *client;
	struct dma_device *device;
	dma_cookie_t cookie;

	/* sysfs */
	int chan_id;
	struct class_device class_dev;

	struct kref refcount;
	int slow_ref;
	struct rcu_head rcu;

	struct list_head client_node;
	struct list_head device_node;
	struct dma_chan_percpu *local;
};

void dma_chan_cleanup(struct kref *kref);

static inline void dma_chan_get(struct dma_chan *chan)
{
	if (unlikely(chan->slow_ref))
		kref_get(&chan->refcount);
	else {
		local_inc(&(per_cpu_ptr(chan->local, get_cpu())->refcount));
		put_cpu();
	}
}

static inline void dma_chan_put(struct dma_chan *chan)
{
	if (unlikely(chan->slow_ref))
		kref_put(&chan->refcount, dma_chan_cleanup);
	else {
		local_dec(&(per_cpu_ptr(chan->local, get_cpu())->refcount));
		put_cpu();
	}
}

/*
 * typedef dma_event_callback - function pointer to a DMA event callback
 */
typedef void (*dma_event_callback) (struct dma_client *client,
		struct dma_chan *chan, enum dma_event event);

/**
 * struct dma_client - info on the entity making use of DMA services
 * @event_callback: func ptr to call when something happens
 * @chan_count: number of chans allocated
 * @chans_desired: number of chans requested. Can be +/- chan_count
 * @lock: protects access to the channels list
 * @channels: the list of DMA channels allocated
 * @global_node: list_head for global dma_client_list
 */
struct dma_client {
	dma_event_callback	event_callback;
	int			chan_count;
	int			chans_desired;

	spinlock_t		lock;
	struct list_head	channels;
	struct list_head	global_node;
};

typedef void (*dma_async_tx_callback)(void *dma_async_param);
/**
 * struct dma_async_tx_descriptor - async transaction descriptor
 * @ack: the descriptor can not be reused until the client acknowledges
 *	receipt, i.e. has has a chance to establish any dependency chains
 * @cookie: tracking cookie for this transaction, set to -EBUSY if
 *	this tx is sitting on a dependency list
 * @callback: routine to call after this operation is complete
 * @callback_param: general parameter to pass to the callback routine
 * @chan: target channel for this operation
 * @depend_node: allow this transaction to be executed after another
 *	transaction has completed
 * @depend_list: at completion this list of transactions are submitted
 * @parent: pointer to the next level up in the dependency chain
 * @lock: protect the dependency list
 * @type: allows backend implementations to key off the tx_type
 */
struct dma_async_tx_descriptor {
	int ack;
	dma_cookie_t cookie;
	dma_async_tx_callback callback;
	void *callback_param;
	struct dma_chan *chan;
	struct list_head depend_node;
	struct list_head depend_list;
	struct dma_async_tx_descriptor *parent;
	spinlock_t lock;
	enum dma_transaction_type type;
};

/**
 * struct dma_device - info on the entity supplying DMA services
 * @chancnt: how many DMA channels are supported
 * @channels: the list of struct dma_chan
 * @global_node: list_head for global dma_device_list
 * @capabilities: one or more dma_capability flags
 * @max_xor: maximum number of xor sources, 0 if no capability
 * @refcount: reference count
 * @done: IO completion struct
 * @dev_id: unique device ID
 * @device_alloc_chan_resources: allocate resources and return the
 *	number of allocated descriptors
 * @device_free_chan_resources: release DMA channel's resources
 * @device_prep_dma_memcpy: prepares a memcpy operation
 * @device_prep_dma_xor: prepares a xor operation
 * @device_prep_dma_zero_sum: prepares a zero_sum operation
 * @device_prep_dma_memset: prepares a memset operation
 * @device_prep_dma_interrupt: prepares an end of chain interrupt operation
 * @device_tx_submit: execute an operation
 * @device_set_dest: set a destination address in a hardware descriptor
 * @device_set_src: set a source address in a hardware descriptor
 * @device_dependency_added: async_tx notifies the channel about new deps
 * @map_page: get a dma address that this device can use
 * @map_single: get a dma address that this device can use
 * @unmap_page: undo a dma mapping
 * @unmap_single: undo a dma mapping
 * @device_issue_pending: push pending transactions to hardware
 */
struct dma_device {

	unsigned int chancnt;
	struct list_head channels;
	struct list_head global_node;
	unsigned long capabilities;
	unsigned int max_xor;

	struct kref refcount;
	struct completion done;

	int dev_id;

	int (*device_alloc_chan_resources)(struct dma_chan *chan);
	void (*device_free_chan_resources)(struct dma_chan *chan);

	struct dma_async_tx_descriptor *(*device_prep_dma_memcpy)(
		struct dma_chan *chan, size_t len, int int_en);
	struct dma_async_tx_descriptor *(*device_prep_dma_xor)(
		struct dma_chan *chan, unsigned int src_cnt, size_t len,
		int int_en);
	struct dma_async_tx_descriptor *(*device_prep_dma_zero_sum)(
		struct dma_chan *chan, unsigned int src_cnt, size_t len,
		u32 *result, int int_en);
	struct dma_async_tx_descriptor *(*device_prep_dma_memset)(
		struct dma_chan *chan, int value, size_t len, int int_en);
	struct dma_async_tx_descriptor *(*device_prep_dma_interrupt)(
		struct dma_chan *chan);

	dma_cookie_t (*device_tx_submit)(struct dma_async_tx_descriptor *tx);
	void (*device_set_dest)(dma_addr_t addr,
		struct dma_async_tx_descriptor *tx, int index);
	void (*device_set_src)(dma_addr_t addr,
		struct dma_async_tx_descriptor *tx, int index);
	void (*device_dependency_added)(struct dma_chan *chan);
	enum dma_status (*device_is_tx_complete)(struct dma_chan *chan,
			dma_cookie_t cookie, dma_cookie_t *last,
			dma_cookie_t *used);
	dma_addr_t (*map_page)(struct dma_chan *chan, struct page *page,
				unsigned long offset, size_t size,
				int direction);
	dma_addr_t (*map_single)(struct dma_chan *chan, void *cpu_addr,
				size_t size, int direction);
	void (*unmap_page)(struct dma_chan *chan, dma_addr_t handle,
				size_t size, int direction);
	void (*unmap_single)(struct dma_chan *chan, dma_addr_t handle,
				size_t size, int direction);
	void (*device_issue_pending)(struct dma_chan *chan);
};

/* --- public DMA engine API --- */

struct dma_client *dma_async_client_register(dma_event_callback event_callback);
void dma_async_client_unregister(struct dma_client *client);
void dma_async_client_chan_request(struct dma_client *client, int number);

/**
 * dma_async_memcpy_buf_to_buf - offloaded copy between virtual addresses
 * @chan: DMA channel to offload copy to
 * @dest: destination address (virtual)
 * @src: source address (virtual)
 * @len: length
 *
 * Both @dest and @src must be mappable to a bus address according to the
 * DMA mapping API rules for streaming mappings.
 * Both @dest and @src must stay memory resident (kernel memory or locked
 * user space pages).
 */
static inline dma_cookie_t dma_async_memcpy_buf_to_buf(struct dma_chan *chan,
        void *dest, void *src, size_t len)
{
	struct dma_device *dev = chan->device;
	struct dma_async_tx_descriptor *tx;
	dma_addr_t addr;
	dma_cookie_t cookie;
	int cpu;

	tx = dev->device_prep_dma_memcpy(chan, len, 0);
	if (!tx)
		return -ENOMEM;

	tx->ack = 1;
	addr = dev->map_single(chan, src, len, DMA_TO_DEVICE);
	dev->device_set_src(addr, tx, 0);
	addr = dev->map_single(chan, dest, len, DMA_FROM_DEVICE);
	dev->device_set_dest(addr, tx, 0);
	cookie = dev->device_tx_submit(tx);

	cpu = get_cpu();
	per_cpu_ptr(chan->local, cpu)->bytes_transferred += len;
	per_cpu_ptr(chan->local, cpu)->memcpy_count++;
	put_cpu();

	return cookie;
}

/**
 * dma_async_memcpy_buf_to_pg - offloaded copy from address to page
 * @chan: DMA channel to offload copy to
 * @page: destination page
 * @offset: offset in page to copy to
 * @kdata: source address (virtual)
 * @len: length
 *
 * Both @page/@offset and @kdata must be mappable to a bus address according
 * to the DMA mapping API rules for streaming mappings.
 * Both @page/@offset and @kdata must stay memory resident (kernel memory or
 * locked user space pages)
 */
static inline dma_cookie_t dma_async_memcpy_buf_to_pg(struct dma_chan *chan,
        struct page *page, unsigned int offset, void *kdata, size_t len)
{
	struct dma_device *dev = chan->device;
	struct dma_async_tx_descriptor *tx;
	dma_addr_t addr;
	dma_cookie_t cookie;
	int cpu;

	tx = dev->device_prep_dma_memcpy(chan, len, 0);
	if (!tx)
		return -ENOMEM;

	tx->ack = 1;
	addr = dev->map_single(chan, kdata, len, DMA_TO_DEVICE);
	dev->device_set_src(addr, tx, 0);
	addr = dev->map_page(chan, page, offset, len, DMA_FROM_DEVICE);
	dev->device_set_dest(addr, tx, 0);
	cookie = dev->device_tx_submit(tx);

	cpu = get_cpu();
	per_cpu_ptr(chan->local, cpu)->bytes_transferred += len;
	per_cpu_ptr(chan->local, cpu)->memcpy_count++;
	put_cpu();

	return cookie;
}

/**
 * dma_async_memcpy_pg_to_pg - offloaded copy from page to page
 * @chan: DMA channel to offload copy to
 * @dest_pg: destination page
 * @dest_off: offset in page to copy to
 * @src_pg: source page
 * @src_off: offset in page to copy from
 * @len: length
 *
 * Both @dest_page/@dest_off and @src_page/@src_off must be mappable to a bus
 * address according to the DMA mapping API rules for streaming mappings.
 * Both @dest_page/@dest_off and @src_page/@src_off must stay memory resident
 * (kernel memory or locked user space pages).
 */
static inline dma_cookie_t dma_async_memcpy_pg_to_pg(struct dma_chan *chan,
        struct page *dest_pg, unsigned int dest_off, struct page *src_pg,
        unsigned int src_off, size_t len)
{
	struct dma_device *dev = chan->device;
	struct dma_async_tx_descriptor *tx;
	dma_addr_t addr;
	dma_cookie_t cookie;
	int cpu;

	tx = dev->device_prep_dma_memcpy(chan, len, 0);
	if (!tx)
		return -ENOMEM;

	tx->ack = 1;
	addr = dev->map_page(chan, src_pg, src_off, len, DMA_TO_DEVICE);
	dev->device_set_src(addr, tx, 0);
	addr = dev->map_page(chan, dest_pg, dest_off, len, DMA_FROM_DEVICE);
	dev->device_set_dest(addr, tx, 0);
	cookie = dev->device_tx_submit(tx);

	cpu = get_cpu();
	per_cpu_ptr(chan->local, cpu)->bytes_transferred += len;
	per_cpu_ptr(chan->local, cpu)->memcpy_count++;
	put_cpu();

	return cookie;
}

static inline void
dma_async_tx_descriptor_init(struct dma_async_tx_descriptor *tx,
	struct dma_chan *chan)
{
	tx->chan = chan;
	spin_lock_init(&tx->lock);
	INIT_LIST_HEAD(&tx->depend_node);
	INIT_LIST_HEAD(&tx->depend_list);
}

/**
 * dma_async_issue_pending - flush pending transactions to HW
 * @chan: target DMA channel
 *
 * This allows drivers to push copies to HW in batches,
 * reducing MMIO writes where possible.
 */
static inline void dma_async_issue_pending(struct dma_chan *chan)
{
	return chan->device->device_issue_pending(chan);
}

#define dma_async_memcpy_issue_pending(chan) dma_async_issue_pending(chan)

/**
 * dma_async_is_tx_complete - poll for transaction completion
 * @chan: DMA channel
 * @cookie: transaction identifier to check status of
 * @last: returns last completed cookie, can be NULL
 * @used: returns last issued cookie, can be NULL
 *
 * If @last and @used are passed in, upon return they reflect the driver
 * internal state and can be used with dma_async_is_complete() to check
 * the status of multiple cookies without re-checking hardware state.
 */
static inline enum dma_status dma_async_is_tx_complete(struct dma_chan *chan,
	dma_cookie_t cookie, dma_cookie_t *last, dma_cookie_t *used)
{
	return chan->device->device_is_tx_complete(chan, cookie, last, used);
}

#define dma_async_memcpy_complete(chan, cookie, last, used)\
	dma_async_is_tx_complete(chan, cookie, last, used)

/**
 * dma_async_is_complete - test a cookie against chan state
 * @cookie: transaction identifier to test status of
 * @last_complete: last know completed transaction
 * @last_used: last cookie value handed out
 *
 * dma_async_is_complete() is used in dma_async_memcpy_complete()
 * the test logic is seperated for lightweight testing of multiple cookies
 */
static inline enum dma_status dma_async_is_complete(dma_cookie_t cookie,
			dma_cookie_t last_complete, dma_cookie_t last_used)
{
	if (last_complete <= last_used) {
		if ((cookie <= last_complete) || (cookie > last_used))
			return DMA_SUCCESS;
	} else {
		if ((cookie <= last_complete) && (cookie > last_used))
			return DMA_SUCCESS;
	}
	return DMA_IN_PROGRESS;
}

static inline enum dma_status dma_sync_wait(struct dma_chan *chan,
						dma_cookie_t cookie)
{
	enum dma_status status;
	unsigned long dma_sync_wait_timeout = jiffies + msecs_to_jiffies(5000);

	dma_async_issue_pending(chan);
	do {
		status = dma_async_is_tx_complete(chan, cookie, NULL, NULL);
		if (time_after_eq(jiffies, dma_sync_wait_timeout)) {
			printk(KERN_ERR "dma_sync_wait_timeout!\n");
			return DMA_ERROR;
		}
	} while (status == DMA_IN_PROGRESS);

	return status;
}

/* --- DMA device --- */

int dma_async_device_register(struct dma_device *device);
void dma_async_device_unregister(struct dma_device *device);

/* --- Helper iov-locking functions --- */

struct dma_page_list {
	char *base_address;
	int nr_pages;
	struct page **pages;
};

struct dma_pinned_list {
	int nr_iovecs;
	struct dma_page_list page_list[0];
};

struct dma_pinned_list *dma_pin_iovec_pages(struct iovec *iov, size_t len);
void dma_unpin_iovec_pages(struct dma_pinned_list* pinned_list);

dma_cookie_t dma_memcpy_to_iovec(struct dma_chan *chan, struct iovec *iov,
	struct dma_pinned_list *pinned_list, unsigned char *kdata, size_t len);
dma_cookie_t dma_memcpy_pg_to_iovec(struct dma_chan *chan, struct iovec *iov,
	struct dma_pinned_list *pinned_list, struct page *page,
	unsigned int offset, size_t len);

#endif /* DMAENGINE_H */
