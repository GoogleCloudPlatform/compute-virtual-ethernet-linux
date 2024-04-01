@ ethtool_netlink @
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0))
#include <linux/ethtool_netlink.h>
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) */
+#include <linux/ethtool.h>
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) */


@ gve_get_ringparam @
@@
gve_get_ringparam(...)
{
...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0))
	if (!gve_header_split_supported(priv))
		kernel_cmd->tcp_data_split = ETHTOOL_TCP_DATA_SPLIT_UNKNOWN;
	else if (priv->header_split_enabled)
		kernel_cmd->tcp_data_split = ETHTOOL_TCP_DATA_SPLIT_ENABLED;
	else
		kernel_cmd->tcp_data_split = ETHTOOL_TCP_DATA_SPLIT_DISABLED;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) */
...
}


@ gve_set_ringparam @
@@
gve_set_ringparam(...)
{
...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0))
	int err;

	err = gve_set_hsplit_config(...);
	if (err)
		return err;

+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) */
...
}


@ gve_ethtool_ops @
identifier gve_ethtool_ops, ethtool_use_hsplit_flag;
@@
const struct ethtool_ops gve_ethtool_ops = {
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0))
	.supported_ring_params = ethtool_use_hsplit_flag,
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) */
	.get_drvinfo = gve_get_drvinfo,
};


@ gve_set_hsplit_config @
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0))
int gve_set_hsplit_config(struct gve_priv *priv, u8 tcp_data_split)
{
	...
}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) */


@ gve_set_hsplit_config_declaration @
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0))
int gve_set_hsplit_config(struct gve_priv *priv, u8 tcp_data_split);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) */


@ add_gve_priv_variables @
@@
struct gve_priv {
...
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0))
+u8 header_split_strict;
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0) */
};


@ add_priv_flag_bits @
@@
enum gve_ethtool_flags_bit {
	GVE_PRIV_FLAGS_REPORT_STATS		  = 0,
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0))
+	GVE_PRIV_FLAGS_ENABLE_HEADER_SPLIT	  = 1,
+	GVE_PRIV_FLAGS_ENABLE_STRICT_HEADER_SPLIT = 2,
+	GVE_PRIV_FLAGS_ENABLE_MAX_RX_BUFFER_SIZE  = 3,
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0) */
};
+
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0))
+#define GVE_PRIV_FLAGS_MASK \
+	(BIT(GVE_PRIV_FLAGS_REPORT_STATS)		| \
+	 BIT(GVE_PRIV_FLAGS_ENABLE_HEADER_SPLIT)	| \
+	 BIT(GVE_PRIV_FLAGS_ENABLE_STRICT_HEADER_SPLIT)	| \
+	 BIT(GVE_PRIV_FLAGS_ENABLE_MAX_RX_BUFFER_SIZE))
+
+static inline int gve_get_enable_header_split(struct gve_priv *priv)
+{
+	return test_bit(GVE_PRIV_FLAGS_ENABLE_HEADER_SPLIT, &priv->ethtool_flags);
+}
+
+static inline int gve_get_enable_max_rx_buffer_size(struct gve_priv *priv)
+{
+	return test_bit(GVE_PRIV_FLAGS_ENABLE_MAX_RX_BUFFER_SIZE, &priv->ethtool_flags);
+}
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0) */


@ gve_get_priv_flags @
identifier gve_get_priv_flags;
@@
gve_get_priv_flags(...)
{
	struct gve_priv *priv = netdev_priv(netdev);
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0))
	u32 ret_flags = 0;

	/* Only 1 flag exists currently: report-stats (BIT(O)), so set that flag. */
	if (priv->ethtool_flags & BIT(0))
		ret_flags |= BIT(0);
	return ret_flags;
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) */
+	return priv->ethtool_flags & GVE_PRIV_FLAGS_MASK;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) */
}


@ gve_set_priv_flags @
@@
static int gve_set_priv_flags(struct net_device *netdev, u32 flags)
{
	struct gve_priv *priv = netdev_priv(netdev);
...
	int num_tx_queues;
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0))
+	u64 flag_diff;
+	int new_packet_buffer_size;
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0) */
	num_tx_queues = gve_num_tx_queues(priv);
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0))
	ori_flags = READ_ONCE(priv->ethtool_flags);
	new_flags = ori_flags;

	/* Only one priv flag exists: report-stats (BIT(0))*/
	if (flags & BIT(0))
		new_flags |= BIT(0);
	else
		new_flags &= ~(BIT(0));
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) */
+	/* If turning off header split, strict header split will be turned off too*/
+	if (gve_get_enable_header_split(priv) &&
+	    !(flags & BIT(GVE_PRIV_FLAGS_ENABLE_HEADER_SPLIT))) {
+		flags &= ~BIT(GVE_PRIV_FLAGS_ENABLE_HEADER_SPLIT);
+		flags &= ~BIT(GVE_PRIV_FLAGS_ENABLE_STRICT_HEADER_SPLIT);
+	}
+
+	/* If strict header-split is requested, turn on regular header-split */
+	if (flags & BIT(GVE_PRIV_FLAGS_ENABLE_STRICT_HEADER_SPLIT))
+		flags |= BIT(GVE_PRIV_FLAGS_ENABLE_HEADER_SPLIT);
+
+	/* Make sure header-split is available */
+	if ((flags & BIT(GVE_PRIV_FLAGS_ENABLE_HEADER_SPLIT)) &&
+	     !(priv->header_buf_size)) {
+		dev_err(&priv->pdev->dev,
+			"Header-split not available\n");
+		return -EINVAL;
+	}
+
+	if ((flags & BIT(GVE_PRIV_FLAGS_ENABLE_MAX_RX_BUFFER_SIZE)) &&
+	     priv->max_rx_buffer_size <= GVE_DEFAULT_HEADER_BUFFER_SIZE) {
+		dev_err(&priv->pdev->dev,
+			"Max-rx-buffer-size not available\n");
+		return -EINVAL;
+	}
+
+	ori_flags = READ_ONCE(priv->ethtool_flags);
+
+	new_flags = flags & GVE_PRIV_FLAGS_MASK;
+
+	flag_diff = new_flags ^ ori_flags;
+
+	if ((flag_diff & BIT(GVE_PRIV_FLAGS_ENABLE_HEADER_SPLIT)) ||
+	     (flag_diff & BIT(GVE_PRIV_FLAGS_ENABLE_MAX_RX_BUFFER_SIZE))) {
+		bool enable_hdr_split =
+			new_flags & BIT(GVE_PRIV_FLAGS_ENABLE_HEADER_SPLIT);
+		bool enable_max_buffer_size =
+			new_flags & BIT(GVE_PRIV_FLAGS_ENABLE_MAX_RX_BUFFER_SIZE);
+		int err;
+
+		if (enable_max_buffer_size)
+			new_packet_buffer_size = priv->max_rx_buffer_size;
+		else
+			new_packet_buffer_size = GVE_DEFAULT_RX_BUFFER_SIZE;
+
+		err = gve_set_buffer_size_config(priv,
+					      enable_hdr_split,
+					      new_packet_buffer_size);
+		if (err)
+			return err;
+	}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) */
	priv->ethtool_flags = new_flags;
...
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0))
+	priv->header_split_strict =
+		(priv->ethtool_flags &
+		 BIT(GVE_PRIV_FLAGS_ENABLE_STRICT_HEADER_SPLIT)) ? true : false;
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0) */
	return 0;
}


@ gve_gstrings_priv_flags @
@@
const char gve_gstrings_priv_flags[][ETH_GSTRING_LEN] = {
	"report-stats",
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0))
+	"enable-header-split", "enable-strict-header-split",
+	"enable-max-rx-buffer-size"
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0) */
};


@ gve_set_buffer_size_config @
identifier gve_tx_timeout;
@@
static void gve_tx_timeout(struct net_device *dev, unsigned int txqueue)
{
...
}
+
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0))
+int gve_set_buffer_size_config(struct gve_priv *priv, bool enable_hdr_split,
+			        int new_pkt_buf_size)
+{
+	struct gve_tx_alloc_rings_cfg tx_alloc_cfg = {0};
+	struct gve_rx_alloc_rings_cfg rx_alloc_cfg = {0};
+	struct gve_qpls_alloc_cfg qpls_alloc_cfg = {0};
+	int err = 0;
+
+	gve_get_curr_alloc_cfgs(priv, &qpls_alloc_cfg,
+				&tx_alloc_cfg, &rx_alloc_cfg);
+
+	rx_alloc_cfg.enable_header_split = enable_hdr_split;
+	rx_alloc_cfg.packet_buffer_size = new_pkt_buf_size;
+
+	if (netif_running(priv->dev)) {
+		err = gve_adjust_config(priv, &qpls_alloc_cfg,
+					&tx_alloc_cfg, &rx_alloc_cfg);
+	}
+	return err;
+}
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0) */


@ gve_set_buffer_size_config_declaration @
@@
static inline u32 gve_xdp_tx_start_queue_id(struct gve_priv *priv)
{
...
}
+
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0))
+int gve_set_buffer_size_config(struct gve_priv *priv, bool enable_hdr_split,
+			        int new_pkt_buf_size);
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0) */


@ gve_rx_dqo @
@@
int gve_rx_dqo(...)
{
...
	if (unlikely(hbo
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0))
+		     && priv->header_split_strict
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0) */
		     )) {
		gve_enqueue_buf_state(rx, &rx->dqo.recycled_buf_states,
				      buf_state);
		return -EFAULT;
	}
...
	if (sph) {
	...
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0))
+		if (!hbo) {
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0) */
			dma_sync_single_for_cpu(&priv->pdev->dev,
						buf_state->hdr_buf->addr,
						hdr_len, DMA_FROM_DEVICE);

			rx->ctx.skb_head = gve_rx_copy_data(priv->dev, napi,
							buf_state->hdr_buf->data,
							hdr_len);
			if (unlikely(!rx->ctx.skb_head))
				goto error;

			rx->ctx.skb_tail = rx->ctx.skb_head;
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0))
+		}
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0) */
	...
	}
...
}
