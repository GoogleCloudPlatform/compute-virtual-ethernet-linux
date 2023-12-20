@@
expression skb, is_napi;
@@

+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0))
napi_consume_skb(skb, is_napi);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0) */
+dev_consume_skb_any(skb);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0) */

@@
expression skb;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
dev_consume_skb_any(skb);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
+dev_kfree_skb_any(skb);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
