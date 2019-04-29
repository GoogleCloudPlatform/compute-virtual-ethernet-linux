@@
identifier skb, func;
expression check;
@@

func(struct sk_buff *skb, struct net_device *dev)
{
...

 if (check 
+#if LINUX_VERSION_CODE > KERNEL_VERSION(5,2,0)
  || !netdev_xmit_more()
+#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
+ || !skb->xmit_more
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3,18,0) */
 ) {...}
 ...}
