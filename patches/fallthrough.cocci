@@
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,4,0)
fallthrough;
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,4,0) */
+/* fallthrough */
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,4,0) */
