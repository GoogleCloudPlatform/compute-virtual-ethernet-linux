@repick@
identifier var, priv, skb;
@@
+#if LINUX_VERSION_CODE < KERNEL_VERSION(4,16,0)
+	/* Workaround for bug in older kernels */
+	if (skb_get_queue_mapping(skb) >= priv->tx_cfg.num_queues) {
+		return -EBUSY;
+	}
+
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,16,0) */
var = &priv->tx[skb_get_queue_mapping(skb)];


