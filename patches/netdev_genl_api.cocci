@@
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0)
netif_napi_set_irq(...);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) */

@@
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0)
netif_queue_set_napi(...);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) */