# configs/bsp/pd2308-demo-board-bsp.ini : RTEMS BSP ini config
# configs/buildset/pd2308-demo-board-libbsd.ini : RTEMS LibBSD ini config
# configs/netconf/pd2308-demo-board-net.conf : RTEMS LibBSD network config
# configs/dts/pd2308-demo-board.dts: RTEMS Device tree config
# configs/dtb/pd2308-demo-board.c: RTEMS Device tree binary in array


pd2308_demo_default:
	$(call get_rtems_bsp_default_config,$(RTEMS_SRC_DIR),aarch64,phytium_pd2308_demo)

pd2308_demo_config:
	@echo "======Copy PD2308 Demo BSP Parameters"
	@mkdir $(RTEMS_SRC_DIR)/bsps/aarch64/phytium/include/soc/pd2308 -p
	@cp $(RTEMS_STANDALONE)/soc/pd2308/fparameters.h $(RTEMS_SRC_DIR)/bsps/aarch64/phytium/include/soc/pd2308 -f

	@echo "======PD2308 Demo BSP"
	$(call build_rtems_devtree,pd2308-demo-board)
	$(call config_rtems_bsp,pd2308-demo-board-bsp.ini,$(RTEMS_SRC_DIR))

pd2308_demo_bsp: clean_config pd2308_demo_config build_aarch64_bsp

pd2308_demo_libbsd:
	@echo "======PD2308 Demo LIBBSD"
	$(call build_rtems_libbsd,$(RTEMS_AARCH64_TOOL_PREFIX),$(RTEMS_LIBBSD_DIR),$(RTEMS_VERSION),aarch64,phytium_pd2308_demo,pd2308-demo-board-libbsd.ini)

pd2308_demo_fio:
	@echo "======PD2308 Demo FIO"
	$(call build_rtems_fio,$(RTEMS_SDK_DIR)/third-party/fio,$(RTEMS_AARCH64_TOOL_PREFIX)/bin/aarch64-rtems6-,$(RTEMS_AARCH64_TOOL_PREFIX)/aarch64-rtems6/phytium_pd2308_demo)
	
pd2308_demo_example:
	@echo "======PD2308 Demo Example"
	@source ./tools/env_pd2308_demo.sh && cd $(RTEMS_EXAMPLE_DIR) && ./waf clean && ./waf

pd2308_demo_clean: clean_bsp clean_libbsd
	@rm $(RTEMS_AARCH64_TOOL_PREFIX)/aarch64-rtems6/phytium_pd2308_demo -rf
	@rm $(RTEMS_EXAMPLE_DIR)/build/aarch64-rtems6-phytium_pd2308_demo -rf

pd2308_demo_all: pd2308_demo_bsp pd2308_demo_libbsd

PD2308_DEMO_TEST_IMAGES :=$(RTEMS_SRC_DIR)/build/aarch64/phytium_pd2308_demo/testsuites/tmtests

pd2308_demo_test: aarch64_setup_tester
	$(call start_rtems_tester,phytium_pd2308_demo,$(RTEMS_AARCH64_TOOL_PREFIX),$(PD2308_DEMO_TEST_IMAGES),*.exe,phytium_pd2308_demo_test.log)