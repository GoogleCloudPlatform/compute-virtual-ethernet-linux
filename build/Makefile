# Google virtual Ethernet (gve) driver

ifeq (,$(BUILD_KERNEL))
 BUILD_KERNEL=$(shell uname -r)
endif

ifeq (,$(DESTDIR))
 DESTDIR="/lib/modules/$(BUILD_KERNEL)/kernel/drivers/net/ethernet/google/gve/"
endif

obj-m += gve.o
gve-objs := gve_main.o gve_tx.o gve_tx_dqo.o gve_rx.o gve_rx_dqo.o gve_ethtool.o gve_adminq.o gve_utils.o

ifeq (,$(KERNELDIR))
KERNELDIR := /lib/modules/$(BUILD_KERNEL)/build
endif


default:
	$(MAKE) -C $(KERNELDIR) M=$$PWD

clean:
	@-rm -rf gve_main.o gve_tx.o gve_tx_dqo.o gve_rx.o gve_rx_dqo.o \
	gve_ethtool.o gve_adminq.o gve_adminq_dqo.o gve_utils.o gve.o \
	built-in.o Module.symvers modules.order gve.ko *.mod.* .*.*o.cmd .tmp*

install:
	@mkdir -p $(DESTDIR)
	@cp $$PWD/gve.ko $(DESTDIR)/gve.ko
	@depmod -a
