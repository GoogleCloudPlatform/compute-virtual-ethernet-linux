@ assigned @
identifier get_stats, ethtool_struct;
@@

const struct ethtool_ops ethtool_struct = {
	.get_ethtool_stats = get_stats,
};

@ memset depends on assigned @
identifier assigned.get_stats, netdev, stats, data;
@@

get_stats(struct net_device *netdev, struct ethtool_stats *stats, u64 *data)
{
...
	ASSERT_RTNL();
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0))
+	memset(data, 0, stats->n_stats * sizeof(*data));
+#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0)) */
...
}
