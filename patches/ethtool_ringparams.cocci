@ gve_get_ringparam @
identifier gve_get_ringparam, netdev, cmd;
@@

static void gve_get_ringparam(struct net_device *netdev,
                               struct ethtool_ringparam *cmd
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 7)
                              , struct kernel_ethtool_ringparam *kernel_cmd,
                              struct netlink_ext_ack *extack
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 7) */
			    )
{
...
}

@ gve_set_ringparam @
identifier gve_set_ringparam, netdev, cmd;
@@

static int gve_set_ringparam(struct net_device *netdev,
                               struct ethtool_ringparam *cmd
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 7)
                              , struct kernel_ethtool_ringparam *kernel_cmd,
                              struct netlink_ext_ack *extack
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 7) */
			    )
{
...
}
