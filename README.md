# Phytium-RTEMS-SDK

v2.0.0 [ReleaseNote](./docs/ChangeLog.md)

**如需Phytium全系CPU的软件适配支持和完整设备树，请联系** linan1284@phytium.com.cn

[飞腾腾云S系列高性能服务器CPU](https://www.phytium.com.cn/homepage/production/list/0)

[飞腾腾锐D系列高效能桌面CPU](https://www.phytium.com.cn/homepage/production/list/1)

[飞腾腾珑E系列高端嵌入式CPU](https://www.phytium.com.cn/homepage/production/list/2)

## 1. 介绍

- RTEMS (Real-Time Executive for Multiprocessor Systems) 是一个开源实时操作系统，可以用于太空飞行、医疗、网络和各类嵌入式设备。本项目发布了 Phytium 系列 CPU 的 RTEMS内核/RTEMS-LibBSD 源码使用工具，参考例程以及配置构建工具。

- [RTEMS内核仓库](https://gitee.com/phytium_embedded/rtems)

- [RTEMS-LibBSD仓库](https://gitee.com/phytium_embedded/rtems-libbsd)

- 目前支持的 RTEMS 版本包括

| **芯片**      | **RTEMS 版本**  | **RTEMS (单核)** | **RTEMS (多核 SMP)** | **RTEMS-LibBSD** |
| :-------------| :----------: | :-----------------: |  :-----------------: |  :-----------------|
| PD2408(AARCH64 模式)        |     6.1     | 支持  | 支持  | 支持  |
| PD2308(AARCH64 模式)        |     6.1     | 支持  | 支持  | 支持  |
| E2000D(AARCH64 模式)        |     6.1     | 支持  | 支持  | 支持  |
| E2000Q(AARCH64 模式)        |     6.1     | 支持  | 支持  | 支持  |
| PhytiumPI(AARCH64 模式)        |     6.1     | 支持  | 支持  | 支持  |
| D2000(AARCH64 模式)        |     6.1     | 支持  | 支持  | 支持  |
| FT2000/4(AARCH64 模式)        |     6.1     | 支持  | 支持  | 支持  |

## 2. 快速入门

- 目前 SDK 支持在 Ubuntu 20.04 (x64)、WSL 1/2 Ubuntu/Debain 通过交叉编译构建 RTEMS 镜像 [Ubuntu x86_64/Windows WSL 快速入门](./docs/get-start/ubuntu_x64_setup.md)


## 3. 使用方法

- 参考[使用方法](./docs/get-start/ubuntu_x64_setup.md)中提供的说明构建 RTEMS 镜像
- SDK 主要包括下面几个部分
- - ./examples, SDK 使用例程
- - ./rtems/rtems, RTEMS 内核源码
- - ./rtems/rtems-libbsd, RTEMS LibBSD 驱动库源码
- - ./rtems/rtems-source-builder, RTEMS 工具链源码
- - ./rtems/rtems-examples, RTEMS 提供的例程
- - ./rtems/rtems/testsuites, RTEMS 提供的测试程序源码
- - ./rtems/rtems/testsuites/samples, RTEMS 提供的程序示例
- - ./rtems/rtems-libbsd/testsuite, RTEMS LibBSD 提供的测试程序源码
- - ./standalone, SDK 引用的 Phytium-Standalone-SDK 源码
- - ./toolchain, 编译生成的 RTEMS 工具链和 BSP 静态库
- - ./tools, 构建方法的入口和镜像拷贝的工具
- - ./configs/bsp, RTEMS Phytium BSP 的默认编译配置
- - ./configs/buildset, RTEMS Phytium LibBSD 的默认编译配置
- - ./configs/dts, RTEMS Phytium BSP 的设备树
- - ./configs/dtb，编译生成的设备树二进制文件
- - ./configs/testing, RTEMS Phytium BSP 的测试配置

## 4. 应用例程

- SDK 提供的应用例程包括

| 特性            | 支持平台                        | 例程              |
| -------------------| ------------------------------------------ | ---------------------- |
| Hello World         | E2000D <br>E2000Q <br>PHYTIUMPI<br> D2000 <br> FT2000/4| examples/rtems           |
| POSIX         | E2000D <br>E2000Q <br>PHYTIUMPI<br> D2000 <br> FT2000/4| examples/rtems           |
| C++ (with Standard Library)        | E2000D <br>E2000Q <br>PHYTIUMPI<br> D2000 <br> FT2000/4| examples/rtems           |
| BSD Shell        | E2000D <br>E2000Q <br>PHYTIUMPI<br> D2000 <br> FT2000/4| examples/rtems-libbsd           |
| Network        | E2000D <br>E2000Q <br>PHYTIUMPI<br> D2000 <br> FT2000/4| examples/rtems-libbsd           |
| Telnet Shell        | E2000D <br>E2000Q <br>PHYTIUMPI<br> D2000 <br> FT2000/4| examples/rtems-libbsd           |
| TCP/IP GDB        | E2000D <br>E2000Q <br>PHYTIUMPI<br> D2000 <br> FT2000/4| rtems-libbsd/debugger01           |
| SD/eMMC Filesystem        | E2000D <br>E2000Q <br>PHYTIUMPI | examples/rtems-libbsd           |
| USB HID/MSC        | E2000D <br>E2000Q <br>PHYTIUMPI | examples/rtems-libbsd           |
| USB HID/MSC (in PCIe)        | E2000D <br>E2000Q <br>PHYTIUMPI<br> D2000 <br> FT2000/4| examples/rtems-libbsd           |
| NVMe FileSystem (in PCIe)       | E2000D <br>E2000Q <br>PHYTIUMPI | examples/rtems-libbsd           |
| WLAN (RTL8188EU in USB)       | E2000D <br>E2000Q <br>PHYTIUMPI | examples/rtems-libbsd           |

## 5. 参考资料

- [RTEMS 官网](https://gitlab.rtems.org/)
- [FreeBSD 和 RTEMS，UNIX 系统作为实时操作系统](https://freebsdfoundation.org/wp-content/uploads/2016/08/FreeBSD-and-RTEMS-Unix-in-a-Real-Time-Operating-System.pdf)

## 6. 贡献方法

请联系飞腾嵌入式软件部

opensource_embedded@phytium.com.cn

## 7. 许可协议

BSD-2