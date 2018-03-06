/*
 * Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2015-2018 Google, Inc.
 *
 * This software is available to you under a choice of one of two licenses. You
 * may choose to be licensed under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation, and may be copied,
 * distributed, and modified under those terms. See the GNU General Public
 * License for more details. Otherwise you may choose to be licensed under the
 * terms of the MIT license below.
 *
 * --------------------------------------------------------------------------
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
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
	char *s = (char *)data;

	if (stringset != ETH_SS_STATS)
		return;

	memcpy(s, *gve_gstrings_main_stats,
	       sizeof(gve_gstrings_main_stats));
	s += sizeof(gve_gstrings_main_stats);
	sprintf(s, "rx_desc_cnt");
	s += ETH_GSTRING_LEN;
	sprintf(s, "rx_desc_fill_cnt");
	s += ETH_GSTRING_LEN;
	sprintf(s, "tx_req");
	s += ETH_GSTRING_LEN;
	sprintf(s, "tx_done");
	s += ETH_GSTRING_LEN;
	sprintf(s, "tx_wake");
	s += ETH_GSTRING_LEN;
	sprintf(s, "tx_stop");
	s += ETH_GSTRING_LEN;
	sprintf(s, "tx_event_counter");
	s += ETH_GSTRING_LEN;
}

static int gve_get_sset_count(struct net_device *netdev, int sset)
{
	struct gve_priv *priv = netdev_priv(netdev);

	if (!priv->is_up)
		return 0;

	switch (sset) {
	case ETH_SS_STATS:{
		return GVE_MAIN_STATS_LEN + NUM_GVE_RX_CNTS + NUM_GVE_TX_CNTS;
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
	int i;

	ASSERT_RTNL();

	if (!priv->is_up)
		return;

	memset(data, 0, GVE_MAIN_STATS_LEN * sizeof(*data));

	i = 0;
	data[i++] = priv->rx->rpackets;
	data[i++] = priv->tx->pkt_done;
	data[i++] = priv->rx->rbytes;
	data[i++] = priv->tx->bytes_done;

	i = GVE_MAIN_STATS_LEN;
	data[i++] = priv->rx->desc.cnt;
	data[i++] = priv->rx->desc.fill_cnt;
	data[i++] = priv->tx->req;
	data[i++] = priv->tx->done;
	data[i++] = priv->tx->wake_queue;
	data[i++] = priv->tx->stop_queue;
	data[i++] = be32_to_cpu(gve_tx_load_event_counter(priv, priv->tx));
}

void gve_get_channels(struct net_device *netdev, struct ethtool_channels *cmd)
{
	cmd->max_rx = 1;
	cmd->max_tx = 1;
	cmd->max_other = 0;
	cmd->max_combined = 1;
	cmd->rx_count = 1;
	cmd->tx_count = 1;
	cmd->other_count = 0;
	cmd->combined_count = 1;
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
	if (*flags == ETH_RESET_ALL) {
		/* TODO(csully): look into a way to wait here until the reset
		 * has actually completed instead of assuming it has and
		 * everything was successful.
		 */
		gve_schedule_aq_reset(netdev_priv(netdev));
		*flags = 0;
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
	.get_channels = gve_get_channels,
	.get_link = ethtool_op_get_link,
	.get_ringparam = gve_get_ringparam,
	.reset = gve_reset,
};
