@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
#include <linux/filter.h>
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
#include <net/xdp_sock_drv.h>
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
#include <net/xdp.h>
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */


@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
#include <linux/bpf.h>
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
static int gve_reg_xdp_info(struct gve_priv *priv, struct net_device *dev)
{
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
...
+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */
+	return 0;
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */
}

@@
@@
static void gve_unreg_xdp_info(struct gve_priv *priv)
{
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
...
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */
}

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
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_set_xdp(struct gve_priv *priv, struct bpf_prog *prog,
		       struct netlink_ext_ack *extack)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_xsk_pool_enable(struct net_device *dev,
			       struct xsk_buff_pool *pool,
			       u16 qid)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_xsk_pool_disable(struct net_device *dev,
				u16 qid)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_xsk_wakeup(struct net_device *dev, u32 queue_id, u32 flags)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int verify_xdp_configuration(struct net_device *dev)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_xdp(struct net_device *dev, struct netdev_bpf *xdp)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
identifier gve_netdev_ops;
identifier gve_xdp, gve_xdp_xmit, gve_xsk_wakeup;
@@
struct net_device_ops gve_netdev_ops = {
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
	.ndo_bpf		=	gve_xdp,
	.ndo_xdp_xmit		=	gve_xdp_xmit,
	.ndo_xsk_wakeup		=	gve_xsk_wakeup,
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */
};

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_xsk_tx(struct gve_priv *priv, struct gve_tx_ring *tx,
		       int budget)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
type bool;
identifier gve_xdp_poll, block, budget;
identifier tx, repoll;
@@
bool gve_xdp_poll(struct gve_notify_block *block, int budget)
{
...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
	if (tx->xsk_pool) {
	...
	}

+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */
	/* If we still have work we want to repoll */
	return repoll;
}


@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
	if (xsk_complete > 0 && tx->xsk_pool)
		xsk_tx_completed(tx->xsk_pool, xsk_complete);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */


@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
int gve_xdp_xmit(struct net_device *dev, int n, struct xdp_frame **frames,
		 u32 flags)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
int gve_xdp_xmit_one(struct gve_priv *priv, struct gve_tx_ring *tx,
		     void *data, int len, void *frame_p)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */


@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_tx_fill_xdp(struct gve_priv *priv, struct gve_tx_ring *tx,
			   void *data, int len, void *frame_p, bool is_xsk)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_remove_xdp_queues(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_add_xdp_queues(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static void gve_free_xdp_qpls(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_alloc_xdp_qpls(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static void gve_free_xdp_rings(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_destroy_xdp_rings(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_alloc_xdp_rings(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_create_xdp_rings(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_unregister_xdp_qpls(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_register_xdp_qpls(struct gve_priv *priv)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static void add_napi_init_xdp_sync_stats(struct gve_priv *priv,
					 int (*napi_poll)(struct napi_struct *napi,
							  int budget))
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
					xdp_return_frame(info->xdp_frame);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
static void gve_set_netdev_xdp_features(struct gve_priv *priv)
{
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0))
...
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)) */
}
