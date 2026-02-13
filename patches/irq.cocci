@@
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,13,0)
err = request_irq(priv->msix_vectors[msix_idx].vector,
				  gve_is_gqi(priv) ? gve_intr : gve_intr_dqo,
				  IRQF_NO_AUTOEN, block->name, block);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,13,0) */
+irq_set_status_flags(priv->msix_vectors[msix_idx].vector, IRQ_NOAUTOEN);
+err = request_irq(priv->msix_vectors[msix_idx].vector,
+ 				  gve_is_gqi(priv) ? gve_intr : gve_intr_dqo,
+ 				  0, block->name, block);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,13,0) */