#!/usr/bin/env python3

import os
import platform

os.system("rm ./rtems/rtems -rf")
os.system("rm ./rtems/rtems-libbsd/ -rf")
os.system("rm ./standalone/ -rf")
os.system("rm ./rtems/rtems-examples/ -rf")
os.system("rm ./rtems/rtems-source-builder/ -rf")

print("Phytium RTEMS SDK at {} uninstalled !!!".format(os.getcwd()))