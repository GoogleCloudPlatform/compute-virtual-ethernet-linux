# Google virtual Ethernet (gve) driver

ifeq (,$(BUILD_KERNEL))
 BUILD_KERNEL=$(shell uname -r)
endif

ifeq (,$(DESTDIR))
 DESTDIR="/lib/modules/$(BUILD_KERNEL)/kernel/drivers/net/ethernet/google/gve/"
endif

obj-m += gve.o
gve-y := gve_main.o gve_tx.o gve_tx_dqo.o gve_rx.o gve_rx_dqo.o gve_ethtool.o gve_adminq.o gve_utils.o gve_flow_rule.o \
	    gve_buffer_mgmt_dqo.o gve_mailbox.o

gve-$(CONFIG_PTP_1588_CLOCK) += gve_ptp.o

ifeq (,$(KERNELDIR))
KERNELDIR := /lib/modules/$(BUILD_KERNEL)/build
endif


default:
	$(MAKE) -C $(KERNELDIR) M=$$PWD

clean:
	$(MAKE) -C $(KERNELDIR) M=$$PWD clean

install:
	@mkdir -p $(DESTDIR)
	@cp $$PWD/gve.ko $(DESTDIR)/gve.ko
	@depmod -a
