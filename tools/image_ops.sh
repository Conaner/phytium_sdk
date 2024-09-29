#!/bin/bash

export PATH=${RTEMS_TOOLCHAIN_PATH}/bin:${PATH}

source ./tools/copy_image_files.sh
source ./tools/list_image_files.sh

copy_all_images() {

    if [ -d ${RTEMS_EXAMPLE_IMAGS_PATH} ]; then
        copy_image_files ${RTEMS_EXAMPLE_IMAGS_PATH} ${RTEMS_TFTP_PATH}
    fi

    if [ -d ${RTEMS_TESTSUITE_IMAGES_PATH} ]; then
        copy_image_files ${RTEMS_TESTSUITE_IMAGES_PATH} ${RTEMS_TFTP_PATH}
    fi

    if [ -d ${RTEMS_BSD_TESTSUITE_IMAGES_PATH} ]; then
        copy_image_files ${RTEMS_BSD_TESTSUITE_IMAGES_PATH} ${RTEMS_TFTP_PATH}
    fi
}

list_images() {
    if [ "$#" -ne 1 ]; then
        echo "Usage: list_images type"
        echo "      example:  images built from /examples"
        echo "      testsuite:  images built from /rtems/testsuite"
        echo "      bsdtestsuite:  images built from /rtems-libbsd/testsuite"
        return 1
    fi

    local image_type="$1"

    case "$image_type" in
        example*)
            list_image_files ${RTEMS_EXAMPLE_IMAGS_PATH}
            ;;
        testsuite*)
            list_image_files ${RTEMS_TESTSUITE_IMAGES_PATH}
            ;;
        bsdtestsuite*)
            list_image_files ${RTEMS_BSD_TESTSUITE_IMAGES_PATH}
            ;;
        *)
            echo "Invalid type: $image_type"
            echo "Valid types are: example, testsuite, bsdtestsuite"
            return 1
            ;;
    esac

    list_image_files ${RTEMS_IMAGE_PATH}
}

copy_image() {
    if [ "$#" -ne 2 ]; then
        echo "Usage: copy_image image_name type"
        echo "      example:  images built from /examples"
        echo "      testsuite:  images built from /rtems/testsuite"
        echo "      bsdtestsuite:  images built from /rtems-libbsd/testsuite"
        return 1
    fi

    local image_name="$1"
    local image_type="$2"

    case "$image_type" in
        example*)
            copy_files_by_name ${RTEMS_EXAMPLE_IMAGS_PATH} $image_name ${RTEMS_TFTP_PATH}
            ;;
        testsuite*)
            copy_files_by_name ${RTEMS_TESTSUITE_IMAGES_PATH} $image_name ${RTEMS_TFTP_PATH}
            ;;
        bsdtestsuite*)
            copy_files_by_name ${RTEMS_BSD_TESTSUITE_IMAGES_PATH} $image_name ${RTEMS_TFTP_PATH}
            ;;
        *)
            echo "Invalid type: $image_type"
            echo "Valid types are: example, testsuite, bsdtestsuite"
            return 1
            ;;
    esac

    cp ${RTEMS_TFTP_PATH}/$image_name ${RTEMS_TFTP_PATH}/rtems.exe -f
    echo "Copy "$image_name" > rtems.exe"
    aarch64-rtems6-objcopy -O binary ${RTEMS_TFTP_PATH}/rtems.exe ${RTEMS_TFTP_PATH}/rtems.bin
    echo "Objcopy rtems.exe > rtems.bin"
    # aarch64-rtems6-objdump -D ${RTEMS_TFTP_PATH}/rtems.exe > ${RTEMS_TFTP_PATH}/rtems.asm
    # echo "Objdump rtems.exe > rtems.asm"
}

export -f copy_all_images
export -f copy_image

echo "RTEMS Tool: ${RTEMS_TOOLCHAIN_PATH}"
echo "RTEMS BSP: ${RTEMS_MAKEFILE_PATH}"

cd rtems/rtems-examples && ./waf configure --rtems=${RTEMS_TOOLCHAIN_PATH} \
                --rtems-tools=${RTEMS_TOOLCHAIN_PATH} \
                --rtems-bsps=${RTEMS_BSP} && cd ../../

echo "Environment setup done !!!"
echo "  ./waf to build all examples with wscript"
echo "  make all to build all examples with makefile"
echo "  cd to specific example path and ./waf or make to build"
