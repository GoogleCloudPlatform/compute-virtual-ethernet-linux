@ assigned @
identifier tx_timeout, ndo_struct;
@@

struct net_device_ops ndo_struct = {
	.ndo_tx_timeout	=	tx_timeout,
};

@ declared depends on assigned @
identifier dev, queue;
identifier assigned.tx_timeout;
fresh identifier backport = "backport_" ## tx_timeout;
@@

+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 3) || UBUNTU_VERSION_CODE >= UBUNTU_VERSION(5,4,0,1102))
static void tx_timeout(struct net_device *dev, unsigned int queue)
{
	...
}

+#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 3) || UBUNTU_VERSION_CODE >= UBUNTU_VERSION(5,4,0,1102)) */
+static void
+backport(struct net_device *dev)
+{
+	struct gve_priv *priv = netdev_priv(dev);
+	gve_schedule_reset(priv);
+	priv->tx_timeo_cnt++;
+}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 3) || UBUNTU_VERSION_CODE >= UBUNTU_VERSION(5,4,0,1102)) */

@ mod_assignment depends on assigned @
identifier assigned.ndo_struct;
identifier assigned.tx_timeout;
fresh identifier backport = "backport_" ## tx_timeout;
@@


struct net_device_ops ndo_struct = {
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 3) || UBUNTU_VERSION_CODE >= UBUNTU_VERSION(5,4,0,1102))
	.ndo_tx_timeout	=	tx_timeout,
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 3) || UBUNTU_VERSION_CODE >= UBUNTU_VERSION(5,4,0,1102) */
+	.ndo_tx_timeout =	backport,
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 3) || UBUNTU_VERSION_CODE >= UBUNTU_VERSION(5,4,0,1102)) */
};

