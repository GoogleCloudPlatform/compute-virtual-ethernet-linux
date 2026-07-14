# Linux kernel driver for Compute Engine Virtual Ethernet

This repository contains the source for building an out-of-tree Linux kernel
module for the Compute Engine Virtual Ethernet device. This branch specifically
contains a driver with support for U4 instances using the GVE network
interface:
* Support for Gvnic network interfaces on u4c-metal. **u4c-metal instances will fail to gain network connectivity without this driver**
* Support for PTP clock synchronization on u4s VMs. If your u4s VM needs to syncrhonize the system clock to the NIC clock with chrony, you must use this driver.

NOTE: This branch is temporary and will be deleted following the changes in this
branch being upstreamed. Following that, users of this branch can rely on the
in tree driver of the distro of their choosing or the OOT driver in this repo in
the `release` branch.

# Supported Hardware

NOTE: This driver is intended only for use on the U4 family of machines. All other families should be utilizing the in-tree driver or the driver located in the `release` branch.

The driver here binds to two PCI device ids used by the virtual Ethernet
device.

The following virtual device is utilized exclusively in VMs.

Field         | Value    | Comments
------------- | -------- | --------
Vendor ID     | `0x1AE0` | Google
Device ID     | `0x0042` |
Sub-vendor ID | `0x1AE0` | Google
Sub-device ID | `0x0058` |
Revision ID   | `0x0`    |
Device Class  | `0x200`  | Ethernet

The following physical device is utilized in U4-metal instances.

Field         | Value    | Comments
------------- | -------- | --------
Vendor ID     | `0x1AE0` | Google
Device ID     | `0x0043` |
Sub-vendor ID | `0x1AE0` | Google
Sub-device ID | `0x0058` |
Revision ID   | `0x0`    |
Device Class  | `0x200`  | Ethernet

# Supported Kernels

This driver has formally been tested in the following distros:
* RHEL 10.2

Reach out to support to request verification for more distros/kernels.

NOTE: The requirement to run this variant of gve may be different than the requirement to run other critical software (i.e. onload) on U4.

# Installation

The source code in this repository should not be used to build production images. It is offered for reference and development only. For production images, the driver should be installed via the published packages. Utilizing the appropriate package repository is strongly encouraged over installing the github artifacts directly to receive critical functionality and security fixes automatically.

## RPM Package Installation

This driver has been published to a yum repo found at https://packages.cloud.google.com/yum/repos/gve-el10-stable

The public key for this RPM can be found at https://packages.cloud.google.com/yum/doc/rpm-package-key-v10.gpg

Alternatively GVE releases can be found [here](https://github.com/GoogleCloudPlatform/compute-virtual-ethernet-linux/releases). GVE releases support installation as an RPM.

Download the target release and run

```
sudo rpm -ivh gve-<VERSION>-1dkms.noarch.rpm
```

to install as an RPM.

`VERSION` above is simply the GVE release version that was downloaded, say, `1.4.10~17`.

Depending on the distro, installing the package might not load the driver. If the driver has not been loaded, refer to [Loading the Driver](#loading-the-driver).

## Building from Source
If the source is part of a tarball from GitHub, this source code should
already be multi-kernel compatible. Continue on to [Building the Driver](#building-the-driver).

### Generating the Multi-Kernel Compatible Driver Source

If the driver source is downloaded from GitHub, depending on the version, multi-kernel compatible source code might need to be built. Check for a `build` directory in the root directory of the git repo. If it exists, this step can be skipped.

If there is no `build` directory, [Coccinelle](https://coccinelle.gitlabpages.inria.fr/website/), a semantic patching tool, will be needed to generate the multi-kernel compatible source. Many distros will include a version of Coccinelle in their package manager but generating GVE source will require version 1.1.0 or newer.

> [!NOTE]
> The latest version of Coccinelle can be downloaded and installed from source from the official [website](https://coccinelle.gitlabpages.inria.fr/website/download.html), or via the OCaml Package Manager, [opam](https://opam.ocaml.org/).

To generate the source:

```bash
export SPATCH='/path/to/coccinelle/spatch'
./build_src.sh --target=oot
```

> [!TIP]
> The `spatch` path may be omitted if it has been installed on the search path.

### Building the Driver

#### Kernel Header Dependencies

Building and installing this driver requires that kernel headers installed for kernel version on which the driver will be loaded.

Using `apt`:
```
sudo apt-get install -y linux-headers-$(uname -r)
```

Using `dnf`:
```
sudo dnf install -y kernel-devel-$(uname -r)
```

#### Compilation and Installation
Once kernel headers are installed, GVE can be compiled and loaded.

```bash
make -C /lib/modules/`uname -r`/build M=$(pwd)/build modules modules_install
```

## Loading the Driver
To load the new driver, run:

```bash
depmod
sudo rmmod gve && sudo modprobe gve
```

Check via `ethtool -i <DEV>` that the new driver is installed.

If not, it can be installed manually using the `.ko` file. Depending on whether the driver was installed via `make` or `DEB/RPM`, the `.ko` file can be in different locations within `/lib/modules/$(uname -r)`.

|Install method|Module location|
|---|---|
|From source|`/lib/modules/$(uname -r)/extra/`|
|DEB|`/lib/modules/$(uname -r)/updates/dkms/`|
|RPM|`/lib/modules/$(uname -r)/extra/`|

> [!NOTE]
> Depnding on the distro, the module might be compressed as an XZ file. It should still be possible to directly `insmod` such a file.

> [!WARNING]
> Run this as a single line, as running `rmmod` alone will remove the existing
> driver and network connectivity will be lost.

```bash
sudo rmmod gve; sudo insmod ./path/to/gve.ko
```

### Automatically loading GVE on boot

Installing GVE from source will not necessarily allow the out-of-tree driver to load on boot. To load the out-of-tree driver on boot, the initramfs will need to re-generated:

```bash
sudo dracut -f
```

# Driver Features and Configuration

## Queue Counts

Viewing/Changing the number of queues per traffic class is done via ethtool.

```bash
ethtool -l|--show-channels <DEV>
```

Returns the number of transmit and receive queues per traffic class, along with
those maximums.

```bash
ethtool -L|--set-channels <DEV> [rx N] [tx N]
```

## Modify Ring Size
GVE has support for changing the ring descriptor counts. To check the current ring size/hardware limits:

```
ethtool -g|--show-ring <DEV>
```

To update the ring size:
```
ethtool -G|--set-ring <DEV> [rx N] [tx N]
```

## RSS Configuration
The DQO RDA queue format has support for querying and configuring the RSS hash and indirection table.

GVE only supports Toeplitz hashing, and the RSS hash key must be exactly 40 bytes. Upon RSS hash configuration, a default RSS indirection table will be set using a round-robin assignment of hash values to queues. The GVE indirection table has 128 entries.

To read the RSS hash and indirection table:
```
ethtool -x|--show-rxfh <DEV>
```

The `ethtool -X` command can be used to set the RSS hash and the indirection table. The ethtool man pages have more information about this, but as an example:
```
ethtool -X eth0 hkey 00:11:22:33:44:55:66:77:88:99:aa:bb:cc:dd:ee:ff:00:11:22:33:44:55:66:77:88:99:aa:bb:cc:dd:ee:ff:00:11:22:33:44:55:66:77 \
        start 4 equal 4
```

will create an RSS table with hash key `00:11:22:33:44:55:66:77:88:99:aa:bb:cc:dd:ee:ff:00:11:22:33:44:55:66:77:88:99:aa:bb:cc:dd:ee:ff:00:11:22:33:44:55:66:77` and an indirection table with all entries filled with `4 5 6 7` in a round-robin fashion, causing an even distribution of traffic on RX queues 4, 5, 6, and 7.

## Receive Flow Steering
Receive flow steering allows the traffic to be routed to a specific queue based on a configured N-tuple. Support for flow steering varies by VM platform, so it is best to check for support before attempting to use the feature:

```bash
$ ethtool -k <DEV> | grep ntuple-filters
ntuple-filters: off [fixed] # not supported
ntuple-filters: off # supported, but not enabled
ntuple-filters: on # enabled
```

To enable/disable:

```
ethtool -K <DEV> ntuple on|off
```

Once enabled, flow rules can be programmed using the standard `ethtool --config-ntuple` interface.

As an example:
```
ethtool --config-ntuple eth0 flow-type tcp4 src-ip 192.168.100.2 dst-ip 192.168.100.1 src-port 12345 dst-port 7777 action 8 loc 1
```

Creates an IPv4 TCP flow rule (ID 1) for interface `eth0` that routes packets from 192.168.100.1:7777 destined for 192.168.100.2:12345 to be directed to queue 8.

## Header-Data Split
Header-data split, or TCP data split is only supported on the DQO RDA format. Support can be tested using `ethtool -g`.

To enable/disable:
```
ethtool -G <DEV> tcp-data-split on|off
```

## XDP
Driver-mode support for [XDP](https://docs.cilium.io/en/latest/reference-guides/bpf/progtypes/#xdp) support was introduced in release [1.3.4](github.com/GoogleCloudPlatform/compute-virtual-ethernet-linux/releases/tag/v1.3.4) for the GQI QPL queue format and [1.4.6](github.com/GoogleCloudPlatform/compute-virtual-ethernet-linux/releases/tag/v1.4.6) for the DQO RDA queue format. The XDP implementation supports the following features:

1) Basic XDP action support (`PASS`, `DROP`, `TX`, )
1) `NDO_XDP_XMIT` API
1) XDP redirect support
1) AF_XDP zero-copy

### Configuration

To attach an XDP program to the driver, the number of RX and TX queues must be
no more than half their maximum values to accommodate the creation of extra XDP TX queues. The maximum values are based on the number of CPUs available. See [Queue Counts](#queue-counts) to see how to get/set the number of queues.

XDP can be enabled via comand line through `iproute2` or `bpftool`, or in a C program using `libbpf`.

`iproute2`:
```
ip link set dev <DEV> xdp obj <XDP_PROG>
```

`bpftool`:
```
bpftool net attach xdp name <XDP_PROG> dev <DEV>
```


# Feature Changelog
See [CHANGELOG.md](CHANGELOG.md) for the feature changelog.

