@@
identifier skb, func, dev;
expression check, ret;
@@

func(struct sk_buff *skb, struct net_device *dev)
{
...

+ /* If we have xmit_more - don't ring the doorbell unless we are stopped */
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
 if (check 
+#if LINUX_VERSION_CODE > KERNEL_VERSION(5,2,0)
  && netdev_xmit_more()
+#else /* LINUX_VERSION_CODE > KERNEL_VERSION(5,2,0) */
+ && skb->xmit_more
+#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(5,2,0) */
 )
 return ret;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0) */
 ...}
