# Linux kernel driver for Compute Engine Virtual Ethernet

This repository contains the source for building an out-of-tree Linux kernel
module for the Compute Engine Virtual Ethernet device.

# Supported Hardware

The driver here binds to a single PCI device id used by the virtual Ethernet
device found in some Compute Engine VMs.

Field         | Value    | Comments
------------- | -------- | --------
Vendor ID     | `0x1AE0` | Google
Device ID     | `0x0042` |
Sub-vendor ID | `0x1AE0` | Google
Sub-device ID | `0x0058` |
Revision ID   | `0x0`    |
Device Class  | `0x200`  | Ethernet

# Supported Kernels

4.16, 4.14, 4.9, 4.6, 4.4, 4.2, 3.19, 3.16, 3.13, 3.10

# Installation

## GitHub

If you downloaded a release tarball from GitHub: This source code should
already be multi-kernel compatible. You can skip down to Compiling the Driver.

### Building the multi-kernel compatible driver source

If you downloaded the source from GitHub: To install this driver on anything
other than the current upstream kernel, you will need to download coccinelle,
and untar it. It can be found here: http://coccinelle.lip6.fr/download.php

NOTE: Most distros will include a version of coccinelle in their package manager
but we require version 1.0.6 or newer, available from the official website.

To build the multi-kernel compatible driver source:

```bash
export SPATCH='/path/to/coccinelle/spatch.opt'
./build_src.sh --target=oot
```

TIP: The spatch path may be omitted if it has been installed on the search path.

### Compiling the driver

Building and installing this driver requires that you have the headers installed
for your current kernel version. Ensure the drivers are installed, then build,
install, and load `gve.ko`:

```bash
make -C /lib/modules/`uname -r`/build M=$(pwd)/build modules modules_install
depmod
modprobe gve
```

# Configuration

## Ethtool

gve supports common ethtool operations including controlling offloads and
multi-queue operation.

Viewing/Changing the number of queues per traffic class is done via ethtool.

```bash
ethtool --show-channels devname
```

Returns the number of transmit and receive queues per traffic class, along with
those maximums.

```bash
ethtool --set-channels devname [rx N] [tx N] [combined N]
```

combined: attempts to set both rx and tx queues to N rx: attempts to set rx
queues to N tx: attempts to set tx queues to N
