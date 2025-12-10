@ setup @
identifier gve_stats_report_timer;
struct gve_priv *priv;
@@

+#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
+setup_timer(&priv->stats_report_timer, gve_stats_report_timer,
+	     (unsigned long)priv);
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0) */
timer_setup(&priv->stats_report_timer, gve_stats_report_timer, 0);
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0) */

@ service @
type timer_list;
identifier gve_stats_report_timer, t, priv, service_timer;
@@

+#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
+static void gve_stats_report_timer(unsigned long data)
+{
+	struct gve_priv *priv = (struct gve_priv *)data;

+	mod_timer(&priv->stats_report_timer,
+		  round_jiffies(jiffies +
+		  msecs_to_jiffies(priv->stats_report_timer_period)));
+	gve_stats_report_schedule(priv);
+}
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0) */
static void gve_stats_report_timer(timer_list *t)
{
	struct gve_priv *priv = timer_container_of(priv, t,
						   stats_report_timer);
	...
}
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0) */

@ struct_size @
identifier member;
expression result, p, size1, size2;
@@

+#if !defined(struct_size) || !defined(size_add)
+result = sizeof(*p) + sizeof((p)->member[0]) * (size1 + size2);
+#else
result = struct_size(p, member, size_add(size1, size2));
+#endif

@@
expression t;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,15,0)
timer_delete_sync(t);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,15,0) */
+del_timer_sync(t);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,15,0) */

@@
expression list l;
@@

+#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,16,0) || (RHEL_VERSION_GTE(9,8) && RHEL_VERSION_LT(10,0)) || RHEL_VERSION_GTE(10,2)
struct gve_priv *priv = timer_container_of(l);
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(6,16,0) || (RHEL_VERSION_GTE(9,8) && RHEL_VERSION_LT(10,0)) || RHEL_VERSION_GTE(10,2) */
+struct gve_priv *priv = from_timer(l);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,16,0) || (RHEL_VERSION_GTE(9,8) && RHEL_VERSION_LT(10,0)) || RHEL_VERSION_GTE(10,2) */

