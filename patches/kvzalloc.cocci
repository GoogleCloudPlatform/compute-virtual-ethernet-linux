@@
expression var, size, flags;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
var = kvzalloc(size, flags);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0) */
+var = kcalloc(1, size, flags);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0) */

@@
expression var, count, size, flags;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
var = kvcalloc(count, size, flags);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0) */
+var = kcalloc(count, size, flags);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0) */

@@
expression var;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
kvfree(var);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0) */
+kfree(var);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0) */
