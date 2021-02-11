/* SPDX-License-Identifier: (GPL-2.0 OR MIT)
 * Google virtual Ethernet (gve) driver
 *
 * Copyright (C) 2015-2019 Google, Inc.
 */

#ifndef _GVE_UTILS_H
#define _GVE_UTILS_H

#include <linux/etherdevice.h>

#include "gve.h"

void gve_tx_remove_from_block(struct gve_priv *priv, int queue_idx);
void gve_tx_add_to_block(struct gve_priv *priv, int queue_idx);

void gve_rx_remove_from_block(struct gve_priv *priv, int queue_idx);
void gve_rx_add_to_block(struct gve_priv *priv, int queue_idx);

#endif /* _GVE_UTILS_H */

