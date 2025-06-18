@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
#include <linux/filter.h>
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
#include <net/xdp_sock_drv.h>
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
#include <net/xdp.h>
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */


@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
#include <linux/bpf.h>
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
static int gve_reg_xdp_info(struct gve_priv *priv, struct net_device *dev)
{
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
...
+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */
+	return 0;
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */
}

@@
@@
static void gve_unreg_xdp_info(struct gve_priv *priv)
{
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
...
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */
}

@@
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)
err = xdp_rxq_info_reg_mem_model(
				&rx->xdp_rxq, MEM_TYPE_PAGE_POOL,
				rx->dqo.page_pool);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0) */
+err = xdp_rxq_info_reg_mem_model(
+				&rx->xdp_rxq, MEM_TYPE_PAGE_ORDER0,
+				NULL);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0) */

@@
@@
static void gve_drain_page_cache(struct gve_priv *priv)
{
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
	...
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0) */
}

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int gve_configure_rings_xdp(...)
{
	...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,14,0))
+#ifndef XDP_PACKET_HEADROOM
+#define XDP_PACKET_HEADROOM 0
+#endif
+#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(5,14,0)) */
#define GVE_XDP_RX_BUFFER_SIZE_DQO 4096

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int gve_set_xdp(struct gve_priv *priv, struct bpf_prog *prog,
		       struct netlink_ext_ack *extack)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int gve_xsk_pool_enable(struct net_device *dev,
			       struct xsk_buff_pool *pool,
			       u16 qid)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int gve_xsk_pool_disable(struct net_device *dev,
				u16 qid)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int gve_xsk_wakeup(struct net_device *dev, u32 queue_id, u32 flags)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int verify_xdp_configuration(struct net_device *dev)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int gve_xdp(struct net_device *dev, struct netdev_bpf *xdp)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
identifier gve_netdev_ops;
identifier gve_xdp, gve_xdp_xmit, gve_xsk_wakeup;
@@
struct net_device_ops gve_netdev_ops = {
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
	.ndo_bpf		=	gve_xdp,
	.ndo_xdp_xmit		=	gve_xdp_xmit,
	.ndo_xsk_wakeup		=	gve_xsk_wakeup,
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */
};

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
int gve_xsk_tx(...) { ... }
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
int gve_xsk_tx_poll(...);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
int gve_xsk_tx_poll(...) { ... }
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
identifier work_done, block, budget;
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
work_done = max_t(int, work_done, gve_xsk_tx_poll(block, budget));
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */


@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
	if (xsk_complete > 0 && tx->xsk_pool)
		xsk_tx_completed(tx->xsk_pool, xsk_complete);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */


@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int gve_xdp_xmit(...) { ... }
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
int gve_xdp_xmit_gqi(...) { ... }
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
int gve_xdp_xmit_dqo(struct net_device *dev, int n, struct xdp_frame **frames,
		 u32 flags)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
int gve_xdp_xmit_one(struct gve_priv *priv, struct gve_tx_ring *tx,
		     void *data, int len, void *frame_p)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */


@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int gve_tx_fill_xdp(struct gve_priv *priv, struct gve_tx_ring *tx,
			   void *data, int len, void *frame_p, bool is_xsk)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int gve_remove_xdp_queues(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int gve_add_xdp_queues(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static void gve_free_xdp_qpls(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int gve_alloc_xdp_qpls(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static void gve_free_xdp_rings(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int gve_destroy_xdp_rings(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int gve_alloc_xdp_rings(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int gve_create_xdp_rings(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int gve_unregister_xdp_qpls(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static int gve_register_xdp_qpls(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
static void add_napi_init_xdp_sync_stats(struct gve_priv *priv,
					 int (*napi_poll)(struct napi_struct *napi,
							  int budget))
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
int gve_xdp_xmit_one_dqo(struct gve_priv *priv, struct gve_tx_ring *tx,
			 struct xdp_frame *xdpf)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
		gve_unmap_packet(tx->dev, pending_packet);
		(*pkts)++;
		*bytes += pending_packet->xdpf->len;
		...
		pending_packet->xdpf = NULL;
		gve_free_pending_packet(tx, pending_packet);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL)
					xdp_return_frame(info->xdp_frame);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) || defined(KUNIT_KERNEL) */

@@
@@
static void gve_set_netdev_xdp_features(struct gve_priv *priv)
{
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)) || defined(KUNIT_KERNEL) || RHEL_VERSION_GTE(9,4)
...
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)) || defined(KUNIT_KERNEL) || RHEL_VERSION_GTE(9,4) */
}

@@
expression list args;
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,15,0))
xdp_set_features_flag_locked(args);
+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,15,0)) */
+xdp_set_features_flag(args);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,15,0)) */

@@
expression list args;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,15,0)
xdp_features_set_redirect_target_locked(args);
+#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(9,4)
+xdp_features_set_redirect_target(args);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(9,4) */


@@
expression list args;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,15,0)
xdp_features_clear_redirect_target_locked(args);
+#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(9,4)
+xdp_features_clear_redirect_target(args);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(9,4) */

@add_xdp_qfmt_vefify@
parameter list args;
identifier dev;
@@
int verify_xdp_configuration(args
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)) || defined(KUNIT_KERNEL)
+, struct netdev_bpf *xdp
+#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)) || defined(KUNIT_KERNEL) */
 )
{
...
if (dev->features & NETIF_F_LRO) { ... }
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)) || defined(KUNIT_KERNEL)
+	/* Check XDP support for various queue formats. */
+	switch (priv->queue_format) {
+	case GVE_GQI_QPL_FORMAT: /* GQI_QPL supports everything, so ignore. */
+		break;
+	case GVE_DQO_RDA_FORMAT:
+		if (xdp->command == XDP_SETUP_XSK_POOL) {
+			netdev_warn(dev, "AF_XDP zero-copy is not supported in mode %d\n",
+				    priv->queue_format);
+			return -EOPNOTSUPP;
+		}
+		break;
+	default:
+		netdev_warn(dev, "XDP is not supported in mode %d.\n",
+			    priv->queue_format);
+		return -EOPNOTSUPP;
+	}
+#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)) || defined(KUNIT_KERNEL) */
...
}

@@
expression err;
@@
static int gve_xdp(struct net_device *dev, struct netdev_bpf *xdp) {
...
err = verify_xdp_configuration(dev
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)) || defined(KUNIT_KERNEL)
+, xdp
+#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)) || defined(KUNIT_KERNEL) */
 );
...
}

