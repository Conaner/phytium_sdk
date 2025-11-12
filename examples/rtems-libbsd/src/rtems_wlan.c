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
 * FilePath: rtems_wlan.c
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
#include <rtems/bsd/iface.h>
#include <rtems/dhcpcd.h>
#include <rtems/telnetd.h>

#define WLAN_CFG_INTERFACE_0 "wlan0"
#define WLAN_CFG_DEVICE_0 "rtwn0"

#if defined(RTEMS_BSD_MODULE_NET80211) && (RTEMS_BSD_MODULE_NET80211)
rtems_status_code wlan_create_dev(void)
{
	int exit_code;
	char *ifcfg[] = {
		"ifconfig",
		WLAN_CFG_INTERFACE_0,
		"create",
		"wlandev",
		WLAN_CFG_DEVICE_0,
		"up",
		NULL
	};

	exit_code = rtems_bsd_command_ifconfig(RTEMS_BSD_ARGC(ifcfg), ifcfg);
	if(exit_code != EXIT_SUCCESS) {
		printf("ERROR while creating %s.", WLAN_CFG_INTERFACE_0);
	}

    return (rtems_status_code)exit_code;
}

#define WPA_SUPPLICANT_CONF_FILE  "/etc/wpa_supplicant.conf"

static char *wpa_supplicant_cmd[] = {
	"wpa_supplicant",
	"-Dbsd",
	"-i"WLAN_CFG_INTERFACE_0,
	"-c",
	WPA_SUPPLICANT_CONF_FILE,
	NULL
};

static int file_exists(const char *filename) {
    struct stat st;
    int result = stat(filename, &st);
    return result == 0;
}

static void
wpa_supplicant_watcher_task(rtems_task_argument arg)
{
	int argc;
	char ** argv;
	int err;
	(void) arg;

	argv = wpa_supplicant_cmd;
	argc = sizeof(wpa_supplicant_cmd)/sizeof(wpa_supplicant_cmd[0])-1;

	while (true) {
		rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(2000));
		err = rtems_bsd_command_wpa_supplicant(argc, argv);
		printf("wpa_supplicant returned with %d\n", err);
	}
}

void wlan_create_wpa_supplicant_conf(void)
{
    char name[64];
    int n = snprintf(&name[0], sizeof(name), "%s", WPA_SUPPLICANT_CONF_FILE);
    FILE *file = NULL;
    size_t bytes_written;

    assert(n < (int)sizeof(name));

    file = fopen(&name[0], "w");
    if (file == NULL) {
        printf("Error: Failed to create file %s\n", name);
        return;
    }

    const char wpa_config[] = 
        "network={\n"
        "    ssid=\"phytium_net\"\n"
        "    key_mgmt=WPA-PSK\n"
        "    psk=\"phytium-net\"\n"
        "}\n";

    bytes_written = fwrite(wpa_config, sizeof(wpa_config) - 1, 1, file);
    if (bytes_written != 1) {
        printf("Error: Failed to write configuration to file\n");
    }

    fclose(file);
}

rtems_status_code wlan_init_wpa_supplicant(void)
{
	rtems_status_code sc;
	rtems_id id;

	if (!file_exists(WPA_SUPPLICANT_CONF_FILE)) {
		printf("ERROR: wpa configuration does not exist: %s\n",WPA_SUPPLICANT_CONF_FILE);
		return -1;
	}

	sc = rtems_task_create(
		rtems_build_name('W', 'P', 'A', 'W'),
		(RTEMS_MAXIMUM_PRIORITY - 1),
		32 * 1024,
		RTEMS_DEFAULT_MODES,
		RTEMS_FLOATING_POINT,
		&id
	);
	assert(sc == RTEMS_SUCCESSFUL);

	sc = rtems_task_start(id, wpa_supplicant_watcher_task, 0);
	return sc;
}

#endif
