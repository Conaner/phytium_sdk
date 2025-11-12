/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2024 Phytium Technology Co., Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * FilePath: main.c
 * Created Date: 2023-10-27 17:02:35
 * Last Modified: 2023-10-27 09:22:20
 * Description:  This file is for RTEMS system entry
 *
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0     zhugengyu  2024/08/20    first release
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sysexits.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include <machine/rtems-bsd-commands.h>

#include <rtems/console.h>
#include <rtems/shell.h>
#include <rtems/stackchk.h>
#include <rtems/bsd/bsd.h>
#include <rtems/bsd/modules.h>

#if defined(RTEMS_BSD_MODULE_NET80211)
#include <rtems/dhcpcd.h>
#endif

int main(int argc, char **argv);

rtems_status_code media_initialization(void);
rtems_status_code network_initialization(void);
rtems_status_code network_configuration(void);
rtems_status_code usb_mouse_initialization(void);
rtems_status_code usb_mouse_configuration(void);
rtems_status_code usb_keyboard_initialization(void);
rtems_status_code usb_keyboard_configuration(void);
int wlan_wait_for_link_up(void);
void wlan_create_wpa_supplicant_conf(void);
rtems_status_code wlan_create_dev(void);
rtems_status_code wlan_init_wpa_supplicant(void);

static rtems_status_code early_initialization(void)
{
    rtems_status_code sc;

    sc = media_initialization();
    assert(sc == RTEMS_SUCCESSFUL);

#if defined(RTEMS_BSD_MODULE_DEV_NET)
    sc = network_initialization();
    assert(sc == RTEMS_SUCCESSFUL);
#endif /* RTEMS_BSD_MODULE_DEV_NET */

#if defined(RTEMS_BSD_MODULE_DEV_USB_INPUT)
    sc = usb_mouse_initialization();
    assert(sc == RTEMS_SUCCESSFUL);

    sc = usb_keyboard_initialization();
    assert(sc == RTEMS_SUCCESSFUL);
#endif /* RTEMS_BSD_MODULE_DEV_USB_INPUT */

    return sc;
}

static rtems_status_code later_initialization(void)
{
    rtems_status_code sc;

#if defined(RTEMS_BSD_MODULE_DEV_NET)
    sc = network_configuration();
    assert(sc == RTEMS_SUCCESSFUL);
#endif /* RTEMS_BSD_MODULE_DEV_NET */

#if defined(RTEMS_BSD_MODULE_DEV_USB_INPUT)
    sc = usb_mouse_configuration();
    assert(sc == RTEMS_SUCCESSFUL);

    sc = usb_keyboard_configuration();
    assert(sc == RTEMS_SUCCESSFUL);
#endif /* RTEMS_BSD_MODULE_DEV_USB_INPUT */

	/* Let the callout timer allocate its resources */
	sc = rtems_task_wake_after(10);
	assert(sc == RTEMS_SUCCESSFUL);

#if defined(RTEMS_BSD_MODULE_NET80211)
    if (wlan_create_dev() == RTEMS_SUCCESSFUL)
    {
        wlan_create_wpa_supplicant_conf();

        sc = rtems_dhcpcd_start(NULL);
        assert(sc == RTEMS_SUCCESSFUL);

        sc = wlan_init_wpa_supplicant();
        assert(sc == RTEMS_SUCCESSFUL);
    }
#endif

    return sc;
}

static void show_platforminfo(void)
{
    printf("\r\n");
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++\r\n");
    printf("  _____   _______   ______   __  __    _____           \r\n");
    printf(" |  __ \\ |__   __| |  ____| |  \\/  |  / ____|          \r\n");
    printf(" | |__) |   | |    | |__    | \\  / | | (___            \r\n");
    printf(" |  _  /    | |    |  __|   | |\\/| |  \\___ \\           \r\n");
    printf(" | | \\ \\    | |    | |____  | |  | |  ____) |          \r\n");
    printf(" |_|  \\_\\   |_|    |______| |_|  |_| |_____/           \r\n");
    printf("                                                       \r\n");
    printf("******************** LibBSD Shell *********************\r\n");
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++\r\n");
}

static void
default_set_self_prio(rtems_task_priority prio)
{
    rtems_status_code sc;

    sc = rtems_task_set_priority(RTEMS_SELF, prio, &prio);
    assert(sc == RTEMS_SUCCESSFUL);
}

static void Init(rtems_task_argument arg)
{
    rtems_status_code sc;

    (void)arg;

    show_platforminfo();

    sc = early_initialization();
    assert(sc == RTEMS_SUCCESSFUL);

    sc = rtems_bsd_initialize();
    assert(sc == RTEMS_SUCCESSFUL);

    sc = later_initialization();
    assert(sc == RTEMS_SUCCESSFUL);

	/* Let UI1 Init task has lower priority to act as background task */
	default_set_self_prio(RTEMS_MAXIMUM_PRIORITY - 1U);

    sc = rtems_shell_init("SHLL", 32 * 1024, 150,
                          CONSOLE_DEVICE_NAME,
                          false,
                          true,
                          NULL);
    assert(sc == RTEMS_SUCCESSFUL);

    exit(0);
}

#ifdef CONFIGURE_POSIX_INIT_THREAD_TABLE
static void *POSIX_Init(void *arg)
{
    (void)arg; /* deliberately ignored */

    /*
     * Initialize optional services
     */

    /*
     * Could get arguments from command line or have a static set.
     */
    (void)main(0, NULL);

    return NULL;
}
#endif

#define CONFIGURE_MICROSECONDS_PER_TICK 1000
#define CONFIGURE_MAXIMUM_PROCESSORS    8

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_STUB_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_ZERO_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_LIBBLOCK

#define CONFIGURE_MAXIMUM_DRIVERS 32

#define CONFIGURE_FILESYSTEM_DOSFS
#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 32

#define CONFIGURE_MAXIMUM_USER_EXTENSIONS 1

#define CONFIGURE_UNLIMITED_ALLOCATION_SIZE 32
#define CONFIGURE_UNLIMITED_OBJECTS
#define CONFIGURE_UNIFIED_WORK_AREAS

#define CONFIGURE_STACK_CHECKER_ENABLED

#define CONFIGURE_BDBUF_BUFFER_MAX_SIZE (512 * 1024)
#define CONFIGURE_BDBUF_MAX_WRITE_BLOCKS 1024
#define CONFIGURE_BDBUF_MAX_READ_AHEAD_BLOCKS 0
#define CONFIGURE_BDBUF_CACHE_MEMORY_SIZE (512 * 1024)
#define CONFIGURE_SWAPOUT_TASK_PRIORITY 105
#define CONFIGURE_SWAPOUT_WORKER_TASK_PRIORITY  105
#define CONFIGURE_BDBUF_READ_AHEAD_TASK_PRIORITY 106

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_INIT_TASK_STACK_SIZE  (256 * 1024)
#define CONFIGURE_INIT_TASK_INITIAL_MODES RTEMS_DEFAULT_MODES
#define CONFIGURE_INIT_TASK_ATTRIBUTES RTEMS_FLOATING_POINT

#define CONFIGURE_INIT
#include <rtems/confdefs.h>

#include <bsp/nexus-devices.h>

#define CONFIGURE_SHELL_COMMANDS_INIT

#include <bsp/irq-info.h>

#include <rtems/netcmds-config.h>

/*
 * Configure LibBSD.
 */
#define RTEMS_BSD_CONFIG_NET_PF_UNIX
#define RTEMS_BSD_CONFIG_NET_IP_MROUTE
#define RTEMS_BSD_CONFIG_NET_IP6_MROUTE
#define RTEMS_BSD_CONFIG_NET_IF_BRIDGE
#define RTEMS_BSD_CONFIG_NET_IF_LAGG
#define RTEMS_BSD_CONFIG_NET_IF_VLAN

#define RTEMS_BSD_CONFIG_BSP_CONFIG
#define RTEMS_BSD_CONFIG_TERMIOS_KQUEUE_AND_POLL
#define RTEMS_BSD_CONFIG_INIT


#ifdef RTEMS_BSD_MODULE_USER_SPACE_WLANSTATS
  #define SHELL_WLANSTATS_COMMAND &rtems_shell_WLANSTATS_Command,
#else
  #define SHELL_WLANSTATS_COMMAND
#endif

#ifdef RTEMS_BSD_MODULE_USR_SBIN_WPA_SUPPLICANT
  #define SHELL_WPA_SUPPLICANT_COMMAND &rtems_shell_WPA_SUPPLICANT_Command,
  #define SHELL_WPA_SUPPLICANT_FOR_COMMAND &rtems_shell_WPA_SUPPLICANT_FORK_Command, 
#else
  #define SHELL_WPA_SUPPLICANT_COMMAND
  #define SHELL_WPA_SUPPLICANT_FOR_COMMAND
#endif

extern rtems_shell_cmd_t rtems_shell_emmc_Command;

#define CONFIGURE_SHELL_USER_COMMANDS \
    SHELL_WLANSTATS_COMMAND \
    SHELL_WPA_SUPPLICANT_COMMAND \
    SHELL_WPA_SUPPLICANT_FOR_COMMAND \
    &bsp_interrupt_shell_command,     \
    &rtems_shell_SYSCTL_Command,      \
	&rtems_shell_BLKSTATS_Command,    \
    &rtems_shell_ARP_Command, \
    &rtems_shell_HOSTNAME_Command, \
    &rtems_shell_PING_Command, \
    &rtems_shell_ROUTE_Command, \
    &rtems_shell_NETSTAT_Command, \
    &rtems_shell_IFCONFIG_Command, \
    &rtems_shell_TCPDUMP_Command, \
    &rtems_shell_VMSTAT_Command

#define CONFIGURE_SHELL_COMMAND_RTEMS
#define CONFIGURE_SHELL_COMMAND_CPUUSE
#define CONFIGURE_SHELL_COMMAND_PERIODUSE
#define CONFIGURE_SHELL_COMMAND_STACKUSE
#define CONFIGURE_SHELL_COMMAND_PROFREPORT
#define CONFIGURE_SHELL_COMMAND_WKSPACE_INFO

#define CONFIGURE_SHELL_COMMAND_RTRACE
#define CONFIGURE_SHELL_COMMAND_RTC
#define CONFIGURE_SHELL_COMMAND_SPI
#define CONFIGURE_SHELL_COMMAND_FLASHDEV
#define CONFIGURE_SHELL_COMMAND_I2CDETECT
#define CONFIGURE_SHELL_COMMAND_I2CGET
#define CONFIGURE_SHELL_COMMAND_I2CSET
#define CONFIGURE_SHELL_COMMAND_PCI

#define CONFIGURE_SHELL_COMMAND_LOGOFF
#define CONFIGURE_SHELL_COMMAND_EXIT
#define CONFIGURE_SHELL_COMMAND_SETENV
#define CONFIGURE_SHELL_COMMAND_GETENV
#define CONFIGURE_SHELL_COMMAND_UNSETENV

#define CONFIGURE_SHELL_COMMAND_MDUMP
#define CONFIGURE_SHELL_COMMAND_WDUMP
#define CONFIGURE_SHELL_COMMAND_LDUMP
#define CONFIGURE_SHELL_COMMAND_HEXDUMP

#define CONFIGURE_SHELL_COMMAND_CMDCHOWN
#define CONFIGURE_SHELL_COMMAND_CMDCHMOD
#define CONFIGURE_SHELL_COMMAND_JOEL
#define CONFIGURE_SHELL_COMMAND_DATE
#define CONFIGURE_SHELL_COMMAND_ECHO
#define CONFIGURE_SHELL_COMMAND_EDIT
#define CONFIGURE_SHELL_COMMAND_MEDIT
#define CONFIGURE_SHELL_COMMAND_MFILL
#define CONFIGURE_SHELL_COMMAND_MMOVE
#define CONFIGURE_SHELL_COMMAND_SLEEP
#define CONFIGURE_SHELL_COMMAND_ID
#define CONFIGURE_SHELL_COMMAND_TTY
#define CONFIGURE_SHELL_COMMAND_WHOAMI

#define CONFIGURE_SHELL_COMMAND_CP
#define CONFIGURE_SHELL_COMMAND_PWD
#define CONFIGURE_SHELL_COMMAND_LS
#define CONFIGURE_SHELL_COMMAND_LN
#define CONFIGURE_SHELL_COMMAND_LSOF
#define CONFIGURE_SHELL_COMMAND_CHDIR
#define CONFIGURE_SHELL_COMMAND_CD
#define CONFIGURE_SHELL_COMMAND_MKDIR
#define CONFIGURE_SHELL_COMMAND_RMDIR
#define CONFIGURE_SHELL_COMMAND_CHROOT
#define CONFIGURE_SHELL_COMMAND_CHMOD
#define CONFIGURE_SHELL_COMMAND_CAT
#define CONFIGURE_SHELL_COMMAND_MV
#define CONFIGURE_SHELL_COMMAND_RM
#define CONFIGURE_SHELL_COMMAND_UMASK
#define CONFIGURE_SHELL_COMMAND_MALLOC_INFO

#define CONFIGURE_SHELL_COMMAND_BLKSTATS
#define CONFIGURE_SHELL_COMMAND_BLKSYNC
#define CONFIGURE_SHELL_COMMAND_MOUNT
#define CONFIGURE_SHELL_COMMAND_UNMOUNT
#define CONFIGURE_SHELL_COMMAND_MSDOSFMT
#define CONFIGURE_SHELL_COMMAND_MKRFS
#define CONFIGURE_SHELL_COMMAND_DEBUGRFS
#define CONFIGURE_SHELL_COMMAND_FDISK
#define CONFIGURE_SHELL_COMMAND_BLKSTATS
#define CONFIGURE_SHELL_COMMAND_BLKSYNC
#define CONFIGURE_SHELL_COMMAND_DF
#define CONFIGURE_SHELL_COMMAND_DD

#define CONFIGURE_SHELL_COMMAND_MD5

#define CONFIGURE_SHELL_COMMAND_SHUTDOWN
#define CONFIGURE_SHELL_COMMAND_CPUINFO
#define CONFIGURE_SHELL_COMMAND_TOP

#include <rtems/shellconfig.h>