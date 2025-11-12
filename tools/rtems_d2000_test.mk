# configs/bsp/d2000-test-board-bsp.ini : RTEMS BSP ini config
# configs/buildset/d2000-test-board-libbsd.ini : RTEMS LibBSD ini config
# configs/netconf/d2000-test-board-net.conf : RTEMS LibBSD network config
# configs/dts/d2000-test-board.dts: RTEMS Device tree config
# configs/dtb/d2000-test-board.c: RTEMS Device tree binary in array

d2000_test_aarch64_default:
	$(call get_rtems_bsp_default_config,$(RTEMS_SRC_DIR),aarch64,phytium_d2000_test)

d2000_test_aarch64_config:
	@echo "======Copy D2000 Test BSP Parameters"
	@mkdir $(RTEMS_SRC_DIR)/bsps/aarch64/phytium/include/soc/d2000 -p
	@cp $(RTEMS_STANDALONE)/soc/pd2008/fparameters.h $(RTEMS_SRC_DIR)/bsps/aarch64/phytium/include/soc/d2000 -f

	@echo "======AARCH64 D2000 Test BSP"
	$(call build_rtems_devtree,d2000-test-board)
	$(call config_rtems_bsp,d2000-test-board-bsp.ini,$(RTEMS_SRC_DIR))

d2000_test_aarch64_bsp: clean_config d2000_test_aarch64_config build_aarch64_bsp

d2000_test_aarch64_libbsd:
	@echo "======AARCH64 D2000 Test LIBBSD"
	$(call build_rtems_libbsd,$(RTEMS_AARCH64_TOOL_PREFIX),$(RTEMS_LIBBSD_DIR),$(RTEMS_VERSION),aarch64,phytium_d2000_test,d2000-test-board-libbsd.ini)

d2000_test_aarch64_example:
	@echo "======AARCH64 D2000 Test Example"
	@source ./tools/env_d2000_test_aarch64.sh && cd $(RTEMS_EXAMPLE_DIR) && ./waf clean && ./waf

d2000_test_aarch64_clean: clean_bsp clean_libbsd
	@rm $(RTEMS_AARCH64_TOOL_PREFIX)/aarch64-rtems6/phytium_d2000_test -rf
	@rm $(RTEMS_EXAMPLE_DIR)/build/aarch64-rtems6-phytium_d2000_test -rf

d2000_test_aarch64_all: d2000_test_aarch64_bsp d2000_test_aarch64_libbsd

D2000_TEST_IMAGES :=$(RTEMS_SRC_DIR)/build/aarch64/phytium_d2000_test/testsuites/tmtests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_d2000_test/testsuites/fstests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_d2000_test/testsuites/libtests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_d2000_test/testsuites/psxtests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_d2000_test/testsuites/psxtmtests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_d2000_test/testsuites/smptests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_d2000_test/testsuites/validation \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_d2000_test/testsuites/unit \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_d2000_test/testsuites/samples

d2000_aarch64_test: aarch64_setup_tester
	$(call start_rtems_tester,phytium_d2000_test,$(RTEMS_AARCH64_TOOL_PREFIX),$(D2000_TEST_IMAGES),*.exe,phytium_d2000_test.log)