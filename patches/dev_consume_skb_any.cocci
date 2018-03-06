@@
expression skb;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
dev_consume_skb_any(skb);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
+dev_kfree_skb_any(skb);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
