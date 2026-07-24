@@
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2)
#include <net/netdev_lock.h>
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2) */
+#include <linux/rtnetlink.h>
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2) */

@@
expression netdev;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2)
	netdev_assert_locked(netdev);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2) */
+	ASSERT_RTNL();
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2) */

@@
expression netdev;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2)
	netdev_lock(netdev);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2) */

@@
expression netdev;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0)|| RHEL_VERSION_GTE(10,2)
	netdev_unlock(netdev);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2) */


@@
expression dev, napi, func;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0)|| RHEL_VERSION_GTE(10,2)
netif_napi_add_locked(dev, napi, func);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2) */
+netif_napi_add(dev, napi, func);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2) */

@@
expression napi, irq;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2)
netif_napi_set_irq_locked(napi, irq);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2) */
+netif_napi_set_irq(napi, irq);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2) */

@@
expression napi;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2)
netif_napi_del_locked(napi);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2) */
+netif_napi_del(napi);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2) */

@@
expression napi;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2)
		napi_disable_locked(napi);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2) */
+		napi_disable(napi);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2) */

@@
expression napi;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2)
		napi_enable_locked(napi);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2) */
+		napi_enable(napi);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) || RHEL_VERSION_GTE(10,2) */

