@ setup @
identifier gve_service_timer;
struct gve_priv *priv;
@@

+#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
+setup_timer(&priv->service_timer, gve_service_timer,
+	     (unsigned long)priv);
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0) */
timer_setup(&priv->service_timer, gve_service_timer, 0);
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0) */

@ service @
type timer_list;
identifier gve_service_timer, t, priv, service_timer;
@@

+#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
+static void gve_service_timer(unsigned long data)
+{
+	struct gve_priv *priv = (struct gve_priv *)data;

+	mod_timer(&priv->service_timer,
+		  round_jiffies(jiffies +
+		  msecs_to_jiffies(priv->service_timer_period)));
+	gve_service_task_schedule(priv);
+}
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0) */
static void gve_service_timer(timer_list *t)
{
	struct gve_priv *priv = from_timer(priv, t, service_timer);
	...
}
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0) */
