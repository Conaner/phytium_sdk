# 配置 RTEMS 开发环境 (Ubuntu/Debian x64)

- 本文介绍 Ubuntu 21.04 (x64)、WSL 1/2 Ubuntu/Debain 环境下，通过交叉编译构建 RTEMS 镜像的方法

## 1. 安装环境依赖

- 安装 GCC 编译链，注意 python3.6-dev 根据主机的上的 Python 版本定，如果是 python 3.11 就用 python3.11-dev
```
python --version
sudo apt-get update
sudo apt-get install binutils gcc g++ gdb unzip git python3.8-dev
```

- apt-get build-dep 需要将 /etc/apt/sources.list 的 deb-src 打开

- 有些系统 python --version 没有效果，需要额外创建 python 软链接
```
sudo apt install python-is-python3
```

## 2. 获取 SDK 源码

- 下载 Phytium RTEMS SDK 源码
```
git clone https://gitee.com/phytium_embedded/phytium-rtems-sdk.git ./phytium-rtems-sdk
```

- 源码构建 AARCH64 用的交叉编译工具，构建前会下载 gcc，gdb 等一系列源码包到 ./rtems/rtems-source-builder/rtems 目录下，可能会由于网络问题下载较慢，可以下载源码包 [toolchain_sources.tar.xz, 提取码：RTTL](https://pan.baidu.com/s/1V09uR2UAx3j_B0dc3Ht9tg)，放置在 SDK 根目录下，运行 ./install.py 时将工具链解压到 ./rtems/rtems-source-builder/rtems 目录下，节省下载时间

- 运行 install.py，拉取 RTEMS，RTEMS LibBSD 和 RTEMS Source Builder 源码，需要的拉取的源码较多，可能需要等待一段时间

```
$ cd phytium-rtems-sdk
$ ./install.py
```

- 拉取完成后会显示

```
Success!!! Phytium RTEMS SDK is Install at /xxx/phytium-rtems-sdk
```

## 3. 构建交叉编译链

- 输入下面的命令开始构建编译链，整个过程可能需要较长的时间（通常会大于 1 小时，具体速度和编译主机的性能有关）

```
$ make aarch64_rtems_toolchain
```

> 注意 WSL1 中不要将 SDK 放到 /mnt/d 等 Windows 挂载盘目录下

- 编译成功后交叉工具链在 toolchain 目录下，后面构建的 BSP 静态库也会被安装在这个目录下

## 4. 构建目标平台 BSP

- 在交叉编译工具的基础上，进一步构建目标平台的 BSP，生成一系列静态库，后面应用程序会链接这些静态库

> 在 SDK 根目录下编译所有目标平台的 BSP，每个平台都会对应一组独立的静态库

```
$ make all
```

> 编译指定目标平台的 BSP

```
$ make e2000d_demo_aarch64_all
$ make phytium_pi_aarch64_all
$ make d2000_test_aarch64_all
$ make ft2004_dsk_aarch64_all
```

## 5. 构建应用程序镜像

- 完成目标平台 BSP 的构建后，最后编译应用代码，和 BSP 静态库进行链接，生成可以在开发板上运行的镜像文件

- 首先在 SDK 根目录下选择当前生效的 BSP，如选择 Phytium PI

```
$ source tools/env_phytium_pi_aarch64.sh
```

- 然后，选择应用程序进行编译，对于 RTEMS 内核应用

```
$ cd examples/rtems
$ make clean image
```

- 编译完成后，会生成 o-optimize 目录和一系列目标文件，其中

```
rtems.exe  应用程序镜像
rtems.asm  应用程序反汇编文件
rtems.map  应用程序镜像分析文件
```

- 对于 RTEMS LibBSD 应用

```
$ cd examples/rtems-libbsd
$ make clean image
```

- 编译完成后，会生成 o-optimize 目录和一系列目标文件，其中

```
rtems_libbsd.exe  应用程序镜像
rtems_libbsd.asm  应用程序反汇编文件
rtems_libbsd.map  应用程序镜像分析文件
```

- 默认用 make image 会将镜像拷贝到 /mnt/d/tftpboot 目录，可以在应用工程的 makefile 中修改 USR_BOOT_DIR 变量的值指定为其它目录

- 然后确保开发板可以访问主机的 tftp 服务，将镜像上传到开发板上

- 如果是加载 bin 文件，启动之前需要刷新一下 dcache
```
setenv ipaddr 192.168.4.20
setenv serverip 192.168.4.30
setenv gatewayip 192.168.4.1
tftpboot 0x80100000 rtems.bin;dcache flush; go 0x80100000;
```

- 如果是加载 elf 文件（RTEMS 编译输出中以 *.exe 结尾）
> 注意必须 RTEMS 镜像中有多个 section，只能用 bootelf -s 启动，不能用 bootelf -p
```
setenv ipaddr 192.168.4.20
setenv serverip 192.168.4.30
setenv gatewayip 192.168.4.1
tftpboot 0xd0100000 rtems.exe; bootelf -s 0xd0100000;
```

## 6. 使用 RTEMS 自带的镜像

- RTEMS 内核中也提供了大量的现成应用程序，在构建 BSP 的过程中会构建成镜像，用户可以通过下面的方法，方便地将这些镜像拷贝到 /mnt/d/tftpboot 目录下，然后加载启动

- 首先需要参考步骤 5 选择生效的 BSP，然后拷贝镜像
> 注意 source toos/env_xxx.sh 后只在当前 Shell 端生效，不要切换控制台 Shell

### 6.1 RTEMS 内核测试套件

- 对于 RTEMS 内核测试套件中的镜像

```
copy_image hello.exe testsuite
```

- 查看 RTEMS 内核测试套件中所有的镜像

```
list_images testsuite
```

### 6.2 RTEMS LibBSD 测试套件

- 对于 RTEMS LibBSD 测试套件中的镜像

```
copy_image media01.exe bsdtestsuite
```

- 查看 RTEMS LibBSD 测试套件中所有的镜像

```
list_images bsdtestsuite
```

### 6.3 RTEMS Examples 测试套件

- 查看 RTEMS Examples 中所有的镜像

```
list_images examples
```

- 对于 RTEMS Examples 中的镜像

```
copy_image both_hello.exe examples
```

## 7. 更新 SDK 版本

- RTEMS-SDK 中有多个组件是通过脚本拉取的，更新 SDK 版本前，需要先删除这些组件

```
$ ./uninstall.py
```

- 然后使用 `git fetch` 或者 `git pull` 更新分支