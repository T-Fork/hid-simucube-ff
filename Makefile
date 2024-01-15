KVERSION ?= `uname -r`
KERNEL_SRC ?= /lib/modules/${KVERSION}/build
MODULEDIR ?= /lib/modules/${KVERSION}/kernel/drivers/hid

default:
	@echo -e "\n::\033[32m Compiling Simucube kernel module\033[0m"
	@echo "========================================"
	$(MAKE) -C $(KERNEL_SRC) M=$$PWD

clean:
	@echo -e "\n::\033[32m Cleaning Simucube kernel module\033[0m"
	@echo "========================================"
	$(MAKE) -C $(KERNEL_SRC) M=$$PWD clean

install:
	@echo -e "\n::\033[34m Installing Simucube kernel module/udev rule\033[0m"
	@echo "====================================================="
	@cp -v hid-simucube.ko ${MODULEDIR}
	@cp -v simucube.rules /etc/udev/rules.d/72-simucube.rules
	depmod

uninstall:
	@echo -e "\n::\033[34m Uninstalling Simucube kernel module/udev rule\033[0m"
	@echo "====================================================="
	@rm -fv ${MODULEDIR}/hid-simucube.ko
	@rm -fv /etc/udev/rules.d/72-simucube.rules
	depmod
