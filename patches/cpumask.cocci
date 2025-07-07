@cpumask_iter_decl@
@@
{
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0)
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0) */
+int _startidx_comp_iter;
+int start_idx;
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0) */
<+... cpumask_nth(...) ...+>
}

@cpumask_nth@
expression N, node_mask, cpu;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0)
cpu = cpumask_nth(N, node_mask);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0) */
+cpu = cpumask_first(node_mask);
+start_idx = N;
+for (_startidx_comp_iter = 0; _startidx_comp_iter < start_idx; _startidx_comp_iter++) {
+        cpu = cpumask_next(cpu, node_mask);
+}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0)*/

@@
identifier irq_cpu;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,2,0)
irq_cpu = cpumask_first(topology_sibling_cpumask(irq_cpu));
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,2,0) */
+irq_cpu = cpumask_first(topology_thread_cpumask(irq_cpu));
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,2,0) */

@@
expression irq, mask;
@@
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0)
irq_set_affinity_and_hint(irq, mask);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0) */
+irq_set_affinity_hint(irq, mask);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0) */
