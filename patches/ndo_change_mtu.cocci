@ assigned @
identifier gve_netdev_ops;
@@

+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0))
+int gve_change_mtu(struct net_device *dev, int new_mtu)
+{
+	struct gve_priv *priv = netdev_priv(dev);
+
+	if (new_mtu < ETH_MIN_MTU || new_mtu > priv->max_mtu)
+		return -EINVAL;
+	dev->mtu = new_mtu;
+	return 0;
+}
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)) */
+
struct net_device_ops gve_netdev_ops = {
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0))
+#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 5) && RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(8, 0)
+	.ndo_change_mtu_rh74	=	gve_change_mtu,
+#else /* RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7, 5) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 0) */
+	.ndo_change_mtu		=	gve_change_mtu,
+#endif /* RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 5) && RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(8, 0) */
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)) */
	...
};

@ block @
expression val;
struct net_device *dev;
@@

+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0))
dev->min_mtu = val;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0) */

@ add @
@@

struct gve_priv {
...
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0))
+	int max_mtu;
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */
}

@ add2 @
expression max;
@@

#define GVE_TX_MAX_IOVEC max
+#ifndef ETH_MIN_MTU
+#define ETH_MIN_MTU	68 /* Min IPv4 MTU per RFC791	*/
+#endif

@ swap @
expression max;
struct gve_priv *priv;
@@

+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0))
+priv->max_mtu = max;
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */
priv->dev->max_mtu = max;
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */

@ swap2 @
expression max, e1;
struct gve_priv *priv;
@@

+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0))
+if (e1 && priv->max_mtu > max)
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */
if (e1 && priv->dev->max_mtu > max)
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */
{
...
}

@ swap3 @
struct gve_priv *priv;
@@

+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0))
+priv->dev->mtu = priv->max_mtu;
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */
priv->dev->mtu = priv->dev->max_mtu;
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */

@ swap4 @
expression val, ret;
struct net_device *dev;
@@

+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0))
+if (((struct gve_priv *)netdev_priv(dev))->max_mtu > val)
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */
if (dev->max_mtu > val)
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */
return ret;

@ swap5 @
struct net_device *dev;
expression max;
@@

+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0))
+return ((struct gve_priv *)netdev_priv(dev))->max_mtu <= max;
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */
return dev->max_mtu <= max;
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */

@ swap6 @
struct gve_priv *priv;
expression e1;
@@

+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0))
+e1 = priv->max_mtu;
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */
e1 = priv->dev->max_mtu;
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */
