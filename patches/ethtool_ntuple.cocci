@@
@@
#include <linux/ethtool.h>
+#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,17,0))
+#define FLOW_RSS 0
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)) */
@@
expression flow_type;
@@
	switch (flow_type) {
	case TCP_V6_FLOW:
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0))
		...
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)) */
	case UDP_V6_FLOW:
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0))
		...
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)) */
	case SCTP_V6_FLOW:
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0))
		...
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)) */
	case AH_V6_FLOW:
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0))
		...
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)) */
	case ESP_V6_FLOW:
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0))
		...
+#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)) */
	default:
		...
	}
