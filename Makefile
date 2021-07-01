SHELL=/bin/bash
WORKDIR=$(CURDIR)
EDK2_DIR=$(WORKDIR)/tools/edk2
KER_DIR=$(WORKDIR)/kernel

.PHONY: all
all: Loader kernel.elf

.PHONY: Loader
Loader:	
		WORKSPACE=$(EDK2_DIR) source $(EDK2_DIR)/edksetup.sh; \
		WORKSPACE=$(EDK2_DIR) build

.PHONY: kernel.elf
kernel.elf:
		make -C $(KER_DIR)





