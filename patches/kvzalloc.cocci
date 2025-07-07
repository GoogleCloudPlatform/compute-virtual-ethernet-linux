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
expression var, count, size;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,18,0)
var = vcalloc(count, size);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,18,0) */
+var = vzalloc(count * size);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,18,0) */

@@
expression var;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
kvfree(var);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0) */
+kfree(var);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0) */


@@
expression var, num, size, flags, node;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)
var = kvcalloc_node(num, size, flags, node);
+#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
+var = kvzalloc_node((num) * (size), flags, node);
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,12,0) */
+var = kzalloc_node((num) * (size), flags, node);
+#endif
