@ find_netdev @
identifier func, netdev;
@@
func(...) {... struct net_device *netdev = ...; ...}

@ fix_call depends on find_netdev @
identifier skb, len, find_netdev.netdev;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
skb = napi_alloc_skb(..., len);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) */
+skb = netdev_alloc_skb_ip_align(netdev, len);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) */

