@ assigned @
identifier change_mtu, ndo_struct;
@@

struct net_device_ops ndo_struct = {
+#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 5)
+	.ndo_change_mtu_rh74	=	change_mtu,
+#else /* RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7, 5) */
	.ndo_change_mtu		=	change_mtu,
+#endif /* RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 5) */
};

@ check depends on assigned @
identifier assigned.change_mtu, dev, new_mtu;
@@

change_mtu(struct net_device *dev, int new_mtu)
{
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0))
+	struct gve_priv *priv = netdev_priv(dev);
+
+	if (new_mtu < ETH_MIN_MTU || new_mtu > priv->max_mtu)
+		return -EINVAL;
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */
	...
}

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
identifier max;
struct gve_priv *priv;
@@

+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0))
+priv->max_mtu = max;
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */
priv->dev->max_mtu = max;
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */

@ swap2 @
identifier max;
struct gve_priv *priv;
@@

+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0))
+if (priv->max_mtu > max)
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */
if (priv->dev->max_mtu > max)
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
expression val;
struct net_device *dev;
@@

+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0))
+struct gve_priv *priv = netdev_priv(dev);
+
+return priv->max_mtu <= val;
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */
return  dev->max_mtu <= val;
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */
