@ remove_field @
identifier gve_ethtool_ops;
expression E;
@@

const struct ethtool_ops gve_ethtool_ops = {
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,2,0)
.op_needs_rtnl = E,
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(7,2,0) */
...
};
