# Feature Changelog
Below is a changelog of features and major fixes which have been introduced to GVE. Note that it is recommended to always use the latest version of GVE, regardless of the features being used, as there might be smaller stability and compatibility patches introduced in more minor releases.

#### [v1.4.10](https://github.com/googlecloudplatform/compute-virtual-ethernet-linux/releases/tag/v1.5.0)
* Optimize and enable HW GRO for DQO.
* Enable support for UDP GSO when using DQO format.
* Improve QPL management and enable support for larger ring sizes when using the DQO-QPL queue format.
* Fix out-of-bounds access in `gve_tx_stop_ring_dqo()` due to incorrect QPL buffer cleanup.
#### [v1.4.9](https://github.com/GoogleCloudPlatform/compute-virtual-ethernet-linux/releases/tag/v1.4.9)
* Convert to use `.get_rx_ring_count`.
* Fix probe failure if clock read fails.
* Stats reporting fixes.
#### [v1.4.8](https://github.com/GoogleCloudPlatform/compute-virtual-ethernet-linux/releases/tag/v1.4.8)
* Add support for modifying RX buffer length via ethtool for DQO.
* Add XDP HW RX Timestamping support for DQ.
* Decouple header split from RX buffer length.
* Fix: prevent ethtool operations after device shutdown.
* Fix: check for valid timestamp bit in RX descriptor before applying hardware timestamp.
#### [v1.4.7](https://github.com/GoogleCloudPlatform/compute-virtual-ethernet-linux/releases/tag/v1.4.7)
* Coccinelle fixes for XDP
* Documentation updates
#### [v1.4.6](https://github.com/GoogleCloudPlatform/compute-virtual-ethernet-linux/releases/tag/v1.4.6)[Deprecated]
* Control/dataplane interaction fixes for XDP
* SKB RX timestamping
* XDP for DQO-RDA queue format (including AF_XDP zero-copy)
#### [v1.4.5](https://github.com/GoogleCloudPlatform/compute-virtual-ethernet-linux/releases/tag/v1.4.5)
* Page pool buffer support
#### [v1.4.3](https://github.com/GoogleCloudPlatform/compute-virtual-ethernet-linux/releases/tag/v1.4.3)
* RSS support
#### [v1.4.2](https://github.com/GoogleCloudPlatform/compute-virtual-ethernet-linux/releases/tag/v1.4.2)
* Receive flow steering
* TSO descriptor limit fix
* XDP GQ counter overflow fix
#### [v1.4.0](https://github.com/GoogleCloudPlatform/compute-virtual-ethernet-linux/releases/tag/v1.4.0)[Deprecated]
* DQO QPL queue format
* Non-4K page size support
* Header-data split
* Modify ring size
#### [v1.3.4](https://github.com/GoogleCloudPlatform/compute-virtual-ethernet-linux/releases/tag/v1.3.4)[Deprecated]
* XDP support for GQI-QPL (including AF_XDP zero-copy)
* IPv6 BigTCP support on DQ
#### [v1.3.0](https://github.com/GoogleCloudPlatform/compute-virtual-ethernet-linux/releases/tag/v1.3.0)[Deprecated]
* DQO RDA queue format
#### [v1.2.0](https://github.com/GoogleCloudPlatform/compute-virtual-ethernet-linux/releases/tag/v1.2.0)
* Support for driver suspend/resume
