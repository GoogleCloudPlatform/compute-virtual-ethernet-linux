@@
identifier ret;
expression arg;
expression func;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,20,0)
ret = func(READ_ONCE(arg));
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3,20,0) */
+ret = func(ACCESS_ONCE(arg));
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,20,0) */

@@
identifier ret;
expression arg;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,20,0)
ret = READ_ONCE(arg);
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3,20,0) */
+ret = ACCESS_ONCE(arg);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,20,0) */

@@
expression arg;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,20,0)
return READ_ONCE(arg);
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3,20,0) */
+return ACCESS_ONCE(arg);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,20,0) */
