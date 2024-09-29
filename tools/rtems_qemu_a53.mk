# configs/bsp/qemu-a53-bsp.ini : RTEMS BSP ini config
# configs/buildset/qemu-a53-libbsd.ini : RTEMS LibBSD ini config
# configs/netconf/qemu-a53-net.conf : RTEMS LibBSD network config


qemu_a53_default:
	$(call get_rtems_bsp_default_config,$(RTEMS_SRC_DIR),aarch64,xilinx_zynqmp_lp64_qemu)

qemu_a53_config:
	@echo "======AARCH64 QEMU-A53 BSP"
	$(call config_rtems_bsp,qemu-a53-bsp.ini,$(RTEMS_SRC_DIR))

qemu_a53_bsp: clean_config qemu_a53_config build_bsp

qemu_a53_libbsd:
	@echo "======AARCH64 QEMU-A53 LIBBSD"
	$(call build_rtems_libbsd,$(RTEMS_AARCH64_TOOL_PREFIX),$(RTEMS_LIBBSD_DIR),$(RTEMS_VERSION),aarch64,xilinx_zynqmp_lp64_qemu,qemu-a53-libbsd.ini)

qemu_a53_clean:
	@rm $(RTEMS_AARCH64_TOOL_PREFIX)/aarch64-rtems6/xilinx_zynqmp_lp64_qemu -rf
	@rm $(RTEMS_EXAMPLE_DIR)/build/aarch64-rtems6-xilinx_zynqmp_lp64_qemu -rf

qemu_a53_examples:
	@echo "======AARCH64 QEMU-A53 Example"
	@source ./tools/env_qemu_a53.sh && ./waf clean && ./waf

qemu_a53_test:
	@$(RTEMS_AARCH64_TOOL_PREFIX)/bin/rtems-test \
		--log=$(RTEMS_AARCH64_TOOL_PREFIX)/log_qemu \
		--rtems-bsp=xilinx_zynqmp_lp64_qemu \
		rtems/rtems/build/aarch64/xilinx_zynqmp_lp64_qemu/testsuites/benchmarks \
		rtems/rtems/build/aarch64/xilinx_zynqmp_lp64_qemu/testsuites/fstests \
		rtems/rtems/build/aarch64/xilinx_zynqmp_lp64_qemu/testsuites/psxtests \
		rtems/rtems/build/aarch64/xilinx_zynqmp_lp64_qemu/testsuites/psxtmtests \
		rtems/rtems/build/aarch64/xilinx_zynqmp_lp64_qemu/testsuites/rhealstone \
		rtems/rtems/build/aarch64/xilinx_zynqmp_lp64_qemu/testsuites/tmtests \
		rtems/rtems/build/aarch64/xilinx_zynqmp_lp64_qemu/testsuites/sample \
		rtems/rtems/build/aarch64/xilinx_zynqmp_lp64_qemu/testsuites/libtests \
		rtems/rtems/build/aarch64/xilinx_zynqmp_lp64_qemu/testsuites/support \
		rtems/rtems/build/aarch64/xilinx_zynqmp_lp64_qemu/testsuites/unit \
		rtems/rtems/build/aarch64/xilinx_zynqmp_lp64_qemu/testsuites/validation

qemu_a53_run:
	@qemu-system-aarch64 -no-reboot -nographic -serial mon:stdio \
		-machine xlnx-zcu102 -m 4096 \
		-kernel rtems/rtems/build/aarch64/xilinx_zynqmp_lp64_qemu/testsuites/smptests/smp01.exe