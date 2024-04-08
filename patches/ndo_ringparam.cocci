@@
parameter P1, P2, P3, P4;
@@

static void gve_get_ringparam( P1, P2
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,16,0)
 ,P3, P4
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,16,0) */
 )
{
...
}
