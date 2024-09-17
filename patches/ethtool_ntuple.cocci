@@
@@
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,17,0))
+#define FLOW_RSS 0
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)) */
static int gve_generate_flow_rule(struct gve_priv *priv, struct ethtool_rx_flow_spec *fsp,
				  struct gve_adminq_flow_rule *rule)
{
	...
}

@@
expression flow_type;
@@

	switch (flow_type) {
	case TCP_V6_FLOW:
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0))
		...
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)) */
	case UDP_V6_FLOW:
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0))
		...
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)) */
	case SCTP_V6_FLOW:
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0))
		...
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)) */
	case AH_V6_FLOW:
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0))
		...
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)) */
	case ESP_V6_FLOW:
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0))
		...
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)) */
	default:
		...
	}

@@
@@
static int gve_set_channels(struct net_device *netdev,
			    struct ethtool_channels *cmd)
{
...
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,2,0))
+	if (old_settings.rx_count != new_rx && priv->num_flow_rules) {
+		dev_err(&priv->pdev->dev,
+			"Changing number of RX queues is disabled when flow rules are active");
+		return -EBUSY;
+	}
+#endif /* (LINUX_VERSION_CODE< KERNEL_VERSION(6,2,0)) */
+
	if (!netif_running(netdev)) {
		priv->tx_cfg.num_queues = new_tx;
		priv->rx_cfg.num_queues = new_rx;
		return 0;
	}

	new_tx_cfg.num_queues = new_tx;
	new_rx_cfg.num_queues = new_rx;

	return gve_adjust_queues(priv, new_rx_cfg, new_tx_cfg);
}
