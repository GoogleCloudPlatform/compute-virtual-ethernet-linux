@@
expression napi;
@@

+#if LINUX_VERSION_CODE < KERNEL_VERSION(4,5,0) && LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0)
+napi_hash_del(napi);
+synchronize_net();
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,5,0) && LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0) */
netif_napi_del(napi);
