// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/* Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2015-2019 Google, Inc.
 */

#include "gve_utils.h"

void gve_tx_remove_from_block(struct gve_priv *priv, int queue_idx)
{
	struct gve_notify_block *block =
			&priv->ntfy_blocks[gve_tx_idx_to_ntfy(priv, queue_idx)];

	block->tx = NULL;
}

static void gve_tx_add_to_block(struct gve_priv *priv, int queue_idx)
{
       int ntfy_idx = gve_tx_idx_to_ntfy(priv, queue_idx);
       struct gve_notify_block *block = &priv->ntfy_blocks[ntfy_idx];
       struct gve_tx_ring *tx = &priv->tx[queue_idx];
       int num_cpus, group_size, cpu;
       cpumask_var_t mask;

       block->tx = tx;
       tx->ntfy_id = ntfy_idx;

       if (!zalloc_cpumask_var(&mask, GFP_KERNEL)) {
               netdev_warn(priv->dev, "Failed to zalloc cpumask!");
               return;
       }
       num_cpus = num_online_cpus();
       group_size = priv->tx_cfg.num_queues;
       for (cpu = queue_idx; cpu < num_cpus; cpu += group_size)
               cpumask_set_cpu(cpu, mask);
       netif_set_xps_queue(priv->dev, mask, queue_idx);
       free_cpumask_var(mask);
}

void gve_rx_remove_from_block(struct gve_priv *priv, int queue_idx)
{
	struct gve_notify_block *block =
			&priv->ntfy_blocks[gve_rx_idx_to_ntfy(priv, queue_idx)];

	block->rx = NULL;
}

void gve_rx_add_to_block(struct gve_priv *priv, int queue_idx)
{
	u32 ntfy_idx = gve_rx_idx_to_ntfy(priv, queue_idx);
	struct gve_notify_block *block = &priv->ntfy_blocks[ntfy_idx];
	struct gve_rx_ring *rx = &priv->rx[queue_idx];

	block->rx = rx;
	rx->ntfy_id = ntfy_idx;
}

