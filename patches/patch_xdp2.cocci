@@
@@
struct gve_rx_ring {
	...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
	/* XDP stuff */
	struct xdp_rxq_info xdp_rxq;
	struct xdp_rxq_info xsk_rxq;
	struct xsk_buff_pool *xsk_pool;
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */
	...
};

@@
@@
struct gve_tx_ring {
	...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
        struct xsk_buff_pool *xsk_pool;
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */
	...
};

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_xsk_pool_redirect(struct net_device *dev,
				 struct gve_rx_ring *rx,
				 void *data, int len,
				 struct bpf_prog *xdp_prog)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static int gve_xdp_redirect(struct net_device *dev, struct gve_rx_ring *rx,
			    struct xdp_buff *orig, struct bpf_prog *xdp_prog)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */


@@
type bool;
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
static bool gve_xdp_done(struct gve_priv *priv, struct gve_rx_ring *rx,
			 struct xdp_buff *xdp, struct bpf_prog *xprog,
			 int xdp_act)
{
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@ assign @
identifier xprog, xdp;
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
	struct bpf_prog *xprog;
	struct xdp_buff xdp;
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@ gve_rx @
identifier assign.xprog, assign.xdp;
identifier READ_ONCE;
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
xprog = READ_ONCE(priv->xdp_prog);
if (xprog && is_only_frag) {
...
}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */


@ assign2 @
identifier xdp_redirects, rx;
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
	u64 xdp_redirects = rx->xdp_actions[XDP_REDIRECT];
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
identifier assign2.xdp_redirects, assign2.rx;
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
	if (xdp_redirects != rx->xdp_actions[XDP_REDIRECT])
		xdp_do_flush();
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@ assign3 @
identifier xdp_txs, rx;
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
	u64 xdp_txs = rx->xdp_actions[XDP_TX];
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */

@@
identifier assign3.xdp_txs, assign3.rx;
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
	if (xdp_txs != rx->xdp_actions[XDP_TX])
		gve_xdp_tx_flush(priv, rx->q_num);
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)) */
