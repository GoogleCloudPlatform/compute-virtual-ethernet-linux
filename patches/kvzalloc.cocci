@@
expression var, param1, param2, param3;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
var = kvzalloc(param1 * param2, param3);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0) */
+var = kcalloc(param1, param2, param3);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0) */
