@@
expression sum, diff;
@@

+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0))
csum_replace_by_diff(&sum, diff);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0) */
+sum = csum_fold(csum_add(diff, ~csum_unfold(sum)));
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0) */
