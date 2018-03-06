@@
struct sk_buff *skb;
expression check, stuff;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
if (!skb->xmit_more || check)
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0) */
+if (1)
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0) */
(
{...}
|
stuff;
)

