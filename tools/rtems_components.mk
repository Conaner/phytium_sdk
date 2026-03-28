SHELL := /bin/bash

# rule out Windows path in WSL 
export PATH :=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/usr/lib/wsl/lib:/snap/bin

RTEMS_STANDALONE := $(RTEMS_SDK_DIR)/standalone
RTEMS_RSB_DIR := $(RTEMS_SDK_DIR)/rtems/rtems-source-builder
RTEMS_SRC_DIR := $(RTEMS_SDK_DIR)/rtems/rtems
RTEMS_BSP_INI_DIR := $(RTEMS_SDK_DIR)/configs/bsp
RTEMS_DTS_DIR := $(RTEMS_SDK_DIR)/configs/dts
RTEMS_DTB_DIR := $(RTEMS_SDK_DIR)/configs/dtb
RTEMS_TST_INT_DIR := $(RTEMS_SDK_DIR)/configs/testing
RTEMS_LIBBSD_DIR := $(RTEMS_SDK_DIR)/rtems/rtems-libbsd
RTEMS_LIBBSD_BUILDSET := $(RTEMS_SDK_DIR)/configs/buildset
RTEMS_EXAMPLE_DIR := $(RTEMS_SDK_DIR)/rtems/rtems-examples
RTEMS_VERSION := 6

# void build_rtems_toolchain
#    $(1) == toolchain install dir
#    $(2) == rtems source builder dir
#    $(3) == rtems version
#    $(4) == rtems aarch
define build_rtems_toolchain
	@##H## Build the toolchain, just run in the first time.
	@mkdir $(1) -p
	@rm -rf $(2)/rtems/build
	@cd $(2) && ./source-builder/sb-check
	# build gcc toolchain
	@cd $(2)/rtems && ../source-builder/sb-set-builder \
		--log=$(1)/rtems-$(3)-toolchain-$(4).log \
		--prefix=$(1) \
		--without-rtems \
		$(3)/rtems-$(4)
	# build device-tree compiler
	@cd $(2)/rtems && ../source-builder/sb-set-builder \
		--log=$(1)/rtems-$(3)-dtc-$(4).log \
		--prefix=$(1) \
		devel/dtc
endef

# void get_rtems_bsp_default_config
#    $(1) == rtems source dir
#    $(2) == rtems bsp arch
#    $(3) == rtems bsp board
define get_rtems_bsp_default_config
	@##H## Get the BSP default configs.
	cd $(1) && ./waf bspdefaults --rtems-bsps=$(2)/$(3) > config.ini
	cd $(1) && sed -i \
		-e "s|RTEMS_POSIX_API = False|RTEMS_POSIX_API = True|" \
		-e "s|BUILD_SAMPLES = False|BUILD_SAMPLES = True|" \
		-e "s|RTEMS_SMP = False|RTEMS_SMP = True|" \
		-e "s|BUILD_SMPTESTS = False|BUILD_SMPTESTS = True|" \
		-e "s|BUILD_TESTS = False|BUILD_TESTS = True|" \
		-e "s|BUILD_ADATESTS = False|BUILD_ADATESTS = True|" \
		-e "s|BUILD_BENCHMARKS = False|BUILD_BENCHMARKS = True|" \
		-e "s|BUILD_FSTESTS = False|BUILD_FSTESTS = True|" \
		-e "s|BUILD_LIBTESTS = False|BUILD_LIBTESTS = True|" \
		-e "s|BUILD_MPTESTS = False|BUILD_MPTESTS = False|" \
		-e "s|BUILD_PSXTESTS = False|BUILD_PSXTESTS = True|" \
		-e "s|BUILD_PSXTMTESTS = False|BUILD_PSXTMTESTS = True|" \
		-e "s|BUILD_RHEALSTONE = False|BUILD_RHEALSTONE = True|" \
		-e "s|BUILD_SMPTESTS = False|BUILD_SMPTESTS = False|" \
		-e "s|BUILD_TMTESTS = False|BUILD_TMTESTS = True|" \
		-e "s|BUILD_UNITTESTS = False|BUILD_UNITTESTS = True|" \
		-e "s|BUILD_VALIDATIONTESTS = False|BUILD_VALIDATIONTESTS = True|" \
		-e "s|AARCH64_FLUSH_CACHE_BOOT_UP = False|AARCH64_FLUSH_CACHE_BOOT_UP = True|" \
		config.ini
	cd $(1) && cat config.ini
endef

# void config_rtems_bsp
#    $(1) == rtems bsp config
#    $(2) == rtems source dir
define config_rtems_bsp
	@##H## Build the BSP.
	cd $(2) && cat $(RTEMS_BSP_INI_DIR)/$(1) >> ./config.ini && echo -e "\n" >> ./config.ini  
endef

# void build_rtems_bsp
#    $(1) == bsp install dir
#    $(2) == rtems source dir
define build_rtems_bsp
	cd $(2) && ./waf configure \
		--prefix=$(1)
	cd $(2) && ./waf
	cd $(2) && ./waf install
endef

# void build_rtems_libbsd
#    $(1) == libbsd install dir
#    $(2) == libbsd source dir
#    $(3) == rtems version
#    $(4) == rtems bsp arch
#    $(5) == rtems bsp board
#    $(6) == rtems libbsd config
#    $(7) == rtems libbsd netconfig
define build_rtems_libbsd
	@##H## Build the libbsd.
	cd $(2) && ./waf configure \
		--prefix=$(1) \
		--rtems-tools=$(1) \
		--rtems-bsps=$(4)/$(5) \
		--enable-warnings \
		--optimization=g \
		--rtems-version=$(3) \
		--buildset=$(RTEMS_LIBBSD_BUILDSET)/$(6)
	cd $(2) && ./waf
	cd $(2) && ./waf install
endef

# void build_rtems_devtree
#   $(1) == target bsp name
define build_rtems_devtree
	@##H## Build the device tree.
	mkdir -p $(RTEMS_DTB_DIR)
	dtc -@ -I dts -O dtb -o $(RTEMS_DTB_DIR)/$(1).dtb $(RTEMS_DTS_DIR)/$(1).dts
	rtems-bin2c -N phytium_dtb $(RTEMS_DTB_DIR)/$(1).dtb  $(RTEMS_DTB_DIR)/$(1).c
endef

# void build_rtems_fio
#   $(1) == fio path
#   $(2) == toolchain prefix
#   $(3) == rtems bsp path
define build_rtems_fio
	@##H## Build the fio.
	cd $(1) && ./configure --cc=$(2)gcc \
						   --disable-optimizations \
						   --extra-cflags=-O3 \
						   --disable-pmem
	cd $(1) && make fio CROSS_COMPILE=$(2) \
						RTEMS_MAKEFILE_PATH=$(3) \
						V=1
	cd $(1) && $(2)objcopy -O binary fio fio.bin
	cd $(1) && cp fio.bin /home/lyj/sdk_test/tftpboot/rtems.bin -f
endef

# void start_rtems_tester
#   $(1) == target bsp name
#   $(2) == rtems tools
#   $(3) == image path
#   $(4) == image filter
#   $(5) == output log
define start_rtems_tester
	@##H## Start the RTEMS tester.
	rtems-test \
		--rtems-bsps=$(1) \
		--log=$(5) --log-mode=all --debug-trace=console \
		--rtems-tools=$(2) \
		--filter=$(4) \
		--timeout=250 \
		$(3)
endef

install:
	python3 ./install.py

# RTEMS toolchain, build from source
RTEMS_AARCH64_TOOL_PREFIX := $(RTEMS_SDK_DIR)/toolchain/aarch64-$(RTEMS_VERSION)
aarch64_rtems_toolchain:
	@echo "======AARCH64 RTEMS toolchain"
	$(call build_rtems_toolchain,$(RTEMS_AARCH64_TOOL_PREFIX),$(RTEMS_RSB_DIR),$(RTEMS_VERSION),aarch64)

aarch64_setup_tester:
	@echo "======AARCH64 RTEMS tester setup"
	@cp $(RTEMS_TST_INT_DIR)/rtems-bsps-aarch64.ini $(RTEMS_AARCH64_TOOL_PREFIX)/share/rtems/config -f
	@cp $(RTEMS_TST_INT_DIR)/rtems-bsps-tiers.ini $(RTEMS_AARCH64_TOOL_PREFIX)/share/rtems/config -f
	@cp $(RTEMS_TST_INT_DIR)/bsps/*.ini $(RTEMS_AARCH64_TOOL_PREFIX)/share/rtems/tester/rtems/testing/bsps -f

help_tester:
	rtems-test --help

RTEMS_ARM_TOOL_PREFIX := $(RTEMS_SDK_DIR)/toolchain/arm-$(RTEMS_VERSION)
arm_rtems_toolchain:
	@echo "======AARCH32 RTEMS toolchain"
	$(call build_rtems_toolchain,$(RTEMS_ARM_TOOL_PREFIX),$(RTEMS_RSB_DIR),$(RTEMS_VERSION),arm)

export RTEMS_TOOL_PREFIX
export PATH := $(RTEMS_AARCH64_TOOL_PREFIX)/bin:$(RTEMS_ARM_TOOL_PREFIX)/bin:$(PATH)

list_bsp:
	@cd $(RTEMS_SRC_DIR) && ./waf bsplist

# will clean all rtems builds, toolchain, bsp and libbsd
distclean_rtems:
	@rm $(RTEMS_RSB_DIR)/toolchain -rf

build_aarch64_bsp:
	$(call build_rtems_bsp,$(RTEMS_AARCH64_TOOL_PREFIX),$(RTEMS_SRC_DIR))

build_arm_bsp:
	$(call build_rtems_bsp,$(RTEMS_ARM_TOOL_PREFIX),$(RTEMS_SRC_DIR))

clean_config:
	@echo -n > $(RTEMS_SRC_DIR)/config.ini

clean_bsp:
	@rm $(RTEMS_SRC_DIR)/build -rf
	@rm $(RTEMS_SRC_DIR)/.lock-waf* -f

clean_libbsd:
	@rm $(RTEMS_LIBBSD_DIR)/build -rf
	@rm $(RTEMS_LIBBSD_DIR)/.lock-waf* -f

clean_examples:
	@rm $(RTEMS_EXAMPLE_DIR)/build -rf
	@rm $(RTEMS_EXAMPLE_DIR)/.lock-waf* -f

include $(RTEMS_SDK_DIR)/tools/rtems_pd2408_testa.mk
include $(RTEMS_SDK_DIR)/tools/rtems_pd2308_demo.mk
include $(RTEMS_SDK_DIR)/tools/rtems_e2000d_demo.mk
include $(RTEMS_SDK_DIR)/tools/rtems_e2000q_demo.mk
include $(RTEMS_SDK_DIR)/tools/rtems_phytium_pi.mk
include $(RTEMS_SDK_DIR)/tools/rtems_d2000_test.mk
include $(RTEMS_SDK_DIR)/tools/rtems_ft2004_dsk.mk

clean_all: \
	e2000d_demo_aarch64_clean \
	e2000q_demo_aarch64_clean \
	phytium_pi_aarch64_clean \
	d2000_test_aarch64_clean \
	ft2004_dsk_aarch64_clean \
	pd2308_demo_clean \
	pd2408_testa_clean

all_bsp: \
	clean_config \
	e2000d_demo_aarch64_config \
	e2000q_demo_aarch64_config \
	phytium_pi_aarch64_config \
	d2000_test_aarch64_config \
	ft2004_dsk_aarch64_config \
	pd2308_demo_config \
	pd2408_testa_config \
	build_aarch64_bsp

all_libbsd: \
	e2000d_demo_aarch64_libbsd \
	e2000q_demo_aarch64_libbsd \
	phytium_pi_aarch64_libbsd \
	d2000_test_aarch64_libbsd \
	ft2004_dsk_aarch64_libbsd \
	pd2308_demo_libbsd \
	pd2408_testa_libbsd

all: all_bsp all_libbsd