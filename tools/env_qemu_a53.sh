#!/bin/bash

echo "Setup RTEMS tools for QEMU Contrex-A53 ..."

export RTEMS_BSP=aarch64/xilinx_zynqmp_lp64_qemu
export RTEMS_MAKEFILE_PATH=$(realpath ./toolchain/aarch64-6/aarch64-rtems6/xilinx_zynqmp_lp64_qemu)
export RTEMS_TOOLCHAIN_PATH=$(realpath ./toolchain/aarch64-6)
export RTEMS_EXAMPLE_IMAGS_PATH=$(realpath ./examples/build/aarch64-rtems6-xilinx_zynqmp_lp64_qemu)
export RTEMS_TESTSUITE_IMAGES_PATH=$(realpath ./rtems/rtems/build/aarch64/xilinx_zynqmp_lp64_qemu/testsuites)
export RTEMS_BSD_TESTSUITE_IMAGES_PATH=$(realpath ./rtems/rtems-libbsd/build/aarch64-rtems6-xilinx_zynqmp_lp64_qemu-qemua53)

source ./tools/image_ops.sh