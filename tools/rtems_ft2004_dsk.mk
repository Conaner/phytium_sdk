# configs/bsp/ft2004-dsk-board-bsp.ini : RTEMS BSP ini config
# configs/buildset/ft2004-dsk-board-libbsd.ini : RTEMS LibBSD ini config
# configs/netconf/ft2004-dsk-board-net.conf : RTEMS LibBSD network config
# configs/dts/ft2004-dsk-board.dts: RTEMS Device tree config
# configs/dtb/ft2004-dsk-board.c: RTEMS Device tree binary in array

ft2004_dsk_aarch64_default:
	$(call get_rtems_bsp_default_config,$(RTEMS_SRC_DIR),aarch64,phytium_ft2004_dsk)

ft2004_dsk_aarch64_config:
	@echo "======Copy FT2000/4 Test BSP Parameters"
	@mkdir $(RTEMS_SRC_DIR)/bsps/aarch64/phytium/include/soc/ft2004 -p
	@cp $(RTEMS_STANDALONE)/soc/ft2004/fparameters.h $(RTEMS_SRC_DIR)/bsps/aarch64/phytium/include/soc/ft2004 -f

	@echo "======AARCH64 FT2000/4 Test BSP"
	$(call build_rtems_devtree,ft2004-dsk-board)
	$(call config_rtems_bsp,ft2004-dsk-board-bsp.ini,$(RTEMS_SRC_DIR))

ft2004_dsk_aarch64_bsp: clean_config ft2004_dsk_aarch64_config build_bsp

ft2004_dsk_aarch64_libbsd:
	@echo "======AARCH64 FT2000/4 Test LIBBSD"
	$(call build_rtems_libbsd,$(RTEMS_AARCH64_TOOL_PREFIX),$(RTEMS_LIBBSD_DIR),$(RTEMS_VERSION),aarch64,phytium_ft2004_dsk,ft2004-dsk-board-libbsd.ini)

ft2004_dsk_aarch64_example:
	@echo "======AARCH64 FT2000/4 Test Example"
	@source ./tools/env_ft2004_dsk_aarch64.sh && cd $(RTEMS_EXAMPLE_DIR) && ./waf clean && ./waf

ft2004_dsk_aarch64_clean:
	@rm $(RTEMS_AARCH64_TOOL_PREFIX)/aarch64-rtems6/phytium_ft2004_dsk -rf
	@rm $(RTEMS_EXAMPLE_DIR)/build/aarch64-rtems6-phytium_ft2004_dsk -rf

ft2004_dsk_aarch64_all: ft2004_dsk_aarch64_bsp ft2004_dsk_aarch64_libbsd

FT2004_DSK_TEST_IMAGES :=$(RTEMS_SRC_DIR)/build/aarch64/phytium_ft2004_dsk/testsuites/tmtests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_ft2004_dsk/testsuites/fstests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_ft2004_dsk/testsuites/libtests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_ft2004_dsk/testsuites/psxtests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_ft2004_dsk/testsuites/psxtmtests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_ft2004_dsk/testsuites/smptests \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_ft2004_dsk/testsuites/validation \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_ft2004_dsk/testsuites/unit \
$(RTEMS_SRC_DIR)/build/aarch64/phytium_ft2004_dsk/testsuites/samples

ft2004_dsk_aarch64_test: aarch64_setup_tester
	$(call start_rtems_tester,phytium_ft2004_dsk,$(RTEMS_AARCH64_TOOL_PREFIX),$(FT2004_DSK_TEST_IMAGES),*.exe,phytium_ft2004_dsk_test.log)