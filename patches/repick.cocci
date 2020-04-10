@repick@
identifier var, priv, skb;
@@
+#if LINUX_VERSION_CODE < KERNEL_VERSION(4,16,0)
+	/* Workaround for bug in older kernels */
+	if (skb_get_queue_mapping(skb) >= priv->tx_cfg.num_queues) {
+		skb_set_queue_mapping(skb, skb_get_queue_mapping(skb) % priv->tx_cfg.num_queues);
+	}
+
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,16,0) */
var = &priv->tx[skb_get_queue_mapping(skb)];


