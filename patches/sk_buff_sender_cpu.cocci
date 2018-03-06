@@
struct sk_buff *skb;
identifier e;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0)
e = skb->sender_cpu - ...;
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0) */
+e = raw_smp_processor_id();
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0) */
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0)
e = skb->sender_cpu + ...;
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0) */
+e = raw_smp_processor_id();
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0) */
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0)
e = skb->sender_cpu;
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0) */
+e = raw_smp_processor_id();
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0) */
)
