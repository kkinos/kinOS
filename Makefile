SHELL=/bin/bash
WORKDIR=$(CURDIR)
EDK2_DIR=$(WORKDIR)/tools/edk2
KER_DIR=$(WORKDIR)/kernel
KER_ELF=$(WORKDIR)/kernel/kernel.elf
LOA_EFI=$(EDK2_DIR)/Build/kinLoaderX64/DEBUG_CLANG38/X64/kinLoader.efi

.PHONY: all
all: build run

.PHONY: build
build: Loader kernel.elf app

.PHONY: Loader
Loader:	
		WORKSPACE=$(EDK2_DIR) source $(EDK2_DIR)/edksetup.sh; \
		WORKSPACE=$(EDK2_DIR) build

.PHONY: kernel.elf
kernel.elf:
		make -C $(KER_DIR)

.PHONY: app
app:
		./appbuild.sh

.PHONY: run
run: 
		KINOS_DIR=$(WORKDIR) $(WORKDIR)/tools/run_kinos.sh


