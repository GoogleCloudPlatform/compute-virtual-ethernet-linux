@@
expression pre_jiffies, arg;
expression jiffies;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0)
jiffies = pre_jiffies + secs_to_jiffies(arg);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
+jiffies = pre_jiffies + msecs_to_jiffies(arg * MSEC_PER_SEC);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */



