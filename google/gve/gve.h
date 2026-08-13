/* SPDX-License-Identifier: (GPL-2.0 OR MIT)
 * Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2015-2024 Google LLC
 */

#ifndef _GVE_H_
#define _GVE_H_

#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/ethtool_netlink.h>
#include <linux/netdevice.h>
#include <linux/net_tstamp.h>
#include <linux/pci.h>
#include <linux/timer.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/u64_stats_sync.h>
#include <linux/utsname.h>
#include <linux/version.h>
#include <net/page_pool/helpers.h>
#include <net/xdp.h>

#include "gve_desc.h"
#include "gve_desc_dqo.h"
#include "gve_mailbox.h"
#include "gve_flow_rule.h"

#ifndef PCI_VENDOR_ID_GOOGLE
#define PCI_VENDOR_ID_GOOGLE	0x1ae0
#endif

#define PCI_DEV_ID_GVNIC	0x0042
#define PCI_DEV_ID_GVNIC_MBX	0x0043

#define GVE_REGISTER_BAR	0
#define GVE_DOORBELL_BAR	2

/* Driver can alloc up to 2 segments for the header and 2 for the payload. */
#define GVE_TX_MAX_IOVEC	4
/* 1 for management, 1 for rx, 1 for tx */
#define GVE_MIN_MSIX 3

/* Numbers of gve tx/rx stats in stats report. */
#define GVE_TX_STATS_REPORT_NUM	6
#define GVE_RX_STATS_REPORT_NUM	2

/* Interval to schedule a stats report update, 20000ms. */
#define GVE_STATS_REPORT_TIMER_PERIOD	20000
#define GVE_RX_NAPI_RESCHED_MS 20 /* msecs */

/* Numbers of NIC tx/rx stats in stats report. */
#define NIC_TX_STATS_REPORT_NUM	0
#define NIC_RX_STATS_REPORT_NUM	4

#define GVE_ADMINQ_BUFFER_SIZE 4096

#define GVE_DATA_SLOT_ADDR_PAGE_MASK (~(PAGE_SIZE - 1))

/* PTYPEs are always 10 bits. */
#define GVE_NUM_PTYPES	1024

/* Default minimum ring size */
#define GVE_DEFAULT_MIN_TX_RING_SIZE 256
#define GVE_DEFAULT_MIN_RX_RING_SIZE 512

#define GVE_DEFAULT_RX_BUFFER_SIZE 2048

#define GVE_XDP_RX_BUFFER_SIZE_DQO 4096

#define GVE_DEFAULT_RX_BUFFER_OFFSET 2048

#define GVE_PAGE_POOL_SIZE_MULTIPLIER 4

#define GVE_FLOW_RULES_CACHE_SIZE \
	(GVE_ADMINQ_BUFFER_SIZE / sizeof(struct gve_flow_rule_config))
#define GVE_FLOW_RULE_IDS_CACHE_SIZE \
	(GVE_ADMINQ_BUFFER_SIZE / sizeof(((struct gve_flow_rule_config *)0)->location))

#define GVE_RSS_KEY_SIZE	40
#define GVE_RSS_INDIR_SIZE	128

#define GVE_XDP_ACTIONS 5

#define GVE_GQ_TX_MIN_PKT_DESC_BYTES 182

#define GVE_DEFAULT_HEADER_BUFFER_SIZE 128

/* Maximum TSO size supported on DQO */
#define GVE_DQO_TX_MAX	0x3FFFF

#define GVE_TX_BUF_SHIFT_DQO 11

/* 2K buffers for DQO-QPL */
#define GVE_TX_BUF_SIZE_DQO BIT(GVE_TX_BUF_SHIFT_DQO)
#define GVE_TX_BUFS_PER_PAGE_DQO (PAGE_SIZE >> GVE_TX_BUF_SHIFT_DQO)
#define GVE_MAX_TX_BUFS_PER_PKT (DIV_ROUND_UP(GVE_DQO_TX_MAX, GVE_TX_BUF_SIZE_DQO))

/* If number of free/recyclable buffers are less than this threshold; driver
 * allocs and uses a non-qpl page on the receive path of DQO QPL to free
 * up buffers.
 * Value is set big enough to post at least 3 64K LRO packet via 2K buffer to NIC.
 */
#define GVE_DQO_QPL_ONDEMAND_ALLOC_THRESHOLD 96

#define GVE_DQO_RX_HWTSTAMP_VALID 0x1

/* Each slot in the desc ring has a 1:1 mapping to a slot in the data ring */
struct gve_rx_desc_queue {
	struct gve_rx_desc *desc_ring; /* the descriptor ring */
	dma_addr_t bus; /* the bus for the desc_ring */
	u8 seqno; /* the next expected seqno for this desc*/
};

/* The page info for a single slot in the RX data queue */
struct gve_rx_slot_page_info {
	/* netmem is used for DQO RDA mode
	 * page is used in all other modes
	 */
	union {
		struct page *page;
		netmem_ref netmem;
	};
	void *page_address;
	u32 page_offset; /* offset to write to in page */
	unsigned int buf_size;
	int pagecnt_bias; /* expected pagecnt if only the driver has a ref */
	u16 pad; /* adjustment for rx padding */
	u8 can_flip; /* tracks if the networking stack is using the page */
};

/* A list of pages registered with the device during setup and used by a queue
 * as buffers
 */
struct gve_queue_page_list {
	u32 id; /* unique id */
	u32 num_entries;
	struct page **pages; /* list of num_entries pages */
	dma_addr_t *page_buses; /* the dma addrs of the pages */
};

/* Each slot in the data ring has a 1:1 mapping to a slot in the desc ring */
struct gve_rx_data_queue {
	union gve_rx_data_slot *data_ring; /* read by NIC */
	dma_addr_t data_bus; /* dma mapping of the slots */
	struct gve_rx_slot_page_info *page_info; /* page info of the buffers */
	struct gve_queue_page_list *qpl; /* qpl assigned to this queue */
	u8 raw_addressing; /* use raw_addressing? */
};

struct gve_priv;

/* These are control path types for PTYPE which are the same as the data path
 * types.
 */
struct gve_ptype_entry {
	u8 l3_type;
	u8 l4_type;
};

struct gve_ptype_map {
	struct gve_ptype_entry ptypes[GVE_NUM_PTYPES]; /* PTYPES are always 10 bits. */
};

/* RX buffer queue for posting buffers to HW.
 * Each RX (completion) queue has a corresponding buffer queue.
 */
struct gve_rx_buf_queue_dqo {
	struct gve_rx_desc_dqo *desc_ring;
	dma_addr_t bus;
	u32 head; /* Pointer to start cleaning buffers at. */
	u32 tail; /* Last posted buffer index + 1 */
	u32 mask; /* Mask for indices to the size of the ring */
};

/* RX completion queue to receive packets from HW. */
struct gve_rx_compl_queue_dqo {
	struct gve_rx_compl_desc_dqo *desc_ring;
	dma_addr_t bus;

	/* Number of slots which did not have a buffer posted yet. We should not
	 * post more buffers than the queue size to avoid HW overrunning the
	 * queue.
	 */
	int num_free_slots;

	/* HW uses a "generation bit" to notify SW of new descriptors. When a
	 * descriptor's generation bit is different from the current generation,
	 * that descriptor is ready to be consumed by SW.
	 */
	u8 cur_gen_bit;

	/* Pointer into desc_ring where the next completion descriptor will be
	 * received.
	 */
	u32 head;
	u32 mask; /* Mask for indices to the size of the ring */
};

struct gve_header_buf {
	u8 *data;
	dma_addr_t addr;
};

/* Stores state for tracking buffers posted to HW */
struct gve_rx_buf_state_dqo {
	/* The page posted to HW. */
	struct gve_rx_slot_page_info page_info;

	/* XSK buffer */
	struct xdp_buff *xsk_buff;

	/* The DMA address corresponding to `page_info`. */
	dma_addr_t addr;

	/* Last offset into the page when it only had a single reference, at
	 * which point every other offset is free to be reused.
	 */
	u32 last_single_ref_offset;

	/* Linked list index to next element in the list, or -1 if none */
	s16 next;
};

/* Wrapper for XDP Rx metadata */
struct gve_xdp_buff {
	struct xdp_buff xdp;
	struct gve_priv *gve;
	const struct gve_rx_compl_desc_dqo *compl_desc;
};

/* `head` and `tail` are indices into an array, or -1 if empty. */
struct gve_index_list {
	s16 head;
	s16 tail;
};

/* A single received packet split across multiple buffers may be
 * reconstructed using the information in this structure.
 */
struct gve_rx_ctx {
	/* head and tail of skb chain for the current packet or NULL if none */
	struct sk_buff *skb_head;
	struct sk_buff *skb_tail;
	u32 total_size;
	u8 frag_cnt;
	bool drop_pkt;
};

struct gve_rx_cnts {
	u32 ok_pkt_bytes;
	u16 ok_pkt_cnt;
	u16 total_pkt_cnt;
	u16 cont_pkt_cnt;
	u16 desc_err_pkt_cnt;
};

/* Contains datapath state used to represent an RX queue. */
struct gve_rx_ring {
	struct gve_priv *gve;

	u16 packet_buffer_size;		/* Size of buffer posted to NIC */
	u16 packet_buffer_truesize;	/* Total size of RX buffer */
	u16 rx_headroom;

	union {
		/* GQI fields */
		struct {
			struct gve_rx_desc_queue desc;
			struct gve_rx_data_queue data;

			/* threshold for posting new buffs and descs */
			u32 db_threshold;

			u32 qpl_copy_pool_mask;
			u32 qpl_copy_pool_head;
			struct gve_rx_slot_page_info *qpl_copy_pool;
		};

		/* DQO fields. */
		struct {
			struct gve_rx_buf_queue_dqo bufq;
			struct gve_rx_compl_queue_dqo complq;

			struct gve_rx_buf_state_dqo *buf_states;
			u16 num_buf_states;

			/* Linked list of gve_rx_buf_state_dqo. Index into
			 * buf_states, or -1 if empty.
			 */
			s16 free_buf_states;

			/* Linked list of gve_rx_buf_state_dqo. Indexes into
			 * buf_states, or -1 if empty.
			 *
			 * This list contains buf_states which are pointing to
			 * valid buffers.
			 *
			 * We use a FIFO here in order to increase the
			 * probability that buffers can be reused by increasing
			 * the time between usages.
			 */
			struct gve_index_list recycled_buf_states;

			/* Linked list of gve_rx_buf_state_dqo. Indexes into
			 * buf_states, or -1 if empty.
			 *
			 * This list contains buf_states which have buffers
			 * which cannot be reused yet.
			 */
			struct gve_index_list used_buf_states;

			/* qpl assigned to this queue */
			struct gve_queue_page_list *qpl;

			/* index into queue page list */
			u32 next_qpl_page_idx;

			/* track number of used buffers */
			u16 used_buf_states_cnt;

			/* Address info of the buffers for header-split */
			struct gve_header_buf hdr_bufs;

			struct page_pool *page_pool;
		} dqo;
	};

	u64 rbytes; /* free-running bytes received */
	u64 rx_hsplit_bytes; /* free-running header bytes received */
	u64 rpackets; /* free-running packets received */
	u32 cnt; /* free-running total number of completed packets */
	u32 fill_cnt; /* free-running total number of descs and buffs posted */
	u32 mask; /* masks the cnt and fill_cnt to the size of the ring */
	u64 rx_hsplit_pkt; /* free-running packets with headers split */
	u64 rx_copybreak_pkt; /* free-running count of copybreak packets */
	u64 rx_copied_pkt; /* free-running total number of copied packets */
	u64 rx_skb_alloc_fail; /* free-running count of skb alloc fails */
	u64 rx_buf_alloc_fail; /* free-running count of buffer alloc fails */
	u64 rx_desc_err_dropped_pkt; /* free-running count of packets dropped by descriptor error */
	/* free-running count of unsplit packets due to header buffer overflow or hdr_len is 0 */
	u64 rx_hsplit_unsplit_pkt;
	u64 rx_cont_packet_cnt; /* free-running multi-fragment packets received */
	u64 rx_frag_flip_cnt; /* free-running count of rx segments where page_flip was used */
	u64 rx_frag_copy_cnt; /* free-running count of rx segments copied */
	u64 rx_frag_alloc_cnt; /* free-running count of rx page allocations */
	u64 xdp_tx_errors;
	u64 xdp_redirect_errors;
	u64 xdp_alloc_fails;
	u64 xdp_actions[GVE_XDP_ACTIONS];
	u32 q_num; /* queue index */
	u32 ntfy_id; /* notification block index */
	struct gve_queue_resources *q_resources; /* head and tail pointer idx */
	dma_addr_t q_resources_bus; /* dma address for the queue resources */
	struct u64_stats_sync statss; /* sync stats for 32bit archs */

	struct gve_rx_ctx ctx; /* Info for packet currently being processed in this ring. */

	/* XDP stuff */
	struct xdp_rxq_info xdp_rxq;
	struct xsk_buff_pool *xsk_pool;
	struct page_frag_cache page_cache; /* Page cache to allocate XDP frames */
	u64 rx_critical_low_bufs; /* count of critical low buffer events */
	struct timer_list starvation_timer; /* for queue starvation recovery */
};

/* A TX desc ring entry */
union gve_tx_desc {
	struct gve_tx_pkt_desc pkt; /* first desc for a packet */
	struct gve_tx_mtd_desc mtd; /* optional metadata descriptor */
	struct gve_tx_seg_desc seg; /* subsequent descs for a packet */
};

/* Tracks the memory in the fifo occupied by a segment of a packet */
struct gve_tx_iovec {
	u32 iov_offset; /* offset into this segment */
	u32 iov_len; /* length */
	u32 iov_padding; /* padding associated with this segment */
};

/* Tracks the memory in the fifo occupied by the skb. Mapped 1:1 to a desc
 * ring entry but only used for a pkt_desc not a seg_desc
 */
struct gve_tx_buffer_state {
	union {
		struct sk_buff *skb; /* skb for this pkt */
		struct xdp_frame *xdp_frame; /* xdp_frame */
	};
	struct {
		u16 size; /* size of xmitted xdp pkt */
		u8 is_xsk; /* xsk buff */
	} xdp;
	union {
		struct gve_tx_iovec iov[GVE_TX_MAX_IOVEC]; /* segments of this pkt */
		struct {
			DEFINE_DMA_UNMAP_ADDR(dma);
			DEFINE_DMA_UNMAP_LEN(len);
		};
	};
};

/* A TX buffer - each queue has one */
struct gve_tx_fifo {
	void *base; /* address of base of FIFO */
	u32 size; /* total size */
	atomic_t available; /* how much space is still available */
	u32 head; /* offset to write at */
	struct gve_queue_page_list *qpl; /* QPL mapped into this FIFO */
};

/* TX descriptor for DQO format */
union gve_tx_desc_dqo {
	struct gve_tx_pkt_desc_dqo pkt;
	struct gve_tx_tso_context_desc_dqo tso_ctx;
	struct gve_tx_general_context_desc_dqo general_ctx;
};

enum gve_packet_state {
	/* Packet is in free list, available to be allocated.
	 * This should always be zero since state is not explicitly initialized.
	 */
	GVE_PACKET_STATE_UNALLOCATED,
	/* Packet is expecting a regular data completion or miss completion */
	GVE_PACKET_STATE_PENDING_DATA_COMPL,
	/* Packet has received a miss completion and is expecting a
	 * re-injection completion.
	 */
	GVE_PACKET_STATE_PENDING_REINJECT_COMPL,
	/* No valid completion received within the specified timeout. */
	GVE_PACKET_STATE_TIMED_OUT_COMPL,
	/* XSK pending packet has received a packet/reinjection completion, or
	 * has timed out. At this point, the pending packet can be counted by
	 * xsk_tx_complete and freed.
	 */
	GVE_PACKET_STATE_XSK_COMPLETE,
};

enum gve_tx_pending_packet_dqo_type {
	GVE_TX_PENDING_PACKET_DQO_SKB,
	GVE_TX_PENDING_PACKET_DQO_XDP_FRAME,
	GVE_TX_PENDING_PACKET_DQO_XSK,
};

struct gve_tx_pending_packet_dqo {
	union {
		struct sk_buff *skb;
		struct xdp_frame *xdpf;
	};

	/* 0th element corresponds to the linear portion of `skb`, should be
	 * unmapped with `dma_unmap_single`.
	 *
	 * All others correspond to `skb`'s frags and should be unmapped with
	 * `dma_unmap_page`.
	 */
	union {
		struct {
			DEFINE_DMA_UNMAP_ADDR(dma[MAX_SKB_FRAGS + 1]);
			DEFINE_DMA_UNMAP_LEN(len[MAX_SKB_FRAGS + 1]);
		};
		s16 tx_qpl_buf_ids[GVE_MAX_TX_BUFS_PER_PKT];
	};

	u16 num_bufs;

	/* Linked list index to next element in the list, or -1 if none */
	s16 next;

	/* Linked list index to prev element in the list, or -1 if none.
	 * Used for tracking either outstanding miss completions or prematurely
	 * freed packets.
	 */
	s16 prev;

	/* Identifies the current state of the packet as defined in
	 * `enum gve_packet_state`.
	 */
	u8 state : 3;

	/* gve_tx_pending_packet_dqo_type */
	u8 type : 2;

	/* If packet is an outstanding miss completion, then the packet is
	 * freed if the corresponding re-injection completion is not received
	 * before kernel jiffies exceeds timeout_jiffies.
	 */
	unsigned long timeout_jiffies;
};

/* Contains datapath state used to represent a TX queue. */
struct gve_tx_ring {
	/* Cacheline 0 -- Accessed & dirtied during transmit */
	union {
		/* GQI fields */
		struct {
			struct gve_tx_fifo tx_fifo;
			u32 req; /* driver tracked head pointer */
			u32 done; /* driver tracked tail pointer */
		};

		/* DQO fields. */
		struct {
			/* Spinlock for XDP tx traffic */
			spinlock_t xdp_lock;

			/* Linked list of gve_tx_pending_packet_dqo. Index into
			 * pending_packets, or -1 if empty.
			 *
			 * This is a consumer list owned by the TX path. When it
			 * runs out, the producer list is stolen from the
			 * completion handling path
			 * (dqo_compl.free_pending_packets).
			 */
			s16 free_pending_packets;

			/* Cached value of `dqo_compl.hw_tx_head` */
			u32 head;
			u32 tail; /* Last posted buffer index + 1 */

			/* Index of the last descriptor with "report event" bit
			 * set.
			 */
			u32 last_re_idx;

			/* free running number of packet buf descriptors posted */
			u16 posted_packet_desc_cnt;
			/* free running number of packet buf descriptors completed */
			u16 completed_packet_desc_cnt;

			/* QPL fields */
			struct {
			       /* Linked list of gve_tx_buf_dqo. Index into
				* tx_qpl_buf_next, or -1 if empty.
				*
				* This is a consumer list owned by the TX path. When it
				* runs out, the producer list is stolen from the
				* completion handling path
				* (dqo_compl.free_tx_qpl_buf_head).
				*/
				s16 free_tx_qpl_buf_head;

			       /* Free running count of the number of QPL tx buffers
				* allocated
				*/
				u32 alloc_tx_qpl_buf_cnt;

				/* Cached value of `dqo_compl.free_tx_qpl_buf_cnt` */
				u32 free_tx_qpl_buf_cnt;
			};

			atomic_t xsk_reorder_queue_tail;
		} dqo_tx;
	};

	/* Cacheline 1 -- Accessed & dirtied during gve_clean_tx_done */
	union {
		/* GQI fields */
		struct {
			/* Spinlock for when cleanup in progress */
			spinlock_t clean_lock;
			/* Spinlock for XDP tx traffic */
			spinlock_t xdp_lock;
		};

		/* DQO fields. */
		struct {
			u32 head; /* Last read on compl_desc */

			/* Tracks the current gen bit of compl_q */
			u8 cur_gen_bit;

			/* Linked list of gve_tx_pending_packet_dqo. Index into
			 * pending_packets, or -1 if empty.
			 *
			 * This is the producer list, owned by the completion
			 * handling path. When the consumer list
			 * (dqo_tx.free_pending_packets) is runs out, this list
			 * will be stolen.
			 */
			atomic_t free_pending_packets;

			/* Last TX ring index fetched by HW */
			atomic_t hw_tx_head;

			u16 xsk_reorder_queue_head;
			u16 xsk_reorder_queue_tail;

			/* List to track pending packets which received a miss
			 * completion but not a corresponding reinjection.
			 */
			struct gve_index_list miss_completions;

			/* List to track pending packets that were completed
			 * before receiving a valid completion because they
			 * reached a specified timeout.
			 */
			struct gve_index_list timed_out_completions;

			/* QPL fields */
			struct {
				/* Linked list of gve_tx_buf_dqo. Index into
				 * tx_qpl_buf_next, or -1 if empty.
				 *
				 * This is the producer list, owned by the completion
				 * handling path. When the consumer list
				 * (dqo_tx.free_tx_qpl_buf_head) is runs out, this list
				 * will be stolen.
				 */
				atomic_t free_tx_qpl_buf_head;

				/* Free running count of the number of tx buffers
				 * freed
				 */
				atomic_t free_tx_qpl_buf_cnt;
			};
		} dqo_compl;
	} ____cacheline_aligned;
	u64 pkt_done; /* free-running - total packets completed */
	u64 bytes_done; /* free-running - total bytes completed */
	u64 dropped_pkt; /* free-running - total packets dropped */
	u64 dma_mapping_error; /* count of dma mapping errors */

	/* Cacheline 2 -- Read-mostly fields */
	union {
		/* GQI fields */
		struct {
			union gve_tx_desc *desc;

			/* Maps 1:1 to a desc */
			struct gve_tx_buffer_state *info;
		};

		/* DQO fields. */
		struct {
			union gve_tx_desc_dqo *tx_ring;
			struct gve_tx_compl_desc *compl_ring;

			struct gve_tx_pending_packet_dqo *pending_packets;
			s16 num_pending_packets;

			u16 *xsk_reorder_queue;

			u32 complq_mask; /* complq size is complq_mask + 1 */

			/* QPL fields */
			struct {
				/* qpl assigned to this queue */
				struct gve_queue_page_list *qpl;

				/* Each QPL page is divided into TX bounce buffers
				 * of size GVE_TX_BUF_SIZE_DQO. tx_qpl_buf_next is
				 * an array to manage linked lists of TX buffers.
				 * An entry j at index i implies that j'th buffer
				 * is next on the list after i
				 */
				s16 *tx_qpl_buf_next;
				u32 num_tx_qpl_bufs;
			};
		} dqo;
	} ____cacheline_aligned;
	struct netdev_queue *netdev_txq;
	struct gve_queue_resources *q_resources; /* head and tail pointer idx */
	struct device *dev;
	u32 mask; /* masks req and done down to queue size */
	u8 raw_addressing; /* use raw_addressing? */

	/* Slow-path fields */
	u32 q_num ____cacheline_aligned; /* queue idx */
	u32 stop_queue; /* count of queue stops */
	u32 wake_queue; /* count of queue wakes */
	u32 queue_timeout; /* count of queue timeouts */
	u32 ntfy_id; /* notification block index */
	u32 last_kick_msec; /* Last time the queue was kicked */
	dma_addr_t bus; /* dma address of the descr ring */
	dma_addr_t q_resources_bus; /* dma address of the queue resources */
	dma_addr_t complq_bus_dqo; /* dma address of the dqo.compl_ring */
	struct u64_stats_sync statss; /* sync stats for 32bit archs */
	struct xsk_buff_pool *xsk_pool;
	u64 xdp_xsk_sent;
	u64 xdp_xmit;
	u64 xdp_xmit_errors;
} ____cacheline_aligned;

/* Wraps the info for one irq including the napi struct and the queues
 * associated with that irq.
 */
struct gve_notify_block {
	union {
		__be32 *aq_irq_db_index; /* pointer to idx into Bar2 */
		struct gve_mbx_interrupt_db_info mbx_db_info;
	}; /* doorbell info */

	char name[IFNAMSIZ + 16]; /* name registered with the kernel */
	struct napi_struct napi; /* kernel napi struct for this block */
	struct gve_priv *priv;
	struct gve_tx_ring *tx; /* tx rings on this block */
	struct gve_rx_ring *rx; /* rx rings on this block */
	u32 irq;
};

/* Tracks allowed and current rx queue settings */
struct gve_rx_queue_config {
	u16 max_queues;
	u16 num_queues;
	u16 packet_buffer_size;
};

/* Tracks allowed and current tx queue settings */
struct gve_tx_queue_config {
	u16 max_queues;
	u16 num_queues; /* number of TX queues, excluding XDP queues */
	u16 num_xdp_queues;
};

/* Tracks the available and used qpl IDs */
struct gve_qpl_config {
	u32 qpl_map_size; /* map memory size */
	unsigned long *qpl_id_map; /* bitmap of used qpl ids */
};

struct gve_irq_db {
	__be32 index;
} ____cacheline_aligned;

struct gve_ptype {
	u8 l3_type;  /* `gve_l3_type` in gve_adminq.h */
	u8 l4_type;  /* `gve_l4_type` in gve_adminq.h */
};

struct gve_ptype_lut {
	struct gve_ptype ptypes[GVE_NUM_PTYPES];
};

/* Parameters for allocating resources for tx queues */
struct gve_tx_alloc_rings_cfg {
	struct gve_tx_queue_config *qcfg;
	u16 pages_per_qpl;

	u16 num_xdp_rings;

	u16 ring_size;
	bool raw_addressing;

	/* Allocated resources are returned here */
	struct gve_tx_ring *tx;
};

/* Parameters for allocating resources for rx queues */
struct gve_rx_alloc_rings_cfg {
	/* tx config is also needed to determine QPL ids */
	struct gve_rx_queue_config *qcfg_rx;
	struct gve_tx_queue_config *qcfg_tx;
	u16 pages_per_qpl;

	u16 ring_size;
	u16 packet_buffer_size;
	bool raw_addressing;
	bool enable_header_split;
	bool reset_rss;
	bool xdp;

	/* Allocated resources are returned here */
	struct gve_rx_ring *rx;
};

/* GVE_QUEUE_FORMAT_UNSPECIFIED must be zero since 0 is the default value
 * when the entire configure_device_resources command is zeroed out and the
 * queue_format is not specified.
 */
enum gve_queue_format {
	GVE_QUEUE_FORMAT_UNSPECIFIED	= 0x0,
	GVE_GQI_RDA_FORMAT		= 0x1,
	GVE_GQI_QPL_FORMAT		= 0x2,
	GVE_DQO_RDA_FORMAT		= 0x3,
	GVE_DQO_QPL_FORMAT		= 0x4,
};

struct gve_flow_rules_cache {
	bool rules_cache_synced; /* False if the driver's rules_cache is outdated */
	struct gve_flow_rule_config *rules_cache;
	u32 *rule_ids_cache;
	/* The total number of queried rules that stored in the caches */
	u32 rules_cache_num;
	u32 rule_ids_cache_num;
};

struct gve_rss_config {
	u8 *hash_key;
	u32 *hash_lut;
};

enum gve_rss_hash_type {
	GVE_RSS_HASH_IPV4,
	GVE_RSS_HASH_TCPV4,
	GVE_RSS_HASH_IPV6,
	GVE_RSS_HASH_IPV6_EX,
	GVE_RSS_HASH_TCPV6,
	GVE_RSS_HASH_TCPV6_EX,
	GVE_RSS_HASH_UDPV4,
	GVE_RSS_HASH_UDPV6,
	GVE_RSS_HASH_UDPV6_EX,
};

struct gve_ptp {
	struct ptp_clock_info info;
	struct ptp_clock *clock;
	struct gve_priv *priv;
	bool started;
};

enum gve_dev_clk_read_type {
	  GVE_DEV_CLK_ADMINQ = 1,
	  GVE_DEV_CLK_MMIO = 2,
};

struct gve_device_info {
	enum gve_queue_format queue_format;
	enum gve_dev_clk_read_type clk_read_type;
	u16 default_tx_queues;
	u16 default_rx_queues;
	u16 max_tx_queues;
	u16 max_rx_queues;
	u16 default_tx_ring_size;
	u16 default_rx_ring_size;
	u16 max_tx_ring_size;
	u16 max_rx_ring_size;
	u16 min_tx_ring_size;
	u16 min_rx_ring_size;
	u16 num_msix_vectors;
	u16 max_mtu;
	u8 mac[ETH_ALEN];
	u16 max_rx_buffer_size;
	u16 header_buf_size;
	u32 max_flow_rules;
	u16 rss_key_size;
	u16 rss_lut_size;
	u64 max_registered_pages;
	u16 tx_pages_per_qpl;
	u16 num_event_counters;
	bool default_min_ring_size;
	bool nic_timestamp_supported;
	bool modify_ring_size_enabled;
	bool cache_rss_config;
	bool gro_hw_supported;
	bool gro_hw_default_enable;
};

/* It is the caller's responsibility to ensure the op is
 * supported before calling it.
 */
struct gve_ctrl_ops {
	int (*init_ctrl_plane)(struct gve_adapter *adapter);
	void (*free_ctrl_plane)(struct gve_adapter *adapter);
	int (*get_device_properties)(struct gve_adapter *adapter);
	int (*map_db_bar)(struct gve_adapter *adapter);
	void (*unmap_db_bar)(struct gve_adapter *adapter);
	void (*set_num_queues)(struct gve_adapter *adapter);
	int (*set_num_ntfy_blks)(struct gve_adapter *adapter);
	void (*get_max_queues)(struct gve_adapter *adapter,
			       int *max_tx_qs, int *max_rx_qs);
	/* Retrieve link status from device and set in
	 * adapter->priv->link_up. */
	int (*report_link_status)(struct gve_adapter *adapter);
	/* Retrieve link speed from device and set in
	 * adapter->priv->link_speed. */
	int (*report_link_speed)(struct gve_adapter *adapter);

	int (*request_db_info)(struct gve_adapter *adapter);
	void (*free_db_resources)(struct gve_adapter *adapter);
	int (*setup_mgmt_irq)(struct gve_adapter *adapter);
	void (*teardown_mgmt_irq)(struct gve_adapter *adapter);
	int (*get_ptype_map)(struct gve_adapter *adapter);
	int (*query_rss)(struct gve_adapter *adapter,
			 struct ethtool_rxfh_param *rxfh);
	int (*configure_rss)(struct gve_adapter *adapter,
			     struct ethtool_rxfh_param *param);
	int (*setup_stats_report)(struct gve_adapter *adapter,
				  u64 stats_rerport_len,
				  dma_addr_t stats_report_addr,
				  u64 interval_ms); /* AQ-specific */
	int (*create_queues)(struct gve_adapter *adapter);
	int (*destroy_queues)(struct gve_adapter *adapter);
	void (*write_q_doorbell)(struct gve_adapter *adapter,
				 const struct gve_queue_resources *q_resources,
				 u32 val);
	void (*write_irq_doorbell_dqo)(struct gve_adapter *adapter,
				       const struct gve_notify_block *block,
				       u32 val);
	int (*query_flow_rules)(struct gve_adapter *adapter,
				u16 query_opcode, u32 starting_loc);
	int (*add_flow_rule)(struct gve_adapter *adapter,
			     struct gve_flow_rule *rule, u32 loc);
	int (*del_flow_rule)(struct gve_adapter *adapter, u32 loc);
	int (*reset_flow_rules)(struct gve_adapter *adapter);
};

struct gve_adapter {
	/* mailbox mode */
	bool mailbox_mode;
	struct gve_mbx_queue *mbx_rx;
	struct gve_mbx_queue *mbx_tx;
	struct gve_dma_mem **mbx_rx_bufs;
	struct gve_dma_mem **mbx_tx_bufs;
	struct gve_mbx_msg_queue *mbx_msg_queue;
	struct gve_mbx_msg **mbx_msgs;
	struct workqueue_struct *gve_mbx_wq;
	struct delayed_work gve_mbx_task;
	struct work_struct gve_mbx_lsc_event_task;
	u32 mbx_irq_db_offset;

	int next_msix_vec;

	unsigned long service_task_flags;
	unsigned long state_flags;

	/* Admin queue - see gve_adminq.h*/
	union gve_adminq_command *adminq;
	dma_addr_t adminq_bus_addr;
	struct dma_pool *adminq_pool;
	struct mutex adminq_lock; /* Protects adminq command execution */
	u32 adminq_mask; /* masks prod_cnt to adminq size */
	u32 adminq_prod_cnt; /* free-running count of AQ cmds executed */
	u32 adminq_cmd_fail; /* free-running count of AQ cmds failed */
	u32 adminq_timeouts; /* free-running count of AQ cmds timeouts */
	/* free-running count of per AQ cmd executed */
	u32 adminq_describe_device_cnt;
	u32 adminq_cfg_device_resources_cnt;
	u32 adminq_register_page_list_cnt;
	u32 adminq_unregister_page_list_cnt;
	u32 adminq_create_tx_queue_cnt;
	u32 adminq_create_rx_queue_cnt;
	u32 adminq_destroy_tx_queue_cnt;
	u32 adminq_destroy_rx_queue_cnt;
	u32 adminq_dcfg_device_resources_cnt;
	u32 adminq_set_driver_parameter_cnt;
	u32 adminq_report_stats_cnt;
	u32 adminq_report_link_speed_cnt;
	u32 adminq_report_nic_timestamp_cnt;
	u32 adminq_get_ptype_map_cnt;
	u32 adminq_verify_driver_compatibility_cnt;
	u32 adminq_query_flow_rules_cnt;
	u32 adminq_cfg_flow_rule_cnt;
	u32 adminq_cfg_rss_cnt;
	u32 adminq_query_rss_cnt;

	struct gve_device_info *device_info;
	struct gve_mbx_caps_resp *caps;

	struct pci_dev *pdev;
	void __iomem *reg_bar0;
	struct gve_priv *priv;
	const struct gve_ctrl_ops *ctrl_ops;

	__le64 __iomem *dev_clk_ns_l;
	__le64 __iomem *dev_clk_ns_h;
	__le64 __iomem *dev_art_ns_l;
	__le64 __iomem *dev_art_ns_h;
	__le64 __iomem *dev_clk_cmd_sync;
};

struct gve_priv {
	struct gve_adapter *adapter;
	struct net_device *dev;
	struct gve_tx_ring *tx; /* array of tx_cfg.num_queues */
	struct gve_rx_ring *rx; /* array of rx_cfg.num_queues */
	struct gve_notify_block *ntfy_blocks; /* array of num_ntfy_blks */
	struct gve_irq_db *irq_db_indices; /* array of num_ntfy_blks */
	dma_addr_t irq_db_indices_bus;
	struct msix_entry *msix_vectors; /* array of num_ntfy_blks + 1 */
	char mgmt_msix_name[IFNAMSIZ + 16];
	u32 mgmt_msix_idx;
	__be32 *counter_array; /* array of num_event_counters */
	dma_addr_t counter_array_bus;

	u16 num_event_counters;
	u16 tx_desc_cnt; /* num desc per ring */
	u16 rx_desc_cnt; /* num desc per ring */
	u16 max_tx_desc_cnt;
	u16 max_rx_desc_cnt;
	u16 min_tx_desc_cnt;
	u16 min_rx_desc_cnt;
	bool modify_ring_size_enabled;
	bool default_min_ring_size;
	u16 tx_pages_per_qpl;
	u16 rx_pages_per_qpl;
	u64 max_registered_pages;
	u64 num_registered_pages; /* num pages registered with NIC */
	struct bpf_prog *xdp_prog; /* XDP BPF program */
	u32 rx_copybreak; /* copy packets smaller than this */
	u16 default_num_queues; /* default num queues to set up */

	struct gve_tx_queue_config tx_cfg;
	struct gve_rx_queue_config rx_cfg;
	unsigned long *xsk_pools; /* bitmap of RX queues with XSK pools */
	u32 num_ntfy_blks; /* split between TX and RX so must be even */
	int numa_node;

	struct gve_adminq_registers __iomem *reg_bar0; /* see gve_register.h */
	__be32 __iomem *db_adminq_bar2; /* "array" of doorbells */
	u32 msg_enable;	/* level for netif* netdev print macros	*/
	struct pci_dev *pdev;

	/* metrics */
	u32 tx_timeo_cnt;


	/* Global stats */
	u32 interface_up_cnt; /* count of times interface turned up since last reset */
	u32 interface_down_cnt; /* count of times interface turned down since last reset */
	u32 reset_cnt; /* count of reset */
	u32 page_alloc_fail; /* count of page alloc fails */
	u32 dma_mapping_error; /* count of dma mapping errors */
	u32 stats_report_trigger_cnt; /* count of device-requested stats-reports since last reset */
	u32 suspend_cnt; /* count of times suspended */
	u32 resume_cnt; /* count of times resumed */
	struct workqueue_struct *gve_wq;
	struct work_struct service_task;
	struct work_struct stats_report_task;

	struct gve_stats_report *stats_report;
	u64 stats_report_len;
	dma_addr_t stats_report_bus; /* dma address for the stats report */
	unsigned long ethtool_flags;

	unsigned long stats_report_timer_period;
	struct timer_list stats_report_timer;

	/* Gvnic device link speed from hypervisor. */
	u64 link_speed;
	bool link_up;
	bool up_before_suspend; /* True if dev was up before suspend */

	struct gve_ptype_lut *ptype_lut_dqo;

	/* Must be a power of two. */
	u16 max_rx_buffer_size; /* device limit */

	enum gve_queue_format queue_format;

	/* Interrupt coalescing settings */
	u32 tx_coalesce_usecs;
	u32 rx_coalesce_usecs;

	u16 header_buf_size; /* device configured, header-split supported if non-zero */
	bool header_split_enabled; /* True if the header split is enabled by the user */

	u32 max_flow_rules;
	u32 num_flow_rules;

	struct gve_flow_rules_cache flow_rules_cache;

	u16 rss_key_size;
	u16 rss_lut_size;
	bool cache_rss_config;
	struct gve_rss_config rss_config;

	bool nic_timestamp_supported;
	enum gve_dev_clk_read_type clk_read_type;
	__le64 __iomem *dev_clk_ns_l;
	__le64 __iomem *dev_clk_ns_h;
	__le64 __iomem *dev_art_ns_l;
	__le64 __iomem *dev_art_ns_h;
	__le64 __iomem *dev_clk_cmd_sync;
	spinlock_t clk_lock;
	struct gve_ptp *ptp;
	struct kernel_hwtstamp_config ts_config;
	struct gve_nic_ts_report *nic_ts_report;
	dma_addr_t nic_ts_report_bus;
	struct mutex nic_ts_read_lock; /* Protects nic timestamp reads */
	u64 last_sync_nic_counter; /* Clock counter from last NIC TS report */
};

enum gve_service_task_flags_bit {
	GVE_FLAGS_DO_RESET		= 1,
	GVE_FLAGS_RESET_IN_PROGRESS	= 2,
	GVE_FLAGS_PROBE_IN_PROGRESS	= 3,
	GVE_FLAGS_DO_REPORT_STATS	= 4,
};

enum gve_state_flags_bit {
	GVE_FLAGS_ADMIN_QUEUE_OK	= 1,
	GVE_FLAGS_DEVICE_RESOURCES_OK	= 2,
	GVE_FLAGS_DEVICE_RINGS_OK	= 3,
	GVE_FLAGS_NAPI_ENABLED		= 4,
	GVE_FLAGS_MBX_QUEUE_OK		= 5,
	GVE_FLAGS_MAILBOX_INTERRUPT_OK	= 6,
};

enum gve_ethtool_flags_bit {
	GVE_PRIV_FLAGS_REPORT_STATS		= 0,
};

static inline bool gve_get_do_reset(struct gve_adapter *adapter)
{
	return test_bit(GVE_FLAGS_DO_RESET, &adapter->service_task_flags);
}

static inline void gve_set_do_reset(struct gve_adapter *adapter)
{
	set_bit(GVE_FLAGS_DO_RESET, &adapter->service_task_flags);
}

static inline void gve_clear_do_reset(struct gve_adapter *adapter)
{
	clear_bit(GVE_FLAGS_DO_RESET, &adapter->service_task_flags);
}

static inline bool gve_get_reset_in_progress(struct gve_adapter *adapter)
{
	return test_bit(GVE_FLAGS_RESET_IN_PROGRESS,
			&adapter->service_task_flags);
}

static inline void gve_set_reset_in_progress(struct gve_adapter *adapter)
{
	set_bit(GVE_FLAGS_RESET_IN_PROGRESS, &adapter->service_task_flags);
}

static inline void gve_clear_reset_in_progress(struct gve_adapter *adapter)
{
	clear_bit(GVE_FLAGS_RESET_IN_PROGRESS, &adapter->service_task_flags);
}

static inline bool gve_get_probe_in_progress(struct gve_adapter *adapter)
{
	return test_bit(GVE_FLAGS_PROBE_IN_PROGRESS,
			&adapter->service_task_flags);
}

static inline void gve_set_probe_in_progress(struct gve_adapter *adapter)
{
	set_bit(GVE_FLAGS_PROBE_IN_PROGRESS, &adapter->service_task_flags);
}

static inline void gve_clear_probe_in_progress(struct gve_adapter *adapter)
{
	clear_bit(GVE_FLAGS_PROBE_IN_PROGRESS, &adapter->service_task_flags);
}

static inline bool gve_get_do_report_stats(struct gve_adapter *adapter)
{
	return test_bit(GVE_FLAGS_DO_REPORT_STATS,
			&adapter->service_task_flags);
}

static inline void gve_set_do_report_stats(struct gve_adapter *adapter)
{
	set_bit(GVE_FLAGS_DO_REPORT_STATS, &adapter->service_task_flags);
}

static inline void gve_clear_do_report_stats(struct gve_adapter *adapter)
{
	clear_bit(GVE_FLAGS_DO_REPORT_STATS, &adapter->service_task_flags);
}

static inline bool gve_get_admin_queue_ok(struct gve_adapter *adapter)
{
	return test_bit(GVE_FLAGS_ADMIN_QUEUE_OK, &adapter->state_flags);
}

static inline void gve_set_admin_queue_ok(struct gve_adapter *adapter)
{
	set_bit(GVE_FLAGS_ADMIN_QUEUE_OK, &adapter->state_flags);
}

static inline void gve_clear_admin_queue_ok(struct gve_adapter *adapter)
{
	clear_bit(GVE_FLAGS_ADMIN_QUEUE_OK, &adapter->state_flags);
}

static inline bool gve_get_device_resources_ok(struct gve_adapter *adapter)
{
	return test_bit(GVE_FLAGS_DEVICE_RESOURCES_OK, &adapter->state_flags);
}

static inline void gve_set_device_resources_ok(struct gve_adapter *adapter)
{
	set_bit(GVE_FLAGS_DEVICE_RESOURCES_OK, &adapter->state_flags);
}

static inline void gve_clear_device_resources_ok(struct gve_adapter *adapter)
{
	clear_bit(GVE_FLAGS_DEVICE_RESOURCES_OK, &adapter->state_flags);
}

static inline bool gve_get_device_rings_ok(struct gve_adapter *adapter)
{
	return test_bit(GVE_FLAGS_DEVICE_RINGS_OK, &adapter->state_flags);
}

static inline void gve_set_device_rings_ok(struct gve_adapter *adapter)
{
	set_bit(GVE_FLAGS_DEVICE_RINGS_OK, &adapter->state_flags);
}

static inline void gve_clear_device_rings_ok(struct gve_adapter *adapter)
{
	clear_bit(GVE_FLAGS_DEVICE_RINGS_OK, &adapter->state_flags);
}

static inline bool gve_get_napi_enabled(struct gve_adapter *adapter)
{
	return test_bit(GVE_FLAGS_NAPI_ENABLED, &adapter->state_flags);
}

static inline void gve_set_napi_enabled(struct gve_adapter *adapter)
{
	set_bit(GVE_FLAGS_NAPI_ENABLED, &adapter->state_flags);
}

static inline void gve_clear_napi_enabled(struct gve_adapter *adapter)
{
	clear_bit(GVE_FLAGS_NAPI_ENABLED, &adapter->state_flags);
}

static inline bool gve_get_report_stats(struct gve_priv *priv)
{
	return test_bit(GVE_PRIV_FLAGS_REPORT_STATS, &priv->ethtool_flags);
}

static inline void gve_clear_report_stats(struct gve_priv *priv)
{
	clear_bit(GVE_PRIV_FLAGS_REPORT_STATS, &priv->ethtool_flags);
}

static inline bool gve_get_mbx_queue_ok(struct gve_adapter *adapter)
{
	return test_bit(GVE_FLAGS_MBX_QUEUE_OK, &adapter->state_flags);
}

static inline void gve_set_mbx_queue_ok(struct gve_adapter *adapter)
{
	set_bit(GVE_FLAGS_MBX_QUEUE_OK, &adapter->state_flags);
}

static inline void gve_clear_mbx_queue_ok(struct gve_adapter *adapter)
{
	clear_bit(GVE_FLAGS_MBX_QUEUE_OK, &adapter->state_flags);
}

static inline bool gve_get_mailbox_interrupt_ok(struct gve_adapter *adapter)
{
	return test_bit(GVE_FLAGS_MAILBOX_INTERRUPT_OK, &adapter->state_flags);
}

static inline void gve_set_mailbox_interrupt_ok(struct gve_adapter *adapter)
{
	set_bit(GVE_FLAGS_MAILBOX_INTERRUPT_OK, &adapter->state_flags);
}

static inline void gve_clear_mailbox_interrupt_ok(struct gve_adapter *adapter)
{
	clear_bit(GVE_FLAGS_MAILBOX_INTERRUPT_OK, &adapter->state_flags);
}

/* Returns the address of the ntfy_blocks irq doorbell
 */
static inline __be32 __iomem *gve_irq_doorbell(struct gve_priv *priv,
					       struct gve_notify_block *block)
{
	return &priv->db_adminq_bar2[be32_to_cpu(*block->aq_irq_db_index)];
}

/* Returns the index into ntfy_blocks of the given tx ring's block
 */
static inline u32 gve_tx_idx_to_ntfy(struct gve_priv *priv, u32 queue_idx)
{
	return queue_idx;
}

/* Returns the index into ntfy_blocks of the given rx ring's block
 */
static inline u32 gve_rx_idx_to_ntfy(struct gve_priv *priv, u32 queue_idx)
{
	return (priv->num_ntfy_blks / 2) + queue_idx;
}

static inline u32 gve_msix_idx_to_ntfy(struct gve_priv *priv, u32 msix_idx)
{
	/* In mailbox mode, the first msix vector is reserved for the mailbox
	 * IRQ vector, so data queue MSI-X vectors start at index 1.
	 */
	int base = (priv->adapter->mailbox_mode) ? 1 : 0;

	return msix_idx - base;
}

static inline u32 gve_ntfy_to_msix_idx(struct gve_priv *priv, u32 ntfy_blk_idx)
{
	/* In mailbox mode, the first msix vector is reserved for the mailbox
	 * IRQ vector, so data queue MSI-X vectors start at index 1.
	 */
	int base = (priv->adapter->mailbox_mode) ? 1 : 0;

	return base + ntfy_blk_idx;
}

static inline bool gve_is_qpl(struct gve_priv *priv)
{
	return priv->queue_format == GVE_GQI_QPL_FORMAT ||
		priv->queue_format == GVE_DQO_QPL_FORMAT;
}

/* Returns the number of tx queue page lists */
static inline u32 gve_num_tx_qpls(const struct gve_tx_queue_config *tx_cfg,
				  bool is_qpl)
{
	if (!is_qpl)
		return 0;
	return tx_cfg->num_queues + tx_cfg->num_xdp_queues;
}

/* Returns the number of rx queue page lists */
static inline u32 gve_num_rx_qpls(const struct gve_rx_queue_config *rx_cfg,
				  bool is_qpl)
{
	if (!is_qpl)
		return 0;
	return rx_cfg->num_queues;
}

static inline u32 gve_tx_qpl_id(struct gve_priv *priv, int tx_qid)
{
	return tx_qid;
}

static inline u32 gve_rx_qpl_id(struct gve_priv *priv, int rx_qid)
{
	return priv->tx_cfg.max_queues + rx_qid;
}

static inline u32 gve_get_rx_qpl_id(const struct gve_tx_queue_config *tx_cfg,
				    int rx_qid)
{
	return tx_cfg->max_queues + rx_qid;
}

static inline u32 gve_tx_start_qpl_id(struct gve_priv *priv)
{
	return gve_tx_qpl_id(priv, 0);
}

static inline u32 gve_rx_start_qpl_id(const struct gve_tx_queue_config *tx_cfg)
{
	return gve_get_rx_qpl_id(tx_cfg, 0);
}

/* Returns the correct dma direction for tx and rx qpls */
static inline enum dma_data_direction gve_qpl_dma_dir(struct gve_priv *priv,
						      int id)
{
	if (id < gve_rx_start_qpl_id(&priv->tx_cfg))
		return DMA_TO_DEVICE;
	else
		return DMA_FROM_DEVICE;
}

static inline bool gve_is_gqi(struct gve_priv *priv)
{
	return priv->queue_format == GVE_GQI_RDA_FORMAT ||
		priv->queue_format == GVE_GQI_QPL_FORMAT;
}

static inline bool gve_is_dqo(struct gve_priv *priv)
{
	return priv->queue_format == GVE_DQO_RDA_FORMAT ||
	       priv->queue_format == GVE_DQO_QPL_FORMAT;
}

static inline u32 gve_num_tx_queues(struct gve_priv *priv)
{
	return priv->tx_cfg.num_queues + priv->tx_cfg.num_xdp_queues;
}

static inline u32 gve_xdp_tx_queue_id(struct gve_priv *priv, u32 queue_id)
{
	return priv->tx_cfg.num_queues + queue_id;
}

static inline u32 gve_xdp_tx_start_queue_id(struct gve_priv *priv)
{
	return gve_xdp_tx_queue_id(priv, 0);
}

static inline bool gve_supports_xdp_xmit(struct gve_priv *priv)
{
	switch (priv->queue_format) {
	case GVE_GQI_QPL_FORMAT:
	case GVE_DQO_RDA_FORMAT:
		return true;
	default:
		return false;
	}
}

static inline bool gve_is_clock_running(struct gve_priv *priv)
{
	return priv->ptp && priv->ptp->started;
}

void gve_adminq_write_version(u8 __iomem *driver_version_register);

/* gqi napi handler defined in gve_main.c */
int gve_napi_poll(struct napi_struct *napi, int budget);

/* buffers */
int gve_alloc_page(struct gve_priv *priv, struct device *dev,
		   struct page **page, dma_addr_t *dma,
		   enum dma_data_direction, gfp_t gfp_flags);
void gve_free_page(struct device *dev, struct page *page, dma_addr_t dma,
		   enum dma_data_direction);
/* qpls */
struct gve_queue_page_list *gve_alloc_queue_page_list(struct gve_priv *priv,
						      u32 id, int pages);
void gve_free_queue_page_list(struct gve_priv *priv,
			      struct gve_queue_page_list *qpl,
			      u32 id);
/* tx handling */
netdev_tx_t gve_tx(struct sk_buff *skb, struct net_device *dev);
int gve_xdp_xmit_gqi(struct net_device *dev, int n, struct xdp_frame **frames,
		     u32 flags);
int gve_xdp_xmit_dqo(struct net_device *dev, int n, struct xdp_frame **frames,
		     u32 flags);
int gve_xdp_xmit_one(struct gve_priv *priv, struct gve_tx_ring *tx,
		     void *data, int len, void *frame_p);
void gve_xdp_tx_flush(struct gve_priv *priv, u32 xdp_qid);
int gve_xdp_xmit_one_dqo(struct gve_priv *priv, struct gve_tx_ring *tx,
			 struct xdp_frame *xdpf);
bool gve_tx_poll(struct gve_notify_block *block, int budget);
bool gve_xdp_poll(struct gve_notify_block *block, int budget);
int gve_xsk_tx_poll(struct gve_notify_block *block, int budget);
int gve_tx_alloc_rings_gqi(struct gve_priv *priv,
			   struct gve_tx_alloc_rings_cfg *cfg);
void gve_tx_free_rings_gqi(struct gve_priv *priv,
			   struct gve_tx_alloc_rings_cfg *cfg);
void gve_tx_start_ring_gqi(struct gve_priv *priv, int idx);
void gve_tx_stop_ring_gqi(struct gve_priv *priv, int idx);
u32 gve_tx_load_event_counter(struct gve_priv *priv,
			      struct gve_tx_ring *tx);
bool gve_tx_clean_pending(struct gve_priv *priv, struct gve_tx_ring *tx);
/* rx handling */
void gve_rx_write_doorbell(struct gve_priv *priv, struct gve_rx_ring *rx);
int gve_rx_poll(struct gve_notify_block *block, int budget);
bool gve_rx_work_pending(struct gve_rx_ring *rx);
int gve_rx_alloc_ring_gqi(struct gve_priv *priv,
			  struct gve_rx_alloc_rings_cfg *cfg,
			  struct gve_rx_ring *rx,
			  int idx);
void gve_rx_free_ring_gqi(struct gve_priv *priv, struct gve_rx_ring *rx,
			  struct gve_rx_alloc_rings_cfg *cfg);
int gve_rx_alloc_rings_gqi(struct gve_priv *priv,
			   struct gve_rx_alloc_rings_cfg *cfg);
void gve_rx_free_rings_gqi(struct gve_priv *priv,
			   struct gve_rx_alloc_rings_cfg *cfg);
void gve_rx_start_ring_gqi(struct gve_priv *priv, int idx);
void gve_rx_stop_ring_gqi(struct gve_priv *priv, int idx);
bool gve_header_split_supported(const struct gve_priv *priv);
int gve_set_rx_buf_len_config(struct gve_priv *priv, u32 rx_buf_len,
			      struct netlink_ext_ack *extack,
			      struct gve_rx_alloc_rings_cfg *rx_alloc_cfg);
int gve_set_hsplit_config(struct gve_priv *priv, u8 tcp_data_split,
			  struct gve_rx_alloc_rings_cfg *rx_alloc_cfg);
/* rx buffer handling */
int gve_buf_ref_cnt(struct gve_rx_buf_state_dqo *bs);
void gve_free_page_dqo(struct gve_priv *priv, struct gve_rx_buf_state_dqo *bs,
		       bool free_page);
struct gve_rx_buf_state_dqo *gve_alloc_buf_state(struct gve_rx_ring *rx);
bool gve_buf_state_is_allocated(struct gve_rx_ring *rx,
				struct gve_rx_buf_state_dqo *buf_state);
void gve_free_buf_state(struct gve_rx_ring *rx,
			struct gve_rx_buf_state_dqo *buf_state);
struct gve_rx_buf_state_dqo *gve_dequeue_buf_state(struct gve_rx_ring *rx,
						   struct gve_index_list *list);
void gve_enqueue_buf_state(struct gve_rx_ring *rx, struct gve_index_list *list,
			   struct gve_rx_buf_state_dqo *buf_state);
struct gve_rx_buf_state_dqo *gve_get_recycled_buf_state(struct gve_rx_ring *rx);
void gve_try_recycle_buf(struct gve_priv *priv, struct gve_rx_ring *rx,
			 struct gve_rx_buf_state_dqo *buf_state);
void gve_free_to_page_pool(struct gve_rx_ring *rx,
			   struct gve_rx_buf_state_dqo *buf_state,
			   bool allow_direct);
int gve_alloc_qpl_page_dqo(struct gve_rx_ring *rx,
			   struct gve_rx_buf_state_dqo *buf_state);
void gve_free_qpl_page_dqo(struct gve_rx_buf_state_dqo *buf_state);
void gve_reuse_buffer(struct gve_rx_ring *rx,
		      struct gve_rx_buf_state_dqo *buf_state);
void gve_free_buffer(struct gve_rx_ring *rx,
		     struct gve_rx_buf_state_dqo *buf_state);
int gve_alloc_buffer(struct gve_rx_ring *rx, struct gve_rx_desc_dqo *desc);
struct page_pool *gve_rx_create_page_pool(struct gve_priv *priv,
					  struct gve_rx_ring *rx,
					  bool xdp);

/* Reset */
void gve_schedule_reset(struct gve_priv *priv);
int gve_reset(struct gve_priv *priv);
void gve_get_curr_alloc_cfgs(struct gve_priv *priv,
			     struct gve_tx_alloc_rings_cfg *tx_alloc_cfg,
			     struct gve_rx_alloc_rings_cfg *rx_alloc_cfg);
void gve_update_num_qpl_pages(struct gve_priv *priv,
			      struct gve_rx_alloc_rings_cfg *rx_alloc_cfg,
			      struct gve_tx_alloc_rings_cfg *tx_alloc_cfg);
int gve_adjust_config(struct gve_priv *priv,
		      struct gve_tx_alloc_rings_cfg *tx_alloc_cfg,
		      struct gve_rx_alloc_rings_cfg *rx_alloc_cfg);
int gve_adjust_queues(struct gve_priv *priv,
		      struct gve_rx_queue_config new_rx_config,
		      struct gve_tx_queue_config new_tx_config,
		      bool reset_rss);
/* Initialization sequence */
int gve_alloc_counter_array(struct gve_priv *priv);
void gve_free_counter_array(struct gve_priv *priv);
/* flow steering rule */
int gve_get_flow_rule_entry(struct gve_priv *priv, struct ethtool_rxnfc *cmd);
int gve_get_flow_rule_ids(struct gve_priv *priv, struct ethtool_rxnfc *cmd, u32 *rule_locs);
int gve_add_flow_rule(struct gve_priv *priv, struct ethtool_rxnfc *cmd);
int gve_del_flow_rule(struct gve_priv *priv, struct ethtool_rxnfc *cmd);
int gve_flow_rules_reset(struct gve_priv *priv);
/* RSS config */
int gve_init_rss_config(struct gve_priv *priv, u16 num_queues);
int gve_configure_rss(struct gve_priv *priv,
		      struct ethtool_rxfh_param *rxfh);
void gve_handle_link_status(struct gve_priv *priv);
/* PTP and timestamping */
#if IS_ENABLED(CONFIG_PTP_1588_CLOCK)
int gve_ptp_register(struct gve_priv *priv);
void gve_ptp_unregister(struct gve_priv *priv);
int gve_ptp_start(struct gve_priv *priv);
void gve_ptp_stop(struct gve_priv *priv);
#else /* CONFIG_PTP_1588_CLOCK */

static inline int gve_ptp_register(struct gve_priv *priv)
{
	return 0;
}

static inline void gve_ptp_unregister(struct gve_priv *priv) { }
static inline int gve_ptp_start(struct gve_priv *priv) { return 0; }
static inline void gve_ptp_stop(struct gve_priv *priv) { }
#endif /* CONFIG_PTP_1588_CLOCK */
/* report stats handling */
void gve_handle_report_stats(struct gve_priv *priv);
/* exported by ethtool.c */
extern const struct ethtool_ops gve_ethtool_ops;
/* needed by ethtool */
extern char gve_driver_name[];
extern const char gve_version_str[];
#endif /* _GVE_H_ */
