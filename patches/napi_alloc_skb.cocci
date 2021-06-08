@ fix_napi_skb_alloc exists @
identifier netdev, napi_ptr, skb, len;
identifier func;
@@
func(struct net_device *netdev, struct napi_struct *napi_ptr, ...)
{
...
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
struct sk_buff *skb = napi_alloc_skb(napi_ptr, len);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) */
+struct sk_buff *skb = netdev_alloc_skb_ip_align(netdev, len);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) */
...
}
