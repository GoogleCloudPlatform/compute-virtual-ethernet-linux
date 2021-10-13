@@
expression dev, mac;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)
eth_hw_addr_set(dev, mac);
+#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
+ether_addr_copy(dev->dev_addr, mac);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
+memcpy(dev->dev_addr, mac, ETH_ALEN);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
