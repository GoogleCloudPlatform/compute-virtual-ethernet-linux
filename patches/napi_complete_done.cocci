@@
expression napi;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
napi_complete_done(napi, ...);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) */
+napi_complete(napi);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) */
