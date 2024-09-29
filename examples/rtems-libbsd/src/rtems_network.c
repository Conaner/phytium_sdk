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
 * FilePath: rtems_meida.c
 * Created Date: 2023-10-27 17:02:35
 * Last Modified: 2023-10-27 09:22:20
 * Description:  This file is for RTEMS Network setup
 *
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0     zhugengyu  2024/08/20    first release
 */

#include <sys/stat.h>
#include <sys/socket.h>

#include <net/if.h>

#include <assert.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <machine/rtems-bsd-commands.h>

#include <bsp.h>

#include <rtems.h>
#include <rtems/printer.h>
#include <rtems/test-info.h>
#include <rtems/stackchk.h>
#include <rtems/bsd/bsd.h>
#include <rtems/bsd/modules.h>
#include <rtems/dhcpcd.h>
#include <rtems/telnetd.h>

/* Configure network interface */
#if defined(PHYTIUM_BSP_TYPE_E2000D_DEMO) || \
    defined(PHYTIUM_BSP_TYPE_E2000Q_DEMO) || \
    defined(PHYTIUM_BSP_TYPE_PHYTIUM_PI)
#define NET_CFG_INTERFACE_0 "cgem0"
#else
#define NET_CFG_INTERFACE_0 "lo0"
#endif
#define NET_CFG_SELF_IP "192.168.4.20"
#define NET_CFG_NETMASK "255.255.255.0"
#define NET_CFG_PEER_IP "192.168.4.50"
#define NET_CFG_GATEWAY_IP "192.168.4.1"

/* Enable network shell */
#define DEFAULT_NETWORK_SHELL

/* Disable DHCP */
/* #define DEFAULT_NETWORK_DHCPCD_ENABLE */

#if defined(DEFAULT_NETWORK_DHCPCD_ENABLE) && \
    !defined(DEFAULT_NETWORK_NO_STATIC_IFCONFIG)
#define DEFAULT_NETWORK_NO_STATIC_IFCONFIG
#endif

#ifdef DEFAULT_NETWORK_NO_STATIC_IFCONFIG
#error "DHCP not supported !!!"
#endif

#ifdef DEFAULT_NETWORK_SHELL
#include <rtems/console.h>
#include <rtems/shell.h>
#endif

static void
default_network_set_self_prio(rtems_task_priority prio)
{
    rtems_status_code sc;

    sc = rtems_task_set_priority(RTEMS_SELF, prio, &prio);
    assert(sc == RTEMS_SUCCESSFUL);
}

#ifndef DEFAULT_NETWORK_NO_INTERFACE_0
static void
default_network_ifconfig_hwif0(char *ifname)
{
    int exit_code;
    char *ifcfg[] = {
        "ifconfig",
        ifname,
#ifdef DEFAULT_NETWORK_NO_STATIC_IFCONFIG
        "up",
#else
        "inet",
        NET_CFG_SELF_IP,
        "netmask",
        NET_CFG_NETMASK,
#endif
        NULL};

    exit_code = rtems_bsd_command_ifconfig(RTEMS_BSD_ARGC(ifcfg), ifcfg);
    assert(exit_code == EX_OK);
}

static void
default_network_route_hwif0(char *ifname)
{
#ifndef DEFAULT_NETWORK_NO_STATIC_IFCONFIG
    int exit_code;
    char *dflt_route[] = {
        "route",
        "add",
        "-host",
        NET_CFG_GATEWAY_IP,
        "-iface",
        ifname,
        NULL};
    char *dflt_route2[] = {
        "route",
        "add",
        "default",
        NET_CFG_GATEWAY_IP,
        NULL};

    exit_code = rtems_bsd_command_route(RTEMS_BSD_ARGC(dflt_route), dflt_route);
    assert(exit_code == EXIT_SUCCESS);

    exit_code = rtems_bsd_command_route(RTEMS_BSD_ARGC(dflt_route2), dflt_route2);
    assert(exit_code == EXIT_SUCCESS);
#endif
}
#endif

static void
default_network_dhcpcd(void)
{
#ifdef DEFAULT_NETWORK_DHCPCD_ENABLE
    static const char default_cfg[] = "clientid libbsd test client\n";
    rtems_status_code sc;
    int fd;
    int rv;
    ssize_t n;

    fd = open("/etc/dhcpcd.conf", O_CREAT | O_WRONLY,
              S_IRWXU | S_IRWXG | S_IRWXO);
    assert(fd >= 0);

    n = write(fd, default_cfg, sizeof(default_cfg) - 1);
    assert(n == (ssize_t)sizeof(default_cfg) - 1);

#ifdef DEFAULT_NETWORK_DHCPCD_NO_DHCP_DISCOVERY
    static const char nodhcp_cfg[] = "nodhcp\nnodhcp6\n";

    n = write(fd, nodhcp_cfg, sizeof(nodhcp_cfg) - 1);
    assert(n == (ssize_t)sizeof(nodhcp_cfg) - 1);
#endif

    rv = close(fd);
    assert(rv == 0);

    sc = rtems_dhcpcd_start(NULL);
    assert(sc == RTEMS_SUCCESSFUL);
#endif
}

static void
default_network_on_exit(int exit_code, void *arg)
{
    rtems_printer printer;

    (void)arg;

    rtems_print_printer_printf(&printer);
    rtems_stack_checker_report_usage_with_plugin(&printer);

    exit(exit_code);
}

static void telnet_shell(char *name, void *arg)
{
    rtems_shell_env_t env;

    rtems_shell_dup_current_env(&env);

    env.devname = name;
    env.taskname = "TLNT";
    env.login_check = NULL;
    env.forever = false;

    rtems_shell_main_loop(&env);
}

rtems_telnetd_config_table rtems_telnetd_config = {
    .command = telnet_shell,
    .arg = NULL,
    .priority = 0,
    .stack_size = 0,
    .login_check = NULL,
    .keep_stdio = false
};

rtems_status_code network_initialization(void)
{
    rtems_status_code sc = RTEMS_SUCCESSFUL;

    /*
     * Default the syslog priority to 'debug' to aid developers.
     */
    rtems_bsd_setlogpriority("debug");

    on_exit(default_network_on_exit, NULL);

	/* Let other tasks run to complete background work */
	default_network_set_self_prio(RTEMS_MAXIMUM_PRIORITY - 1U);

    return sc;
}

rtems_status_code network_configuration(void)
{
    rtems_status_code sc;
#ifndef DEFAULT_NETWORK_NO_INTERFACE_0
#ifdef DEFAULT_NETWORK_NO_STATIC_IFCONFIG
    char ifnamebuf[IF_NAMESIZE];
#endif
    char *ifname;
#endif

#ifndef DEFAULT_NETWORK_NO_INTERFACE_0
#ifdef DEFAULT_NETWORK_NO_STATIC_IFCONFIG
	ifname = if_indextoname(1, &ifnamebuf[0]);
	assert(ifname != NULL);
#else
	ifname = NET_CFG_INTERFACE_0;
#endif
#endif

	/* Let the callout timer allocate its resources */
	sc = rtems_task_wake_after(2);
	assert(sc == RTEMS_SUCCESSFUL);

	rtems_bsd_ifconfig_lo0();
#ifndef DEFAULT_NETWORK_NO_INTERFACE_0
    printf("ifname = %s \n", ifname);
	default_network_ifconfig_hwif0(ifname);
	default_network_route_hwif0(ifname);
#endif
	default_network_dhcpcd();

	sc = rtems_telnetd_initialize();
	assert(sc == RTEMS_SUCCESSFUL);

    return sc;
}