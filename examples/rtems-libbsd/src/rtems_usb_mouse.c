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
 * FilePath: rtems_usb_mouse.c
 * Created Date: 2023-10-27 17:02:35
 * Last Modified: 2023-10-27 09:22:20
 * Description:  This file is for RTEMS USB Mouse setup
 *
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0     zhugengyu  2024/08/20    first release
 */

#include <sys/malloc.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mouse.h>

#include <rtems/console.h>
#include <rtems/shell.h>
#include <rtems/bsd/bsd.h>

#define USB_SERIAL_TEST_BUFFER_SIZE 48
#define PRIO_OPEN  (RTEMS_MAXIMUM_PRIORITY - 12)
#define PRIO_READ  (RTEMS_MAXIMUM_PRIORITY - 11)

struct usb_test_message {
	int fd;
	char rbuf[USB_SERIAL_TEST_BUFFER_SIZE];
};

static rtems_id oid, rid, omid, rmid;
static volatile bool kill_otask, kill_rtask;
static volatile bool otask_active, rtask_active;


static void
usb_mouse_read_task(rtems_task_argument arg)
{
	rtems_status_code sc;
	struct usb_test_message msg;
	size_t size;
	int bytes;

	rtask_active = true;
	kill_rtask = false;
	while (!kill_rtask) {
		while (!kill_rtask) {
			sc = rtems_message_queue_receive(rmid, (void *)&msg, &size, RTEMS_WAIT, RTEMS_MILLISECONDS_TO_TICKS(1000));
			if (sc == RTEMS_SUCCESSFUL) {
				if (msg.fd > 0) {
					break;
				} else {
					printf("Invalid file descriptor\n");
				}
			}
	  }
		while (!kill_rtask) {
			msg.rbuf[0] = 0;
			bytes = read(msg.fd, &msg.rbuf[0], 1);
			if (bytes == 0) {
				printf("Got EOF from the input device\n");
			} else if (bytes < 0) {
				if (errno != EINTR && errno != EAGAIN) {
					printf("Could not read from input device\n");
					break;
				}
				rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(10));
			} else {
				printf("received char: 0x%02X\n", msg.rbuf[0]);
			}
	  }
	  msg.fd = -1;
	  rtems_message_queue_send(omid, (void *)&msg, sizeof(msg));
	}

	rtask_active = false;
	rtems_task_exit();
}

static void
usb_mouse_open_task(rtems_task_argument arg)
{
	rtems_status_code sc;
	struct usb_test_message msg;
	size_t size;
	int fd;

	fd = -2;
	otask_active = true;
	kill_otask = false;
	while (!kill_otask) {
		sc = rtems_message_queue_receive(omid, (void *)&msg, &size, RTEMS_WAIT, RTEMS_MILLISECONDS_TO_TICKS(1000));
		if (sc == RTEMS_SUCCESSFUL) {
			if (fd >= 0) {
				close(fd);
				printf("mouse device closed\n");
			}
			fd = msg.fd;
		}
		if (fd == -1) {
			fd = open("/dev/ums0", O_RDWR | O_NONBLOCK);
			if (fd != -1) {
				printf("mouse device opened: %d\n", fd);
				msg.fd = fd;
				rtems_message_queue_send(rmid, (void *)&msg, sizeof(msg));
			}
			else {
				/*printf("mouse device open failed: %d\n", errno);*/
			}
		}
	}
	if (fd >= 0) {
		close(fd);
		printf("mouse device closed\n");
	}
	otask_active = false;
	rtems_task_exit();
}

static void
default_usb_mouse_on_exit(int exit_code, void *arg)
{
    rtems_status_code sc;
    (void)arg;

	kill_otask = true;
	kill_rtask = true;
	while (otask_active || rtask_active) {
		rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(10));
	}

	sc = rtems_message_queue_delete(rmid);
	assert(sc == RTEMS_SUCCESSFUL);

	sc = rtems_message_queue_delete(omid);
	assert(sc == RTEMS_SUCCESSFUL);

    exit(exit_code);
}

rtems_status_code usb_mouse_initialization(void)
{
	rtems_status_code sc;

	sc = rtems_message_queue_create(
		rtems_build_name ('M', 'U', 'O', 'P'),
		16,
		sizeof(struct usb_test_message),
		RTEMS_PRIORITY,
		&omid
	);
	assert(sc == RTEMS_SUCCESSFUL);

	sc = rtems_message_queue_create(
		rtems_build_name ('M', 'U', 'R', 'D'),
		16,
		sizeof(struct usb_test_message),
		RTEMS_PRIORITY,
		&rmid
	);
	assert(sc == RTEMS_SUCCESSFUL);

	sc = rtems_task_create(
		rtems_build_name('U', 'S', 'B', 'R'),
		PRIO_READ,
		RTEMS_MINIMUM_STACK_SIZE,
		RTEMS_DEFAULT_MODES,
		RTEMS_FLOATING_POINT,
		&rid
	);
	assert(sc == RTEMS_SUCCESSFUL);

	sc = rtems_task_create(
		rtems_build_name('U', 'S', 'B', 'O'),
		PRIO_OPEN,
		RTEMS_MINIMUM_STACK_SIZE,
		RTEMS_DEFAULT_MODES,
		RTEMS_FLOATING_POINT,
		&oid
	);
	assert(sc == RTEMS_SUCCESSFUL);

	sc = rtems_task_start(rid, usb_mouse_read_task, 0);
	assert(sc == RTEMS_SUCCESSFUL);

	sc = rtems_task_start(oid, usb_mouse_open_task, 0);
	assert(sc == RTEMS_SUCCESSFUL);

    on_exit(default_usb_mouse_on_exit, NULL);

    return sc;
}

rtems_status_code usb_mouse_configuration(void)
{
    rtems_status_code sc;
	struct usb_test_message msg;

	msg.fd = -1;
	sc = rtems_message_queue_send(omid, (const void *)&msg, sizeof(msg));

    return sc;
}