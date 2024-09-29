# Phytium RTEMS SDK 2024-09-26 ChangeLog

Change Log sinc init

## rtems

- add rtems 6.0.0.f61086a40492deabec039c4168d3c3cd87322e5f 
- add phytium bsp to support UP/SMP
- - e2000d demo
- - e2000q demo
- - phytium pi
- - d2000 test
- - ft2004 dsk
- add device tree support for phytium bsp
- merge phytium standalone sdk
- add l3 cache support for d2000 test/ft2004 dsk
- add watchdog reset for phytium platform
- modify aarch64 smp startup to decouple processor id and mpidr_el1
- modify arm-gicv3 support for phytium bsp
- fix overflow bugs on main_df.c

## rtems-libbsd

- add rtems-libbsd 6.0.0.8c872c853b44ae988fb537ef9146a65a82c2e790
- add libbsd support for all phytium bsp
- add phytium_sdif drvirs for e2000/phytium sd/emmc support
- add pci_host support from FreeBSD 12.1
- add xhci usb and xhci pci support from FreeBSD 12.1
- fix uninit value bug on ping.c
- modify mii_physubr.c for phytium bsp
- modify mmc.c for phytium bsp
- modify pci.c to set as intx mode
- modify nvd.c to add nvme filesystem auto mount
- fix inc file missing bug

## example

- add rtems example to demo POSIX and C++ usage
- add rtems-libbsd example to demo Shell、Network、Filesystem、SD/eMMC、XHCI-USB/XHCI-PCIe (keyboard、mouse、mass-storage、hub)、PCI（NVMe disk）usage

# tools

- add toolchain makefiles and bsp utilities