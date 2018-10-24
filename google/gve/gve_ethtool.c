// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/* Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2015-2018 Google, Inc.
 */

#include "gve.h"

static void gve_get_drvinfo(struct net_device *netdev,
			    struct ethtool_drvinfo *info)
{
	struct gve_priv *priv = netdev_priv(netdev);

	strlcpy(info->driver, "gve", sizeof(info->driver));
	strlcpy(info->version, gve_version_str, sizeof(info->version));
	strlcpy(info->bus_info, pci_name(priv->pdev), sizeof(info->bus_info));
}

static void gve_set_msglevel(struct net_device *netdev, u32 value)
{
	struct gve_priv *priv = netdev_priv(netdev);

	priv->msg_enable = value;
}

static u32 gve_get_msglevel(struct net_device *netdev)
{
	struct gve_priv *priv = netdev_priv(netdev);

	return priv->msg_enable;
}

static const char gve_gstrings_main_stats[][ETH_GSTRING_LEN] = {
	"rx_packets", "tx_packets", "rx_bytes", "tx_bytes",
	"rx_dropped", "tx_dropped",
};

#define GVE_MAIN_STATS_LEN  ARRAY_SIZE(gve_gstrings_main_stats)
#define NUM_GVE_TX_CNTS	5
#define NUM_GVE_RX_CNTS	2

static void gve_get_strings(struct net_device *netdev, u32 stringset, u8 *data)
{
	struct gve_priv *priv = netdev_priv(netdev);
	char *s = (char *)data;
	int i;

	if (stringset != ETH_SS_STATS)
		return;

	memcpy(s, *gve_gstrings_main_stats,
	       sizeof(gve_gstrings_main_stats));
	s += sizeof(gve_gstrings_main_stats);
	for (i = 0; i < priv->rx_cfg.num_queues; i++) {
		snprintf(s, ETH_GSTRING_LEN, "rx_desc_cnt[%u]", i);
		s += ETH_GSTRING_LEN;
		snprintf(s, ETH_GSTRING_LEN, "rx_desc_fill_cnt[%u]", i);
		s += ETH_GSTRING_LEN;
	}
	for (i = 0; i < priv->tx_cfg.num_queues; i++) {
		snprintf(s, ETH_GSTRING_LEN, "tx_req[%u]", i);
		s += ETH_GSTRING_LEN;
		snprintf(s, ETH_GSTRING_LEN, "tx_done[%u]", i);
		s += ETH_GSTRING_LEN;
		snprintf(s, ETH_GSTRING_LEN, "tx_wake[%u]", i);
		s += ETH_GSTRING_LEN;
		snprintf(s, ETH_GSTRING_LEN, "tx_stop[%u]", i);
		s += ETH_GSTRING_LEN;
		snprintf(s, ETH_GSTRING_LEN, "tx_event_counter[%u]", i);
		s += ETH_GSTRING_LEN;
	}
}

static int gve_get_sset_count(struct net_device *netdev, int sset)
{
	struct gve_priv *priv = netdev_priv(netdev);

	if (!priv->is_up)
		return 0;

	switch (sset) {
	case ETH_SS_STATS:{
		return GVE_MAIN_STATS_LEN +
		       (priv->rx_cfg.num_queues * NUM_GVE_RX_CNTS) +
		       (priv->tx_cfg.num_queues * NUM_GVE_TX_CNTS);
	}
	default:
		return -EOPNOTSUPP;
	}
}

static void
gve_get_ethtool_stats(struct net_device *netdev,
		      struct ethtool_stats *stats, u64 *data)
{
	struct gve_priv *priv = netdev_priv(netdev);
	int ring;
	int i;
	u64 rx_pkts, rx_bytes, tx_pkts, tx_bytes;

	ASSERT_RTNL();

	if (!priv->is_up)
		return;

	for (rx_pkts = 0, rx_bytes = 0, ring = 0;
	     ring < priv->rx_cfg.num_queues; ring++) {
		rx_pkts += priv->rx[ring].rpackets;
		rx_bytes += priv->rx[ring].rbytes;
	}
	for (tx_pkts = 0, tx_bytes = 0, ring = 0;
	     ring < priv->tx_cfg.num_queues; ring++) {
		tx_pkts += priv->tx[ring].pkt_done;
		tx_bytes += priv->tx[ring].bytes_done;
	}
	memset(data, 0, GVE_MAIN_STATS_LEN * sizeof(*data));

	i = 0;
	data[i++] = rx_pkts;
	data[i++] = tx_pkts;
	data[i++] = rx_bytes;
	data[i++] = tx_bytes;
	i = GVE_MAIN_STATS_LEN;

	/* walk RX rings */
	for (ring = 0; ring < priv->rx_cfg.num_queues; ring++) {
		struct gve_rx_ring *rx = &priv->rx[ring];

		data[i++] = rx->desc.cnt;
		data[i++] = rx->desc.fill_cnt;
	}
	/* walk TX rings */
	for (ring = 0; ring < priv->tx_cfg.num_queues; ring++) {
		struct gve_tx_ring *tx = &priv->tx[ring];

		data[i++] = tx->req;
		data[i++] = tx->done;
		data[i++] = tx->wake_queue;
		data[i++] = tx->stop_queue;
		data[i++] = be32_to_cpu(gve_tx_load_event_counter(priv, tx));
	}
}

void gve_get_channels(struct net_device *netdev, struct ethtool_channels *cmd)
{
	struct gve_priv *priv = netdev_priv(netdev);

	cmd->max_rx = priv->rx_cfg.max_queues;
	cmd->max_tx = priv->tx_cfg.max_queues;
	cmd->max_other = 0;
	cmd->max_combined = min_t(u32, priv->rx_cfg.max_queues,
				  priv->tx_cfg.max_queues);
	cmd->rx_count = priv->rx_cfg.num_queues;
	cmd->tx_count = priv->tx_cfg.num_queues;
	cmd->other_count = 0;
	cmd->combined_count = min_t(int, priv->rx_cfg.num_queues,
				    priv->tx_cfg.num_queues);
}

int gve_set_channels(struct net_device *netdev, struct ethtool_channels *cmd)
{
	struct gve_priv *priv = netdev_priv(netdev);
	struct gve_queue_config new_tx_cfg = priv->tx_cfg;
	struct gve_queue_config new_rx_cfg = priv->rx_cfg;
	struct ethtool_channels old_settings;
	int new_tx = cmd->tx_count;
	int new_rx = cmd->rx_count;

	gve_get_channels(netdev, &old_settings);
	if (cmd->combined_count != old_settings.combined_count) {
		/* Changing combined at the same time as rx and tx isn't
		 * allowed
		 */
		if (new_tx != priv->tx_cfg.num_queues ||
		    new_rx != priv->rx_cfg.num_queues)
			return -EINVAL;
		new_rx = cmd->combined_count;
		new_tx = cmd->combined_count;
	}

	if (!new_rx || !new_tx)
		return -EINVAL;

	if (!priv->is_up) {
		priv->tx_cfg.num_queues = new_tx;
		priv->rx_cfg.num_queues = new_rx;
		return 0;
	}

	new_tx_cfg.num_queues = new_tx;
	new_rx_cfg.num_queues = new_rx;

	return gve_adjust_queues(priv, new_rx_cfg, new_tx_cfg);
}

void gve_get_ringparam(struct net_device *netdev,
		       struct ethtool_ringparam *cmd)
{
	struct gve_priv *priv = netdev_priv(netdev);

	cmd->rx_max_pending = priv->rx_desc_cnt;
	cmd->tx_max_pending = priv->tx_desc_cnt;
	cmd->rx_pending = priv->rx_desc_cnt;
	cmd->tx_pending = priv->tx_desc_cnt;
}

int gve_reset(struct net_device *netdev, u32 *flags)
{
	struct gve_priv *priv = netdev_priv(netdev);

	if (*flags == ETH_RESET_ALL) {
		*flags = 0;
		gve_handle_user_reset(priv);
		return 0;
	}

	return -EOPNOTSUPP;
}

const struct ethtool_ops gve_ethtool_ops = {
	.get_drvinfo = gve_get_drvinfo,
	.get_strings = gve_get_strings,
	.get_sset_count = gve_get_sset_count,
	.get_ethtool_stats = gve_get_ethtool_stats,
	.set_msglevel = gve_set_msglevel,
	.get_msglevel = gve_get_msglevel,
	.set_channels = gve_set_channels,
	.get_channels = gve_get_channels,
	.get_link = ethtool_op_get_link,
	.get_ringparam = gve_get_ringparam,
	.reset = gve_reset,
};
