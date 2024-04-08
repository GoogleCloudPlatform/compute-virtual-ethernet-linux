@@
parameter P1, P2, P3, P4;
@@

static int gve_get_coalesce( P1, P2
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)
 ,P3, P4
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0) */
 )
{
...
}

@@
parameter P1, P2, P3, P4;
@@
static int gve_set_coalesce( P1, P2
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)
 ,P3, P4
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0) */
 )
{
...
}
