@ assigned @
identifier change_mtu, ndo_struct;
@@

struct net_device_ops ndo_struct = {
+#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 5) && RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7, 6)
+	.ndo_change_mtu_rh74	=	change_mtu,
+#else /* RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7, 5) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 6) */
	.ndo_change_mtu		=	change_mtu,
+#endif /* RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 5) && RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7, 6) */
};

