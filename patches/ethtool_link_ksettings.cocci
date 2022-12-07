@ remove_field @
identifier gve_ops, get_link_ksettings_func;
@@

const struct ethtool_ops gve_ops = {
	.set_priv_flags = gve_set_priv_flags,
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 9)
	.get_link_ksettings = get_link_ksettings_func
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 9) */
};


@ function depends on remove_field @
identifier remove_field.get_link_ksettings_func, net_device, ethtool_link_ksettings, netdev, cmd;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 9)
static int get_link_ksettings_func(struct net_device *netdev,
				       struct ethtool_link_ksettings *cmd)
{
...
}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 9) */
