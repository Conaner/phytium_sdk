# configs/bsp/pd2408-testa-board-bsp.ini : RTEMS BSP ini config
# configs/buildset/pd2408-testa-board-libbsd.ini : RTEMS LibBSD ini config
# configs/netconf/pd2408-testa-board-net.conf : RTEMS LibBSD network config
# configs/dts/pd2408-testa-board.dts: RTEMS Device tree config
# configs/dtb/pd2408-testa-board.c: RTEMS Device tree binary in array

pd2408_testa_default:
	$(call get_rtems_bsp_default_config,$(RTEMS_SRC_DIR),aarch64,phytium_pd2408_testa)

pd2408_testa_config:
	@echo "======Copy PD2408 TestA BSP Parameters"
	@mkdir $(RTEMS_SRC_DIR)/bsps/aarch64/phytium/include/soc/pd2408 -p
	@cp $(RTEMS_STANDALONE)/soc/pd2408/fparameters.h $(RTEMS_SRC_DIR)/bsps/aarch64/phytium/include/soc/pd2408 -f

	@echo "======PD2408 TestA BSP"
	$(call build_rtems_devtree,pd2408-testa-board)
	$(call config_rtems_bsp,pd2408-testa-board-bsp.ini,$(RTEMS_SRC_DIR))

pd2408_testa_bsp: clean_config pd2408_testa_config build_aarch64_bsp

pd2408_testa_libbsd:
	@echo "======PD2408 TestA LIBBSD"
	$(call build_rtems_libbsd,$(RTEMS_AARCH64_TOOL_PREFIX),$(RTEMS_LIBBSD_DIR),$(RTEMS_VERSION),aarch64,phytium_pd2408_testa,pd2408-testa-board-libbsd.ini)

pd2408_testa_fio:
	@echo "======PD2408 TestA FIO"
	$(call build_rtems_fio,$(RTEMS_SDK_DIR)/third-party/fio,$(RTEMS_AARCH64_TOOL_PREFIX)/bin/aarch64-rtems6-,$(RTEMS_AARCH64_TOOL_PREFIX)/aarch64-rtems6/phytium_pd2408_testa)
	
pd2408_testa_example:
	@echo "======PD2408 TestA Example"
	@source ./tools/env_pd2408_testa.sh && cd $(RTEMS_EXAMPLE_DIR) && ./waf clean && ./waf

pd2408_testa_clean: clean_bsp clean_libbsd
	@rm $(RTEMS_AARCH64_TOOL_PREFIX)/aarch64-rtems6/phytium_pd2408_testa -rf
	@rm $(RTEMS_EXAMPLE_DIR)/build/aarch64-rtems6-phytium_pd2408_testa -rf

pd2408_testa_all: pd2408_testa_bsp pd2408_testa_libbsd

PD2408_TESTA_TEST_IMAGES :=$(RTEMS_SRC_DIR)/build/aarch64/phytium_pd2408_testa/testsuites/tmtests

pd2408_testa_test: aarch64_setup_tester
	$(call start_rtems_tester,phytium_e2000q_demo,$(RTEMS_AARCH64_TOOL_PREFIX),$(PD2408_TESTA_TEST_IMAGES),*.exe,phytium_pd2408_testa_test.log)