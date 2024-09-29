# configs/bsp/phytium-pi-bsp.ini : RTEMS BSP ini config
# configs/buildset/phytium-pi-libbsd.ini : RTEMS LibBSD ini config
# configs/netconf/phytium-pi-net.conf : RTEMS LibBSD network config
# configs/dts/phytium-pi.dts: RTEMS Device tree config
# configs/dtb/phytium-pi.c: RTEMS Device tree binary in array

phytium_pi_aarch64_default:
	$(call get_rtems_bsp_default_config,$(RTEMS_SRC_DIR),aarch64,phytium_pi)

phytium_pi_aarch64_config:
	@echo "======Copy Phytium PI BSP Parameters"
	@mkdir $(RTEMS_SRC_DIR)/bsps/aarch64/phytium/include/soc/phytiumpi -p
	@cp $(RTEMS_STANDALONE)/soc/phytiumpi/fparameters.h $(RTEMS_SRC_DIR)/bsps/aarch64/phytium/include/soc/phytiumpi -f
	@cp $(RTEMS_STANDALONE)/soc/phytiumpi/fparameters_comm.h $(RTEMS_SRC_DIR)/bsps/aarch64/phytium/include/soc/phytiumpi -f

	@echo "======AARCH64 Phytium PI BSP"
	$(call build_rtems_devtree,phytium-pi)
	$(call config_rtems_bsp,phytium-pi-bsp.ini,$(RTEMS_SRC_DIR))

phytium_pi_aarch64_bsp: clean_config phytium_pi_aarch64_config build_bsp

phytium_pi_aarch64_libbsd:
	@echo "======AARCH64 Phytium PI LIBBSD"
	$(call build_rtems_libbsd,$(RTEMS_AARCH64_TOOL_PREFIX),$(RTEMS_LIBBSD_DIR),$(RTEMS_VERSION),aarch64,phytium_pi,phytium-pi-libbsd.ini)

phytium_pi_aarch64_example:
	@echo "======AARCH64 Phytium PI Example"
	@source ./tools/env_phytium_pi_aarch64.sh && cd $(RTEMS_EXAMPLE_DIR) && ./waf clean && ./waf

phytium_pi_aarch64_clean:
	@rm $(RTEMS_AARCH64_TOOL_PREFIX)/aarch64-rtems6/phytium_pi -rf
	@rm $(RTEMS_EXAMPLE_DIR)/build/aarch64-rtems6-phytium_pi -rf

phytium_pi_aarch64_all: phytium_pi_aarch64_bsp phytium_pi_aarch64_libbsd

PHYTIUM_PI_TEST_IMAGES :=$(RTEMS_SRC_DIR)/build/aarch64/phytium_pi/testsuites/tmtests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_pi/testsuites/fstests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_pi/testsuites/libtests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_pi/testsuites/psxtests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_pi/testsuites/psxtmtests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_pi/testsuites/smptests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_pi/testsuites/validation \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_pi/testsuites/unit \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_pi/testsuites/samples

phytium_pi_aarch64_test: aarch64_setup_tester
	$(call start_rtems_tester,phytium_pi,$(RTEMS_AARCH64_TOOL_PREFIX),$(PHYTIUM_PI_TEST_IMAGES),*.exe,phytium_pi_test.log)