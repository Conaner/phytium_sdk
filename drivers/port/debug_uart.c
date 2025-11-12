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
 * FilePath: debug_uart.c
 * Created Date: 2023-10-27 17:02:35
 * Last Modified: 2023-10-27 09:22:20
 * Description:  This file is for RTEMS driver debug printf
 *
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0     zhugengyu  2024/08/20    first release
 */

#include <bsp.h>
#include "ftypes.h"
#include "fio.h"

#define EARLY_UART_BASE BSP_PHYTIUM_CONSOLE_UART_BASE

#define EARLY_UART_UARTDR (EARLY_UART_BASE + 0x0)
#define EARLY_UART_UARTFR (EARLY_UART_BASE + 0x18)

#define EARLY_UART_TXFF (1 << 5)
#define EARLY_UART_DATA_MASK 0xff

void _putchar(char byte)
{
    /* wait until tx fifo is not full */
    while ((FtIn32(EARLY_UART_UARTFR) & EARLY_UART_TXFF) == EARLY_UART_TXFF)
    {

    }

    FtOut32(EARLY_UART_UARTDR, (((u32)byte) & EARLY_UART_DATA_MASK));
}
