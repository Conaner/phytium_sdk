# configs/bsp/e2000d-demo-board-bsp.ini : RTEMS BSP ini config
# configs/buildset/e2000d-demo-board-libbsd.ini : RTEMS LibBSD ini config
# configs/netconf/e2000d-demo-board-net.conf : RTEMS LibBSD network config
# configs/dts/e2000d-demo-board.dts: RTEMS Device tree config
# configs/dtb/e2000d-demo-board.c: RTEMS Device tree binary in array

e2000d_demo_aarch64_default:
	$(call get_rtems_bsp_default_config,$(RTEMS_SRC_DIR),aarch64,phytium_e2000d_demo)

e2000d_demo_aarch64_config:
	@echo "======Copy E2000D Demo BSP Parameters"
	@mkdir $(RTEMS_SRC_DIR)/bsps/aarch64/phytium/include/soc/e2000d -p
	@cp $(RTEMS_STANDALONE)/soc/pe220x//fparameters_comm.h $(RTEMS_SRC_DIR)/bsps/aarch64/phytium/include/soc/e2000d -f
	@cp $(RTEMS_STANDALONE)/soc/pe220x/pe2202/fparameters.h $(RTEMS_SRC_DIR)/bsps/aarch64/phytium/include/soc/e2000d -f

	@echo "======AARCH64 E2000D DEMO BSP"
	$(call build_rtems_devtree,e2000d-demo-board)
	$(call config_rtems_bsp,e2000d-demo-board-bsp.ini,$(RTEMS_SRC_DIR))

e2000d_demo_aarch64_bsp: clean_config e2000d_demo_aarch64_config build_aarch64_bsp

e2000d_demo_aarch64_libbsd:
	@echo "======AARCH64 E2000D DEMO LIBBSD"
	$(call build_rtems_libbsd,$(RTEMS_AARCH64_TOOL_PREFIX),$(RTEMS_LIBBSD_DIR),$(RTEMS_VERSION),aarch64,phytium_e2000d_demo,e2000d-demo-board-libbsd.ini)

e2000d_demo_aarch64_example:
	@echo "======AARCH64 E2000D DEMO Example"
	@source ./tools/env_e2000d_demo_aarch64.sh && cd $(RTEMS_EXAMPLE_DIR) && ./waf clean && ./waf

e2000d_demo_aarch64_clean: clean_bsp clean_libbsd
	@rm $(RTEMS_AARCH64_TOOL_PREFIX)/aarch64-rtems6/phytium_e2000d_demo -rf
	@rm $(RTEMS_EXAMPLE_DIR)/build/aarch64-rtems6-phytium_e2000d_demo -rf

e2000d_demo_aarch64_all: e2000d_demo_aarch64_bsp e2000d_demo_aarch64_libbsd

E2000D_DEMO_TEST_IMAGES :=$(RTEMS_SRC_DIR)/build/aarch64/phytium_e2000d_demo/testsuites/tmtests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_e2000d_demo/testsuites/fstests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_e2000d_demo/testsuites/libtests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_e2000d_demo/testsuites/psxtests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_e2000d_demo/testsuites/psxtmtests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_e2000d_demo/testsuites/smptests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_e2000d_demo/testsuites/validation \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_e2000d_demo/testsuites/unit \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_e2000d_demo/testsuites/samples

e2000d_demo_aarch64_test: aarch64_setup_tester
	$(call start_rtems_tester,phytium_e2000d_demo,$(RTEMS_AARCH64_TOOL_PREFIX),$(E2000D_DEMO_TEST_IMAGES),*.exe,phytium_e2000d_demo_test.log)