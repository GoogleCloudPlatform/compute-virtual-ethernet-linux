/* SPDX-License-Identifier: (GPL-2.0 OR MIT)
 * Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2015-2018 Google, Inc.
 */

/* GVE Transmit Descriptor formats */

#ifndef _GVE_DESC_H_
#define _GVE_DESC_H_

#include "gve_size_assert.h"

struct gve_tx_pkt_desc {
	u8	type_flags;
	u8	checksum_offset;
	u8	l4_offset;
	u8	seg_cnt;
	__be16	len;
	__be16	seg_len;
	__be64	seg_addr;
} __packed;

struct gve_tx_seg_desc {
	u8	type_flags;	/* type is lower 4 bits, flags upper	*/
	u8	l3_offset;	/* TSO: 2 byte units to start of IPH	*/
	__be16	reserved;
	__be16	mss;		/* TSO MSS				*/
	__be16	seg_len;
	__be64	seg_addr;
} __packed;

/* GVE Transmit Descriptor Types */
#define	GVE_TXD_STD		(0x0 << 4) /* Std with Host Address	*/
#define	GVE_TXD_TSO		(0x1 << 4) /* TSO with Host Address	*/
#define	GVE_TXD_SEG		(0x2 << 4) /* Seg with Host Address	*/

/* GVE Transmit Descriptor Flags for Std Pkts */
#define	GVE_TXF_L4CSUM	BIT(0)	/* Need csum offload */
#define	GVE_TXF_TSTAMP	BIT(2)	/* Timestamp required */

/* GVE Transmit Descriptor Flags for TSO Segs */
#define	GVE_TXSF_IPV6	BIT(1)	/* IPv6 TSO */

/* GVE Receive Packet Descriptor */
/* The start of an ethernet packet comes 2 bytes into
 * the rx buffer.  This way, both the DMA and the L3/4 protocol
 * header access is aligned
 */
#define GVE_RX_PAD 2

struct gve_rx_desc {
	u8	padding[48];
	__be32	rss_hash;
	__be16	mss;
	__be16	reserved;
	u8	hdr_len;
	u8	hdr_off;
	__be16	csum;
	__be16	len;
	__be16	flags_seq;
} __packed;
GVE_ASSERT_SIZE(struct, gve_rx_desc, 64);

struct gve_rx_data_slot {
	/* byte offset into the rx registered segment of this slot */
	__be64 qpl_offset;
};

/* GVE Recive Packet Descriptor Seq No */

#ifdef __LITTLE_ENDIAN
#define GVE_SEQNO(x) ((((__force u16)x) >> 8) & 0x7)
#else
#define	GVE_SEQNO(x) ((__force u16)(x) & 0x7)
#endif

/* GVE Recive Packet Descriptor Flags */
#define GVE_RXFLG(x)	cpu_to_be16(1 << (3 + (x)))
#define	GVE_RXF_FRAG	GVE_RXFLG(3)	/* IP Fragment			*/
#define	GVE_RXF_IPV4	GVE_RXFLG(4)	/* IPv4				*/
#define	GVE_RXF_IPV6	GVE_RXFLG(5)	/* IPv6				*/
#define	GVE_RXF_TCP	GVE_RXFLG(6)	/* TCP Packet			*/
#define	GVE_RXF_UDP	GVE_RXFLG(7)	/* UDP Packet			*/

/* GVE IRQ */
#define GVE_IRQ_ACK	BIT(31)
#define GVE_IRQ_MASK	BIT(30)
#define GVE_IRQ_EVENT	BIT(29)

static inline bool gve_rss_valid(__be16 flag)
{
	if (flag & GVE_RXF_FRAG)
		return false;
	if (flag & (GVE_RXF_IPV4 | GVE_RXF_IPV6))
		return true;
	return false;
}

static inline u8 gve_next_seqno(u8 seq)
{
	return (seq + 1) == 8 ? 1 : seq + 1;
}
#endif /* _GVE_DESC_H_ */
