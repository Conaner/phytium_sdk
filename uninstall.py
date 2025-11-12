#!/usr/bin/env python3

import os
import platform

print("即将删除以下目录，请确认没有未提交备份的代码 ！！！！！\n")
print(" /rtems/rtems\n")
print(" /rtems/rtems-libbsd\n")
print(" /standalone\n")
print(" /rtems/rtems-examples\n")
print(" /rtems/rtems-source-builder\n")
confirm = input("\n确认删除以上所有目录吗？此操作不可恢复！ [y/N] ").strip().lower()
if confirm != 'y':
    print("操作已取消")
    return

os.system("rm ./rtems/rtems -rf")
os.system("rm ./rtems/rtems-libbsd/ -rf")
os.system("rm ./standalone/ -rf")
os.system("rm ./rtems/rtems-examples/ -rf")
os.system("rm ./rtems/rtems-source-builder/ -rf")

print("Phytium RTEMS SDK at {} uninstalled !!!".format(os.getcwd()))