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
 * Description:  This file is for RTEMS Meida and filesystem setup
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

#include <machine/rtems-bsd-commands.h>

#include <rtems/bdbuf.h>
#include <rtems/console.h>
#include <rtems/media.h>
#include <rtems/shell.h>
#include <rtems/stackchk.h>
#include <rtems/bsd/bsd.h>
#include <rtems/bsd/modules.h>

static rtems_status_code media_listener(rtems_media_event event,
                                        rtems_media_state state,
                                        const char *src,
                                        const char *dest,
                                        void *arg)
{
    printf(
        "media listener: event = %s, state = %s, src = %s",
        rtems_media_event_description(event),
        rtems_media_state_description(state),
        src);

    if (dest != NULL)
    {
        printf(", dest = %s", dest);
    }

    if (arg != NULL)
    {
        printf(", arg = %p\n", arg);
    }

    printf("\n");

    if (event == RTEMS_MEDIA_EVENT_MOUNT && state == RTEMS_MEDIA_STATE_SUCCESS)
    {
        char name[256];
        int n = snprintf(&name[0], sizeof(name), "%s/test.txt", dest);
        FILE *file;

        assert(n < (int)sizeof(name));

        printf("write file %s\n", &name[0]);
        file = fopen(&name[0], "w");
        if (file != NULL)
        {
            const char hello[] = "Hello, world!\n";

            fwrite(&hello[0], sizeof(hello) - 1, 1, file);
            fclose(file);
        }
    }

    return RTEMS_SUCCESSFUL;
}

rtems_status_code media_initialization(void)
{
    rtems_status_code sc;

    sc = rtems_bdbuf_init();
    assert(sc == RTEMS_SUCCESSFUL);

    sc = rtems_media_initialize();
    assert(sc == RTEMS_SUCCESSFUL);

    sc = rtems_media_listener_add(media_listener, NULL);
    assert(sc == RTEMS_SUCCESSFUL);

    sc = rtems_media_server_initialize(
         200,
         32 * 1024,
         RTEMS_DEFAULT_MODES,
         RTEMS_DEFAULT_ATTRIBUTES);
    assert(sc == RTEMS_SUCCESSFUL);

    return sc;
}