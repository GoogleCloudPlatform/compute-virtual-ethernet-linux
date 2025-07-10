@ assigned @
identifier ndo_struct, get_ts_config, set_ts_config;
@@

struct net_device_ops ndo_struct = {
	.ndo_hwtstamp_get	=	get_ts_config,
	.ndo_hwtstamp_set	=	set_ts_config,
};

@ declared_get depends on assigned @
identifier assigned.get_ts_config;
fresh identifier backport = "backport_" ## get_ts_config;
@@

static int gve_get_ts_config(...)
{
        ...
}

+#if LINUX_VERSION_CODE < KERNEL_VERSION(6,6,0)
+static int backport(struct net_device *dev, struct ifreq *ifr)
+ {
+		 struct gve_priv *priv = netdev_priv(dev);
+        struct hwtstamp_config cfg;
+		 memcpy(&cfg, &priv->ts_config, sizeof(cfg));
+        if (copy_to_user(ifr->ifr_data, &cfg, sizeof(cfg)))
+                return -EFAULT;
+
+        return 0;
+ }
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6,6,0) */

@ gve_get_ts_config @
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,6,0)
static int gve_get_ts_config(...)
{
        ...
}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,6,0) */

@ declared_set depends on assigned @
identifier dev, kernel_config, extack;
identifier assigned.set_ts_config;
identifier assigned.get_ts_config;
fresh identifier backport_set = "backport_" ## set_ts_config;
fresh identifier backport_get = "backport_" ## get_ts_config;
@@

static int gve_set_ts_config(struct net_device *dev,
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,6,0)
			     struct kernel_hwtstamp_config *kernel_config,
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(6,6,0) */
+			     struct hwtstamp_config *kernel_config,
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6,6,0) */
			     struct netlink_ext_ack *extack)
{
        ...
}

+#if LINUX_VERSION_CODE < KERNEL_VERSION(6,6,0)
+static int backport_set(struct net_device *dev, struct ifreq *ifr)
+{
+        struct hwtstamp_config cfg;
+        int ret;
+        if (copy_from_user(&cfg, ifr->ifr_data, sizeof(cfg)))
+                return -EFAULT;
+
+        ret = gve_set_ts_config(dev, &cfg, NULL);
+        if (ret)
+                return ret;
+
+        if (copy_to_user(ifr->ifr_data, &cfg, sizeof(cfg)))
+                return -EFAULT;
+
+        return ret;
+}

+static int gve_eth_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
+{
+        switch(cmd) {
+        case SIOCSHWTSTAMP:
+                return backport_set(dev, ifr);
+        case SIOCGHWTSTAMP:
+                return backport_get(dev, ifr);
+        default:
+                return -EINVAL;
+        }
+}
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6,6,0) */


@ mod_assignment depends on assigned @
identifier assigned.ndo_struct;
@@

struct net_device_ops ndo_struct = {
+#if LINUX_VERSION_CODE < KERNEL_VERSION(5,15,0) && LINUX_VERSION_CODE >= KERNEL_VERSION(5,9,0)
+	.ndo_do_ioctl		=	gve_eth_ioctl,
+#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0) && LINUX_VERSION_CODE < KERNEL_VERSION(6,6,0)
+	.ndo_eth_ioctl		=	gve_eth_ioctl,
+#elif LINUX_VERSION_CODE >= KERNEL_VERSION(6,6,0)
	.ndo_hwtstamp_get	=	gve_get_ts_config,
	.ndo_hwtstamp_set	=	gve_set_ts_config,
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,6,0) */
};

@ gve_header @
identifier ts_config;
@@

struct gve_priv {
...
+#if LINUX_VERSION_CODE < KERNEL_VERSION(6,6,0)
+	struct hwtstamp_config ts_config;
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,6,0) */
	struct kernel_hwtstamp_config ts_config;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,6,0) */
...
};

@@
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
NL_SET_ERR_MSG_MOD(...);
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,12,0) */
