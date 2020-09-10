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

static void tx_timeout(struct net_device *dev, unsigned int queue)
{
	...
}

+#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0))
+static void
+backport(struct net_device *dev)
+{
+	tx_timeout(dev, 0);
+}
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0) */

@ mod_assignment depends on assigned @
identifier assigned.ndo_struct;
identifier assigned.tx_timeout;
fresh identifier backport = "backport_" ## tx_timeout;
@@


struct net_device_ops ndo_struct = {
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0))
+	.ndo_tx_timeout	=	backport,
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0) */
	.ndo_tx_timeout	=	tx_timeout,
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0) */
};

