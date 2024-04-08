@ remove_field @
identifier gve_ethtool_ops, ETHTOOL_COALESCE_USECS;
@@

const struct ethtool_ops gve_ethtool_ops = {
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)
	.supported_coalesce_params = ETHTOOL_COALESCE_USECS,
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0) */
...
};
