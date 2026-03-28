#!/bin/bash

echo "Setup RTEMS tools for D2000 Test ..."

export RTEMS_BSP=aarch64/phytium_d2000_test
export RTEMS_MAKEFILE_PATH=$(realpath ./toolchain/aarch64-6/aarch64-rtems6/phytium_d2000_test)
export RTEMS_TOOLCHAIN_PATH=$(realpath ./toolchain/aarch64-6)
export RTEMS_TOOL_PATH_PREFIX=${RTEMS_TOOLCHAIN_PATH}/bin/aarch64-rtems6-
export RTEMS_EXAMPLE_IMAGS_PATH=$(realpath ./rtems/rtems-examples/build/aarch64/phytium_d2000_test)
export RTEMS_TESTSUITE_IMAGES_PATH=$(realpath ./rtems/rtems/build/aarch64/phytium_d2000_test/testsuites)
export RTEMS_BSD_TESTSUITE_IMAGES_PATH=$(realpath ./rtems/rtems-libbsd/build/aarch64-rtems6-phytium_d2000_test-d2000test)
export RTEMS_TFTP_PATH=$(realpath /home/lyj/sdk_test/tftpboot)

source ./tools/image_ops.sh