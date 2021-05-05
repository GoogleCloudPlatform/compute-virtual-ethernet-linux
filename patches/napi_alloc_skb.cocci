@ fix_napi_skb_alloc exists @
identifier napi, skb, len, sk_buff;
@@

...
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
struct sk_buff *skb = napi_alloc_skb(napi, len);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) */
+struct sk_buff *skb = netdev_alloc_skb_ip_align(netdev, len);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 3) */
