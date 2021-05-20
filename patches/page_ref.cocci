@ fix_page_ref_add exists @
expression p, v;
@@
...
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)
page_ref_add(p, v);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0) */
+atomic_add(v, &p->_count);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0) */
...

@ fix_page_ref_sub exists @
expression p, v;
@@
...
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)
page_ref_sub(p, v);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0) */
+atomic_sub(v, &p->_count);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0) */
...
