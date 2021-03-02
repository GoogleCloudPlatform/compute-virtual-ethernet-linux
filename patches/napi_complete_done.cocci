@@
expression napi, work;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 5)
if (likely(napi_complete_done(napi, work)))
+#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 3)
+	napi_complete_done(napi, work);
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0) || RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7, 3) */
+	napi_complete(napi);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 5) */
{
...
}
