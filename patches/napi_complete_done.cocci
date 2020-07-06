@@
expression napi, work;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
if (likely(napi_complete_done(napi, work)))
+#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
+	napi_complete_done(napi, work);
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0) */
+	napi_complete(napi);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0) */
{
...
}
