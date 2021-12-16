@ get_assigned @
identifier get_coalesce_func, ethtool_ops_obj;
@@

struct ethtool_ops ethtool_ops_obj = {
	.get_coalesce	=	get_coalesce_func,
};

@ get_declared depends on get_assigned @
identifier dev, ec, kernel_ec, extack;
identifier get_assigned.get_coalesce_func;
fresh identifier backport_get = "backport_" ## get_coalesce_func;
@@

+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0))
static int get_coalesce_func(struct net_device *dev,
			    struct ethtool_coalesce *ec,
			    struct kernel_ethtool_coalesce *kernel_ec,
			    struct netlink_ext_ack *extack)
{
        ...
}

+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)) */
+static int
+backport_get(struct net_device *dev1,
+	      struct ethtool_coalesce *ec1)
+{
+       struct gve_priv *priv = netdev_priv(dev1);
+
+	if (gve_is_gqi(priv))
+		return -EOPNOTSUPP;
+	ec1->tx_coalesce_usecs = priv->tx_coalesce_usecs;
+	ec1->rx_coalesce_usecs = priv->rx_coalesce_usecs;
+
+       return 0;
+}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)) */

@ get_mod_assignment depends on get_assigned @
identifier get_assigned.ethtool_ops_obj;
identifier get_assigned.get_coalesce_func;
fresh identifier backport_get = "backport_" ## get_coalesce_func;
@@

struct ethtool_ops ethtool_ops_obj = {
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0))
	.get_coalesce	=	get_coalesce_func,
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0) */
+	.get_coalesce   =       backport_get,
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)) */
};

@ set_assigned @
identifier set_coalesce_func, ethtool_ops_obj;
@@

struct ethtool_ops ethtool_ops_obj = {
	.set_coalesce	=	set_coalesce_func,
};

@ set_declared depends on set_assigned @
identifier dev, ec, kernel_ec, extack;
identifier set_assigned.set_coalesce_func;
fresh identifier backport_set = "backport_" ## set_coalesce_func;
@@

+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0))
static int set_coalesce_func(struct net_device *dev,
			    struct ethtool_coalesce *ec,
			    struct kernel_ethtool_coalesce *kernel_ec,
			    struct netlink_ext_ack *extack)
{
        ...
}

+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)) */
+static int
+backport_set(struct net_device *dev,
+	      struct ethtool_coalesce *ec)
+{
+	struct gve_priv *priv = netdev_priv(netdev);
+	u32 tx_usecs_orig = priv->tx_coalesce_usecs;
+	u32 rx_usecs_orig = priv->rx_coalesce_usecs;
+	int idx;
+
+	if (gve_is_gqi(priv))
+		return -EOPNOTSUPP;
+
+	if (ec->tx_coalesce_usecs > GVE_MAX_ITR_INTERVAL_DQO ||
+	    ec->rx_coalesce_usecs > GVE_MAX_ITR_INTERVAL_DQO)
+		return -EINVAL;
+	priv->tx_coalesce_usecs = ec->tx_coalesce_usecs;
+	priv->rx_coalesce_usecs = ec->rx_coalesce_usecs;
+
+	if (tx_usecs_orig != priv->tx_coalesce_usecs) {
+		for (idx = 0; idx < priv->tx_cfg.num_queues; idx++) {
+			int ntfy_idx = gve_tx_idx_to_ntfy(priv, idx);
+			struct gve_notify_block *block = &priv->ntfy_blocks[ntfy_idx];
+
+			gve_set_itr_coalesce_usecs_dqo(priv, block,
+						       priv->tx_coalesce_usecs);
+		}
+	}
+
+	if (rx_usecs_orig != priv->rx_coalesce_usecs) {
+		for (idx = 0; idx < priv->rx_cfg.num_queues; idx++) {
+			int ntfy_idx = gve_rx_idx_to_ntfy(priv, idx);
+			struct gve_notify_block *block = &priv->ntfy_blocks[ntfy_idx];
+
+			gve_set_itr_coalesce_usecs_dqo(priv, block,
+						       priv->rx_coalesce_usecs);
+		}
+	}
+
+	return 0;
+}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)) */

@ set_mod_assignment depends on set_assigned @
identifier set_assigned.ethtool_ops_obj;
identifier set_assigned.set_coalesce_func;
fresh identifier backport_set = "backport_" ## set_coalesce_func;
@@

struct ethtool_ops ethtool_ops_obj = {
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0))
	.set_coalesce	=	set_coalesce_func,
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0) */
+	.set_coalesce   =       backport_set,
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)) */
};

@ remove_field @
identifier gve_ethtool_ops, ETHTOOL_COALESCE_USECS;
@@

const struct ethtool_ops gve_ethtool_ops = {
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)
        .supported_coalesce_params = ETHTOOL_COALESCE_USECS,
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0) */
...
};

