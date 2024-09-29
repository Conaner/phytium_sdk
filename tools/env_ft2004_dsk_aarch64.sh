#!/bin/bash

echo "Setup RTEMS tools for FT2000/4 DSK ..."

export RTEMS_BSP=aarch64/phytium_ft2004_dsk
export RTEMS_MAKEFILE_PATH=$(realpath ./toolchain/aarch64-6/aarch64-rtems6/phytium_ft2004_dsk)
export RTEMS_TOOLCHAIN_PATH=$(realpath ./toolchain/aarch64-6)
export RTEMS_TOOL_PATH_PREFIX=${RTEMS_TOOLCHAIN_PATH}/bin/aarch64-rtems6-
export RTEMS_EXAMPLE_IMAGS_PATH=$(realpath ./rtems/rtems-examples/build/aarch64/phytium_ft2004_dsk)
export RTEMS_TESTSUITE_IMAGES_PATH=$(realpath ./rtems/rtems/build/aarch64/phytium_ft2004_dsk/testsuites)
export RTEMS_BSD_TESTSUITE_IMAGES_PATH=$(realpath ./rtems/rtems-libbsd/build/aarch64-rtems6-phytium_ft2004_dsk-ft2004dsk)
export RTEMS_TFTP_PATH=$(realpath /mnt/d/tftpboot)

source ./tools/image_ops.sh