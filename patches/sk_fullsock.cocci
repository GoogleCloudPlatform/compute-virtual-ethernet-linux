@@
struct sk_buff *skb;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0)
sk_fullsock(skb->sk)
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0) */
+(skb->sk->sk_state & TCP_TIME_WAIT)
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0) */
