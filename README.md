# Linux kernel driver for Compute Engine Virtual Ethernet

This repository contains the source for building an out-of-tree Linux kernel
module for the Compute Engine Virtual Ethernet device.

This driver as well as the GCE VM virtual device are in Early Access stage [1],
the feature is available to a closed group of testers.

[1] https://cloud.google.com/terms/launch-stages

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

This driver s supported on any of the [distros listed as supporting gVNIC](https://cloud.google.com/compute/docs/images/os-details#networking).
Those distros have native drivers for gVNIC, but this driver can be used to
replace the native driver to get the latest enhancements. Note that native
drivers are likely to report version 1.0.0, this should be ignored. The
upstream community has deprecated the use of driver versions it has not
been updated since the initial upstream version.

This driver is also supported on [clean Linux LTS kernels that are not EOL](https://www.kernel.org/category/releases.html).
Linux kernels starting 5.4 have the driver built in, but this driver can be
installed on any LTS kernel to get the latest enhancements.

Debian 9 and 10 are supported since they use clean Linux LTS kernels
(4.9 and 4.19 respectively).

Versions that are not marked as a release candidate (rc) correspond to upstream
versions of the driver. It is our intention that release candidates
will be upstreamed in the near future, but when and in what form this happens
depends on the Linux community and the upstream review process. We can't
guarantee that a release candidate will land upstream as-is or if it
will be accepted upstream at all.

# Installation

## GitHub

If you downloaded a release tarball from GitHub: This source code should
already be multi-kernel compatible. You can skip down to Compiling the Driver.

### Building the multi-kernel compatible driver source

If you downloaded the source from GitHub: To install this driver on anything
other than the current upstream kernel, you will need to download Coccinelle,
and untar it. It can be found here: https://coccinelle.gitlabpages.inria.fr/website/download.html

NOTE: Most distros will include a version of Coccinelle in their package manager
but we require version 1.0.6 or newer, available from the official website.

To build the multi-kernel compatible driver source:

```bash
export SPATCH='/path/to/coccinelle/spatch'
./build_src.sh --target=oot
```

TIP: The spatch path may be omitted if it has been installed on the search path.

### Compiling the driver

Building and installing this driver requires that you have the headers installed
for your current kernel version. On CentOS, you might have to additionally
install the `kernel-devel` package.

Ensure the headers are installed, then build,
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

### Manual Configuration

To manually configure gVNIC, you'll need to complete the following steps:

* Install a driver capable of driving the new device installed in the guest OS.
The Linux driver source is available on GitHub and is upstreamed into the Linux kernel. 
* Create a boot disk tagged with the 'GVNIC' guest OS feature. 
* Ensure that the nic-type is set to GVNIC for all interfaces intended to use gVNIC. To switch back to VirtioNet (the default option), using the same image, recreate the VM with change the nic-type set to VIRTIO_NET.


The crucial step for both is ensuring you have a capable driver installed. An instance created with a gVNIC interface and no driver will have no internal or external network connectivity, including SSH and RDP. The interactive serial console may be useful for debugging if this situation arises.

#### Instructions
To build and install the kernel driver from source you must have a supported kernel with headers installed. Our driver supports building against mainline and major distribution kernels versions 3.10 and later. Follow your distro's instructions for how to do that. Source code and driver packages can be obtained from GitHub under [GoogleCloudPlatform/compute-virtual-ethernet-linux](https://github.com/GoogleCloudPlatform/compute-virtual-ethernet-linux).

1. Install or update kernel and headers to version 3.10 or later. To complete this step, review the documentation for your operating system. Source code and driver packages can be obtained from GitHub, see [GoogleCloudPlatform/compute-virtual-ethernet-linux](https://github.com/GoogleCloudPlatform/compute-virtual-ethernet-linux).

For example, if you are using Debian, you can install the kernel driver using the deb package either by running:
```shell
dpkg -i gve-dkms_1.1.0_all.deb
```

or

```shell
apt update
apt install ./gve-dkms_1.1.0_all.deb
```
Where gve-dkms_1.1.0_all.deb is the driver deb package that you have been supplied with. This will install the driver into the correct place in your filesystem.

2. To load the driver run:
```shell
modprobe gve
```
This loads the driver until you remove it with rmmod or you reboot.

3. To have the driver automatically load on boot if you are using systemd run:
```shell
echo gve > /etc/modules-load.d/gve.conf
```

Optional: If you are using debian run the following instead of the above command:
```shell
echo gve >> /etc/modules
```

4. Stop the VM.

5. Create an image from the disk that is attached to the VM that you just stopped.
```shell
gcloud compute images create IMAGE_NAME\
        --source-disk DISK_NAME \
        --guest-os-features=GVNIC
```
 
6. Replace the following:

IMAGE_NAME: The name of the image that you want to create. This image has the Compute Engine virtual network driver installed.
DISK_NAME: The name of the boot disk on the VM instance that you just stopped.

7. Use the image, which now has the gVNIC driver installed, to create and start a VM with a gVNIC network interface.
```shell
gcloud compute instances create INSTANCE_NAME\
		--source-image IMAGE_NAME \
		--zone <preferred compute zone> \
		--network-interface=nic-type=GVNIC
```

Note: Before conducting a manual installation of the gVNIC driver, you will want to consult with your distro's out-of-tree (oot) support policy and [tainted](https://www.kernel.org/doc/html/latest/admin-guide/tainted-kernels.html) kernel support policy.
