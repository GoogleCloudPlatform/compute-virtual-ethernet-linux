@@
expression napi;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
napi_schedule_irqoff(napi);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) */
+napi_schedule(napi);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) */

