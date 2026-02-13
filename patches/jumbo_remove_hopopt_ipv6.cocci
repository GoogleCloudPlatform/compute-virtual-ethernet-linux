@@
@@

 static int gve_try_tx_skb(...)
 {

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 0) && LINUX_VERSION_CODE <= KERNEL_VERSION(6,19,0)
+       if (skb_is_gso(skb) && unlikely(ipv6_hopopt_jumbo_remove(skb)))
+           goto drop;
+#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(6,19,0) */

  ...
 }

