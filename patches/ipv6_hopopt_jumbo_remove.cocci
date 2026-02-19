@@
expression priv;
@@

+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,2,1))
if (!gve_is_gqi(priv))
	netif_set_tso_max_size(priv->dev, GVE_DQO_TX_MAX);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,2,1) */

@@
struct gve_tx_ring *tx;
@@

static int gve_try_tx_skb(..., struct sk_buff *skb)
{
	...
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,2,1) && LINUX_VERSION_CODE < KERNEL_VERSION(7,0,0))
+	if (skb_is_gso(skb) && unlikely(ipv6_hopopt_jumbo_remove(skb)))
+		goto drop;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,2,1) && LINUX_VERSION_CODE < KERNEL_VERSION(7,0,0) */
	if (tx->dqo.qpl) {
		...
	} else {
		...
	}
	...
}
