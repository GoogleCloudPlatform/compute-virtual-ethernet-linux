@ pm_ops @
declarer name DEFINE_SIMPLE_DEV_PM_OPS;
declarer name SIMPLE_DEV_PM_OPS;
identifier ops, suspend, resume;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0)
static DEFINE_SIMPLE_DEV_PM_OPS(ops, suspend, resume);
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(5,17,0) */
+#ifdef CONFIG_PM_SLEEP
+static SIMPLE_DEV_PM_OPS(ops, suspend, resume);
+#endif /* CONFIG_PM_SLEEP */
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0) */

@ pm_ptr @
expression E;
@@

static struct pci_driver gve_driver = {
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0)
.driver.pm = pm_sleep_ptr(E),
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(5,17,0) */
+#ifdef CONFIG_PM_SLEEP
+.driver.pm = E,
+#endif /* CONFIG_PM_SLEEP */
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0) */
...
};

