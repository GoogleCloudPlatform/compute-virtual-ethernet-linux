@@
expression alloc_ptr, alloc_func;
expression list args;
expression a, b;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
alloc_ptr = alloc_func(array_size(a, b), args);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0) */
+alloc_ptr = alloc_func((a * b), args);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0) */
