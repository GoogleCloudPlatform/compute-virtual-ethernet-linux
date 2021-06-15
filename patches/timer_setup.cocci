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
	struct gve_priv *priv = from_timer(priv, t, stats_report_timer);
	...
}
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0) */

@ struct_size @
identifier member;
expression result, p, count;
@@

+#ifndef struct_size
+result = sizeof(*p) + sizeof((p)->member[0]) * (count);
+#else
result = struct_size(p, member, count);
+#endif
