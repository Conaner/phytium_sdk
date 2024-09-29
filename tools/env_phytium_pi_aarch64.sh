#!/bin/bash

echo "Setup RTEMS tools for Phytium PI ..."

export RTEMS_BSP=aarch64/phytium_pi
export RTEMS_MAKEFILE_PATH=$(realpath ./toolchain/aarch64-6/aarch64-rtems6/phytium_pi)
export RTEMS_TOOLCHAIN_PATH=$(realpath ./toolchain/aarch64-6)
export RTEMS_TOOL_PATH_PREFIX=${RTEMS_TOOLCHAIN_PATH}/bin/aarch64-rtems6-
export RTEMS_EXAMPLE_IMAGS_PATH=$(realpath ./rtems/rtems-examples/build/aarch64-rtems6-phytium_pi)
export RTEMS_TESTSUITE_IMAGES_PATH=$(realpath ./rtems/rtems/build/aarch64/phytium_pi/testsuites)
export RTEMS_BSD_TESTSUITE_IMAGES_PATH=$(realpath ./rtems/rtems-libbsd/build/aarch64-rtems6-phytium_pi-phytiumpi)
export RTEMS_TFTP_PATH=$(realpath /mnt/d/tftpboot)

source ./tools/image_ops.sh