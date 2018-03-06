@@
identifier num_vec;
expression dev, entries, minvec, maxvec;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
num_vec = pci_enable_msix_range(dev, entries, minvec, maxvec);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
+num_vec = pci_enable_msix(dev, entries, maxvec);
+if (!num_vec) {
+	num_vec = maxvec;
+} else if (num_vec > 0) {
+	if (num_vec >= minvec) {
+		num_vec = pci_enable_msix(dev, entries, minvec);
+		if (num_vec) {
+			dev_err(hdev, "Could not enable min msix %d error %d\n",
+				minvec, num_vec);
+			err = num_vec;
+			goto abort_with_msix_vectors;
+		} else {
+			num_vec = minvec;
+		}
+	} else {
+		dev_err(hdev, "Could not enable msix error %d\n",
+			num_vec);
+		err = num_vec;
+		goto abort_with_msix_vectors;
+	}
+}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) */
