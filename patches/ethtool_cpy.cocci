@ethtool_copy_rename@
expression list args;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,16,0)
ethtool_cpy(args);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,16,0) */
+ethtool_puts(args);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,16,0) */

@@
initializer value;
identifier strarr, strlen;
attribute name __nonstring_array;
type C;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,16,0)
static const char strarr[][strlen] __nonstring_array = value;
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,16,0) */
+static const char strarr[][strlen] = value;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,16,0) */
