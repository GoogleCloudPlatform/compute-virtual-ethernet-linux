@@
identifier ret;
expression param, func;
type T;
@@

(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
T ret = func(smp_load_acquire(param));
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
+T ret = func(*param);
+smp_rmb();
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
ret = smp_load_acquire(param);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
+ret = *param;
+smp_rmb();
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
ret = func(smp_load_acquire(param));
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
+ret = func(*param);
+smp_rmb();
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
)
