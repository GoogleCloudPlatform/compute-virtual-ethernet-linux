@@
@@
static int gve_resume(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct gve_priv *priv = netdev_priv(netdev);
	int err;

	priv->resume_cnt++;
	rtnl_lock();
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0)
	netdev_lock(netdev);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
	err = gve_reset_recovery(priv, priv->up_before_suspend);
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0)
	netdev_unlock(netdev);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
	rtnl_unlock();
	return err;
}

@@
@@
static int gve_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct gve_priv *priv = netdev_priv(netdev);
	bool was_up = netif_running(priv->dev);

	priv->suspend_cnt++;
	rtnl_lock();
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0)
	netdev_lock(netdev);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
	if (was_up && gve_close(priv->dev)) {
		/* If the dev was up, attempt to close, if close fails, reset */
		gve_reset_and_teardown(priv, was_up);
	} else {
		/* If the dev wasn't up or close worked, finish tearing down */
		gve_teardown_priv_resources(priv);
	}
	priv->up_before_suspend = was_up;
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0)
	netdev_unlock(netdev);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
	rtnl_unlock();
	return 0;
}

@@
expression dev, napi, func;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0)
netif_napi_add_locked(dev, napi, func);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
+netif_napi_add(dev, napi, func);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */

@@
expression napi, irq;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0)
netif_napi_set_irq_locked(napi, irq);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
+netif_napi_set_irq(napi, irq);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */

@@
expression napi;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0)
netif_napi_del_locked(napi);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
+netif_napi_del(napi);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */

@@
expression napi;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0)
		napi_disable_locked(napi);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
+		napi_disable(napi);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */

@@
expression napi;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0)
		napi_enable_locked(napi);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */
+		napi_enable(napi);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,14,0) */

