@ setup @
identifier gve_tx_timeout_timer;
struct gve_priv *priv;
@@

+#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
+setup_timer(&priv->tx_timeout_timer, gve_tx_timeout_timer,
+	     (unsigned long)priv);
+#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0) */
timer_setup(&priv->tx_timeout_timer, gve_tx_timeout_timer, 0);
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0) */

@ service @
type timer_list;
identifier gve_tx_timeout_timer, t, priv, tx_timeout_timer;
@@

+#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
+static void gve_tx_timeout_timer(unsigned long data)
+{
+	struct gve_priv *priv = (struct gve_priv *)data;
+	int i;
+
+	for(i = 0; i < priv->tx_cfg.num_queues; i++) {
+		if (time_after(jiffies, priv->tx[i].dqo_compl.last_processed
+			       + priv->tx_timeout_period)) {
+			gve_tx_timeout_for_miss_path(priv->dev, i);
+		}
+	}
+	mod_timer(&priv->tx_timeout_timer,
+		  jiffies + priv->tx_timeout_period);
+}
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0) */
static void gve_tx_timeout_timer(timer_list *t)
{
	struct gve_priv *priv = from_timer(priv, t, tx_timeout_timer);
	...
}
+#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0) */
