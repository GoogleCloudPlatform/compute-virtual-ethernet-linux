@ assigned @
identifier get_ringparam_func, ethtool_ops_obj;
@@

struct ethtool_ops ethtool_ops_obj = {
        .get_ringparam = get_ringparam_func,
};

@ declared depends on assigned @
identifier dev_param, cmd_param, kernel_cmd_param, extack_param;
identifier assigned.get_ringparam_func;
fresh identifier backport_get_ringparam = "backport_" ## get_ringparam_func;
@@

+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,16,0))
static void get_ringparam_func(struct net_device *dev_param,
                                   struct ethtool_ringparam *cmd_param,
                                   struct kernel_ethtool_ringparam *kernel_cmd_param,
                                   struct netlink_ext_ack *extack_param)
{
        ...
}
+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,16,0)) */
+static void
+backport_get_ringparam(struct net_device *dev_param,
+                             struct ethtool_ringparam *cmd_param)
+{
+       struct gve_priv *priv = netdev_priv(netdev);
+
+       cmd->rx_max_pending = priv->rx_desc_cnt;
+       cmd->tx_max_pending = priv->tx_desc_cnt;
+       cmd->rx_pending = priv->rx_desc_cnt;
+       cmd->tx_pending = priv->tx_desc_cnt;
+}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,16,0)) */

@ mod_assignment @
identifier assigned.get_ringparam_func;
identifier assigned.ethtool_ops_obj;
fresh identifier backport_get_ringparam = "backport_" ## get_ringparam_func;
@@

struct ethtool_ops ethtool_ops_obj = {
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,16,0))
        .get_ringparam  = get_ringparam_func,
+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,16,0)) */
+       .get_ringparam = backport_get_ringparam,
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,16,0)) */
};
