@ gve_get_rxfh @
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5))
static int gve_get_rxfh(struct net_device *netdev, struct ethtool_rxfh_param *rxfh)
{
	...
}
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */
+static int gve_get_rxfh(struct net_device *netdev, u32 *indir, u8 *key, u8 *hfunc)
+{
+	struct gve_priv *priv = netdev_priv(netdev);
+
+	if (!priv->rss_key_size || !priv->rss_lut_size)
+		return -EOPNOTSUPP;
+
+	if (priv->cache_rss_config) {
+		gve_get_rss_config_cache(priv, indir, key, hfunc);
+		return 0;
+	}
+
+	return gve_adminq_query_rss_config(priv, indir, key, hfunc);
+}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */

@ gve_set_rxfh @
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5))
static int gve_set_rxfh(struct net_device *netdev, struct ethtool_rxfh_param *rxfh,
			struct netlink_ext_ack *extack)
{
	...
}
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */
+static int gve_set_rxfh(struct net_device *netdev, const u32 *indir,
+			 const u8 *key, const u8 hfunc)
+{
+	struct gve_priv *priv = netdev_priv(netdev);
+	int err;
+
+	if (!priv->rss_key_size || !priv->rss_lut_size)
+		return -EOPNOTSUPP;
+
+	err = gve_adminq_configure_rss(priv, indir, key, hfunc);
+	if (err) {
+		dev_err(&priv->pdev->dev, "Fail to configure RSS config\n");
+		return err;
+	}
+
+	if (priv->cache_rss_config)
+		gve_set_rss_config_cache(priv, indir, key, hfunc);
+
+	return 0;
+}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5))
int gve_init_rss_config(struct gve_priv *priv, u16 num_queues)
{
	...
}
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */
+int gve_init_rss_config(struct gve_priv *priv, u16 num_queues)
+{
+	struct gve_rss_config *rss_config = &priv->rss_config;
+	u16 i;
+
+	if (!priv->cache_rss_config)
+		return 0;
+
+	for (i = 0; i < priv->rss_lut_size; i++)
+		rss_config->hash_lut[i] = i % num_queues;
+
+	netdev_rss_key_fill(rss_config->hash_key, priv->rss_key_size);
+
+	return gve_adminq_configure_rss(priv, NULL, NULL, ETH_RSS_HASH_TOP);
+}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5))
static void gve_set_rss_config_cache(struct gve_priv *priv,
				     struct ethtool_rxfh_param *rxfh)
{
	...
}
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */
+static void gve_set_rss_config_cache(struct gve_priv *priv,
+				      const u32 *indir, const u8 *key,
+				      const u8 hfunc)
+{
+	struct gve_rss_config *rss_config = &priv->rss_config;
+
+	if (key)
+		memcpy(rss_config->hash_key, key, priv->rss_key_size);
+
+	if (indir)
+		memcpy(rss_config->hash_lut, indir,
+		       priv->rss_lut_size * sizeof(*indir));
+}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5))
static void gve_get_rss_config_cache(struct gve_priv *priv,
				     struct ethtool_rxfh_param *rxfh)
{
	...
}
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */
+static void gve_get_rss_config_cache(struct gve_priv *priv,
+				     u32 *indir, u8 *key, u8 *hfunc)
+{
+	struct gve_rss_config *rss_config = &priv->rss_config;
+
+	if (hfunc)
+		*hfunc = ETH_RSS_HASH_TOP;;
+
+	if (key)
+		memcpy(key, rss_config->hash_key, priv->rss_key_size);
+
+	if (indir)
+		memcpy(indir, rss_config->hash_lut,
+		       priv->rss_lut_size * sizeof(*indir));
+}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5))
int gve_adminq_configure_rss(struct gve_priv *priv, struct ethtool_rxfh_param *rxfh);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */
+int gve_adminq_configure_rss(struct gve_priv *priv, const u32 *indir, const u8 *hash_key, const u8 hfunc);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5))
int gve_adminq_query_rss_config(struct gve_priv *priv, struct ethtool_rxfh_param *rxfh);
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */
+int gve_adminq_query_rss_config(struct gve_priv *priv, u32 *indir, u8 *key, u8 *hfunc);
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5))
int gve_adminq_configure_rss(struct gve_priv *priv, struct ethtool_rxfh_param *rxfh)
{
	...
}
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */
+int gve_adminq_configure_rss(struct gve_priv *priv, const u32 *indir, const u8 *hash_key, const u8 hfunc)
+{
+	const u32 *hash_lut_to_config = NULL;
+	const u8 *hash_key_to_config = NULL;
+	dma_addr_t lut_bus = 0, key_bus = 0;
+	union gve_adminq_command cmd;
+	__be32 *lut = NULL;
+	u8 hash_alg = 0;
+	u8 *key = NULL;
+	int err = 0;
+	u16 i;
+
+	switch (hfunc) {
+	case ETH_RSS_HASH_NO_CHANGE:
+		fallthrough;
+	case ETH_RSS_HASH_TOP:
+		hash_alg = ETH_RSS_HASH_TOP;
+		break;
+	default:
+		return -EOPNOTSUPP;
+	}
+
+	if (indir) {
+		hash_lut_to_config = indir;
+	} else if (priv->cache_rss_config) {
+		hash_lut_to_config = priv->rss_config.hash_lut;
+	}
+
+	if (hash_lut_to_config) {
+		lut = dma_alloc_coherent(&priv->pdev->dev,
+					 priv->rss_lut_size * sizeof(*lut),
+					 &lut_bus, GFP_KERNEL);
+		if (!lut)
+			return -ENOMEM;
+
+		for (i = 0; i < priv->rss_lut_size; i++)
+			lut[i] = cpu_to_be32(hash_lut_to_config[i]);
+	}
+
+	if (hash_key) {
+		hash_key_to_config = hash_key;
+	} else if (priv->cache_rss_config) {
+		hash_key_to_config = priv->rss_config.hash_key;
+	}
+
+	if (hash_key_to_config) {
+		key = dma_alloc_coherent(&priv->pdev->dev,
+					 priv->rss_key_size,
+					 &key_bus, GFP_KERNEL);
+		if (!key) {
+			err = -ENOMEM;
+			goto out;
+		}
+
+		memcpy(key, hash_key_to_config, priv->rss_key_size);
+	}
+
+	memset(&cmd, 0, sizeof(cmd));
+	cmd.opcode = cpu_to_be32(GVE_ADMINQ_CONFIGURE_RSS);
+	cmd.configure_rss = (struct gve_adminq_configure_rss) {
+		.hash_types = cpu_to_be16(BIT(GVE_RSS_HASH_TCPV4) |
+					  BIT(GVE_RSS_HASH_UDPV4) |
+					  BIT(GVE_RSS_HASH_TCPV6) |
+					  BIT(GVE_RSS_HASH_UDPV6)),
+		.hash_alg = hash_alg,
+		.hash_key_size = cpu_to_be16((key_bus) ? priv->rss_key_size : 0),
+		.hash_lut_size = cpu_to_be16((lut_bus) ? priv->rss_lut_size : 0),
+		.hash_key_addr = cpu_to_be64(key_bus),
+		.hash_lut_addr = cpu_to_be64(lut_bus),
+	};
+
+	err = gve_adminq_execute_cmd(priv, &cmd);
+
+out:
+	if (lut)
+		dma_free_coherent(&priv->pdev->dev,
+				  priv->rss_lut_size * sizeof(*lut),
+				  lut, lut_bus);
+	if (key)
+		dma_free_coherent(&priv->pdev->dev,
+				  priv->rss_key_size, key, key_bus);
+	return err;
+}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5))
static int gve_adminq_process_rss_query(struct gve_priv *priv,
					struct gve_query_rss_descriptor *descriptor,
					struct ethtool_rxfh_param *rxfh)
{
	...
}
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */
+static int gve_adminq_process_rss_query(struct gve_priv *priv,
+					struct gve_query_rss_descriptor *descriptor,
+					u32 *indir, u8 *key, u8 *hfunc)
+{
+	u32 total_memory_length;
+	u16 hash_lut_length;
+	void *rss_info_addr;
+	__be32 *lut;
+	u16 i;
+
+	total_memory_length = be32_to_cpu(descriptor->total_length);
+	hash_lut_length = priv->rss_lut_size * sizeof(*indir);
+
+	if (sizeof(*descriptor) + priv->rss_key_size + hash_lut_length != total_memory_length) {
+		dev_err(&priv->dev->dev,
+			"rss query desc from device has invalid length parameter.\n");
+		return -EINVAL;
+	}
+
+	if (hfunc)
+		*hfunc = descriptor->hash_alg;
+
+	rss_info_addr = (void *)(descriptor + 1);
+	if (key)
+		memcpy(key, rss_info_addr, priv->rss_key_size);
+
+	rss_info_addr += priv->rss_key_size;
+	lut = (__be32 *)rss_info_addr;
+	if (indir) {
+		for (i = 0; i < priv->rss_lut_size; i++)
+			indir[i] = be32_to_cpu(lut[i]);
+	}
+
+	return 0;
+}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */

@@
@@
+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5))
int gve_adminq_query_rss_config(struct gve_priv *priv, struct ethtool_rxfh_param *rxfh)
{
	...
}
+#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */
+int gve_adminq_query_rss_config(struct gve_priv *priv, u32 *indir, u8 *key, u8 *hfunc)
+{
+	struct gve_query_rss_descriptor *descriptor;
+	union gve_adminq_command cmd;
+	dma_addr_t descriptor_bus;
+	int err = 0;
+
+	descriptor = dma_pool_alloc(priv->adminq_pool, GFP_KERNEL, &descriptor_bus);
+	if (!descriptor)
+		return -ENOMEM;
+
+	memset(&cmd, 0, sizeof(cmd));
+	cmd.opcode = cpu_to_be32(GVE_ADMINQ_QUERY_RSS);
+	cmd.query_rss = (struct gve_adminq_query_rss) {
+		.available_length = cpu_to_be64(GVE_ADMINQ_BUFFER_SIZE),
+		.rss_descriptor_addr = cpu_to_be64(descriptor_bus),
+	};
+	err = gve_adminq_execute_cmd(priv, &cmd);
+	if (err)
+		goto out;
+
+	err = gve_adminq_process_rss_query(priv, descriptor, indir, key, hfunc);
+
+out:
+	dma_pool_free(priv->adminq_pool, descriptor, descriptor_bus);
+	return err;
+}
+#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) || RHEL_VERSION_GTE(9,5) */
