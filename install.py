#!/usr/bin/env python3

import os
import platform
import re

### platform constant
platform_tags = ["Linux_X86_64" "Linux_AARCH64" "Windows_x64"]
linux_x64 = 0
linux_aarch64 = 1
windows_x64 = 2
toolchain_src_package = "toolchain_sources.tar.xz"

# STEP 1: Check environment
if (platform.system() == 'Linux' ) and (platform.machine() == 'x86_64'):
    install_platform = linux_x64
elif (platform.system() == 'Linux' ) and (platform.machine() == 'aarch64'): # Arm64 computer
    install_platform = linux_aarch64
elif (platform.system() == 'Windows') and (platform.machine() == 'AMD64'):
    install_platform = windows_x64
else:
    print("Platform not support !!! ")
    exit()

# get absoulte path current pwd to install sdk
install_path, install_script = os.path.split(os.path.abspath(__file__))
curr_path = os.getcwd()
rtems_sdk_path = ''

# in case user call this script not from current path
if (curr_path != install_path):
    print("Please cd to install script path first !!!")
    exit()

# get absolute path of sdk install dir
rtems_sdk_path = install_path
print("RTEMS SDK at {}".format(rtems_sdk_path))

# STEP 2: Add standalone sdk
standalone_sdk_v="e3ce67ce6748bba3e1a432b0c61572977483eca9"
if (install_platform == windows_x64):
    standalone_path=rtems_sdk_path  + '\\standalone'
else:
    standalone_path=rtems_sdk_path  + '/standalone'
standalone_branch="master"
standalone_remote="https://gitee.com/phytium_embedded/phytium-standalone-sdk.git"

if not os.path.exists(standalone_path):
    current_path = os.getcwd()

    os.system("git clone -b {} {} {}".format(standalone_branch, standalone_remote,standalone_path))
    os.chdir(standalone_path)# 切换工作路径至standalone 路径
    os.system("git config core.sparsecheckout true")
    os.system("git config advice.detachedHead false")
    os.system("git sparse-checkout init")

    # 适配 windows 环境，路径不兼容
    os.system("git sparse-checkout set /arch \
                                       /board \
                                       !/board/port \
                                       /common \
                                       /drivers \
                                       !/drivers/port \
                                       /standalone.mk \
                                       /lib \
                                       /doc \
                                       /tools \
                                       /soc")

    #os.system("git checkout {}".format(standalone_sdk_v))
    print('Standalone sdk download is succeed')
else:
    print('Standalone sdk is exist')
    pass

## STEP 3: add RTEMS source code
if (install_platform == windows_x64):
    rtems_path=rtems_sdk_path  + '\\rtems\\rtems'
else:
    rtems_path=rtems_sdk_path  + '/rtems/rtems'
rtems_remote = "https://gitee.com/phytium_embedded/rtems.git"
rtems_commit = "6b0d1c4d90eade5d5707898ea1f83fddad68e3cb"
rtems_branch = "main"

if not os.path.exists(rtems_path):
    current_path = os.getcwd()

    os.system("git clone -b {} {} {}".format(rtems_branch, rtems_remote, rtems_path))
    os.chdir(rtems_path)# 切换工作路径至 rtems 路径
    os.system("git checkout {}".format(rtems_commit))
    print('RTEMS download is succeed')
else:
    print('RTEMS exists')
    pass

## STEP 4: add RTEMS libbsd source code
if (install_platform == windows_x64):
    rtems_libbsd_path=rtems_sdk_path  + '\\rtems\\rtems-libbsd'
else:
    rtems_libbsd_path=rtems_sdk_path  + '/rtems/rtems-libbsd'
rtems_libbsd_remote = "https://gitee.com/phytium_embedded/rtems-libbsd.git"
rtems_libbsd_commit = "2f39be5b39aa16ece3445004810ae749335a8e62"
rtems_libbsd_branch = "main"

if not os.path.exists(rtems_libbsd_path):
    current_path = os.getcwd()

    os.system("git clone -b {} {} {}".format(rtems_libbsd_branch, rtems_libbsd_remote, rtems_libbsd_path))
    os.chdir(rtems_libbsd_path)# 切换工作路径至 rtems 路径
    os.system("git checkout {}".format(rtems_libbsd_commit))
    os.system("git submodule init rtems_waf && git submodule update rtems_waf")
    print('RTEMS-LibBSD download is succeed')
else:
    print('RTEMS-LibBSD exists')
    pass

## STEP 5: add RTEMS source builder code
if (install_platform == windows_x64):
    rtems_rsb_path=rtems_sdk_path  + '\\rtems\\rtems-source-builder'
else:
    rtems_rsb_path=rtems_sdk_path  + '/rtems/rtems-source-builder'
rtems_rsb_remote = "https://gitlab.rtems.org/rtems/tools/rtems-source-builder.git"
rtems_rsb_commit = "46a5bfdc3fc95d503a63f9bee515e73d9198aa71"
rtems_rsb_branch = "main"

if not os.path.exists(rtems_rsb_path):
    current_path = os.getcwd()

    os.system("git clone -b {} {} {}".format(rtems_rsb_branch, rtems_rsb_remote, rtems_rsb_path))
    os.chdir(rtems_rsb_path)# 切换工作路径至 rtems 路径
    os.system("git checkout {}".format(rtems_rsb_commit))
    print('RTEMS-Source-Builder download is succeed')
else:
    print('RTEMS-Source-Builder exists')
    pass

## STEP 6: add RTEMS examples
if (install_platform == windows_x64):
    rtems_eg_path=rtems_sdk_path  + '\\rtems\\rtems-examples'
else:
    rtems_eg_path=rtems_sdk_path  + '/rtems/rtems-examples'
rtems_eg_remote = "https://gitlab.rtems.org/rtems/rtos/rtems-examples.git"
rtems_eg_commit = "2438af24e0027746c5601386b0776a2b11eb9e6a"
rtems_eg_branch = "main"

if not os.path.exists(rtems_eg_path):
    current_path = os.getcwd()

    os.system("git clone -b {} {} {}".format(rtems_eg_branch, rtems_eg_remote, rtems_eg_path))
    os.chdir(rtems_eg_path)# 切换工作路径至 rtems 路径
    os.system("git checkout {}".format(rtems_eg_commit))
    os.system("git submodule init rtems_waf && git submodule update rtems_waf")
    print('RTEMS-Example download is succeed')
else:
    print('RTEMS-Example exists')
    pass

## STEP 7: add untar toolchain if exists
if (install_platform == windows_x64):
    rtems_tool_src_path=rtems_sdk_path  + '\\' + toolchain_src_package
else:
    rtems_tool_src_path=rtems_sdk_path  + '/' + toolchain_src_package
if os.path.exists(rtems_tool_src_path):
    os.chdir(rtems_sdk_path)# 切换工作路径至 rtems 路径
    os.system("tar -xvJf {} -C ./rtems/rtems-source-builder/rtems".format(rtems_tool_src_path))
    print('RTEMS toolchain is setup')

## STEP 8: display success message and enable environment
print("Success!!! Phytium RTEMS SDK is Install at {}".format(rtems_sdk_path))