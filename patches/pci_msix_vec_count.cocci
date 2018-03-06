@find_func@
identifier func;
expression pdev;
identifier num_ntfy;
@@
func(...)
{...
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
num_ntfy = pci_msix_vec_count(pdev);
if (num_ntfy <=0)
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
+backport_err = pci_read_config_word(pdev, pdev->msix_cap + PCI_MSIX_FLAGS, &backport_val);
+num_ntfy = (backport_val & PCI_MSIX_FLAGS_QSIZE) + 1;
+if (num_ntfy <=0 || backport_err)
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
{...}
...}

@add_vars depends on find_func@
identifier find_func.func;
typedef u16;
@@
func(...)
{
+#if LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0)
+u16 backport_val;
+int backport_err;
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0) */
...}

