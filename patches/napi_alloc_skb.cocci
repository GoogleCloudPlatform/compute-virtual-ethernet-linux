@ fix_napi_skb_alloc exists @
identifier netdev, napi, skb, len;
@@
struct net_device *netdev = ...;
...
-skb = napi_alloc_skb(napi, len);
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
+skb = napi_alloc_skb(napi, len);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) */
+skb = netdev_alloc_skb_ip_align(netdev, len);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) */

