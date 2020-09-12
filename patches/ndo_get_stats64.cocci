@ assigned @
identifier get_stats, ndo_struct;
@@

struct net_device_ops ndo_struct = {
	.ndo_get_stats64	=	get_stats,
};

@ declared depends on assigned @
identifier dev, stats;
identifier assigned.get_stats;
fresh identifier backport = "backport_" ## get_stats;
@@

void get_stats(struct net_device *dev, struct rtnl_link_stats64 *stats)
{
	...
}

+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0)) && (RHEL_RELEASE_CODE <= RHEL_RELEASE_VERSION(7,6))
+static struct rtnl_link_stats64 *
+backport(struct net_device *dev, struct rtnl_link_stats64 *stats)
+{
+	get_stats(dev, s);
+	return s;
+}
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,11.0) */

@ mod_assignment depends on assigned @
identifier assigned.ndo_struct;
identifier assigned.get_stats;
fresh identifier backport = "backport_" ## get_stats;
@@


struct net_device_ops ndo_struct = {
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0)) && (RHEL_RELEASE_CODE <= RHEL_RELEASE_VERSION(7,6))
+	.ndo_get_stats64	=	backport,
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,11.0) */
	.ndo_get_stats64	=	get_stats,
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,11.0) */
};

