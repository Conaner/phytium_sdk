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
 * FilePath: fdrivers_port.c
 * Created Date: 2023-10-27 17:02:35
 * Last Modified: 2023-10-27 09:22:20
 * Description:  This file is for board layer code decoupling
 *
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0     zhugengyu  2024/08/20    first release
 */

/***************************** Include Files *********************************/
#include <string.h>
#include <unistd.h>
#include <rtems.h>
#include "fdrivers_port.h"

/* libc utilies */
void
MEMSET(void *buff, int c, size_t n)
{
	(void)memset(buff, c, n);
}

void
MEMCPY(void *dest, void *src, size_t size)
{
	(void)memcpy(dest, src, size);
}

void FDriverUdelay(uint32_t usec)
{
    rtems_interval ticks = rtems_clock_tick_later_usec(usec);
    //rtems_task_wake_after(ticks);
    while (rtems_clock_tick_before(ticks)) {
        /* Wait */
    }
}

void FDriverMdelay(uint32_t msec)
{
    rtems_interval ticks = rtems_clock_tick_later_usec(msec * 1000);
    //rtems_task_wake_after(ticks);
    while (rtems_clock_tick_before(ticks)) {
        /* Wait */
    }
}

void FDriverSdelay(uint32_t sec)
{
    rtems_interval ticks = rtems_clock_tick_later_usec(sec * 1000 * 1000);
    //rtems_task_wake_after(ticks);
    while (rtems_clock_tick_before(ticks)) {
        /* Wait */
    }
}

#define __is_print(ch) ((unsigned int)((ch) - ' ') < 127u - ' ')
void FtDumpHexWord(const u32 *ptr, u32 buflen)
{
    u32 *buf = (u32 *)ptr;
    u8 *char_data = (u8 *)ptr;
    fsize_t i, j;
    buflen = buflen / 4;
    for (i = 0; i < buflen; i += 4)
    {
        printf("%p: ", ptr + i);

        for (j = 0; j < 4; j++)
        {
            if (i + j < buflen)
            {
                printf("%x ", buf[i + j]);
            }
            else
            {
                printf("   ");
            }
        }

        printf(" ");

        for (j = 0; j < 16; j++)
        {
            if (i + j < buflen)
            {
                printf("%c", (char)(__is_print(char_data[i + j]) ? char_data[i + j] : '.'));
            }
        }

        printf("\r\n");
    }    
}