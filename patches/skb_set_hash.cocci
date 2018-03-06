@ fix_use @
identifier func;
expression skb, hash, flag;
@@

+#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 0) || LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
skb_set_hash(skb, hash, func(flag));
+#else /* RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7, 0) && LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0) */
+skb->rxhash = hash;
+skb->l4_rxhash = !!(flag & (GVE_RXF_TCP | GVE_RXF_UDP));
+#endif /* RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 0) || LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */

@ fix_delcare depends on fix_use @
identifier fix_use.func;
@@
+#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 0) || LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
func(...)
{...}
+#endif /* RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 0) || LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
