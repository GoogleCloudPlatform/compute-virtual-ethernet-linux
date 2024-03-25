@@
expression  reschedule,mask,doorbell;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0)
		if (reschedule && napi_schedule(napi))
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(6,7,0) */
+       if (reschedule && napi_reschedule(napi))
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,7,0) */
			iowrite32be(mask, doorbell);
