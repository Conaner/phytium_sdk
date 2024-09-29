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
 * FilePath: fdrivers_port.h
 * Created Date: 2023-10-27 17:02:35
 * Last Modified: 2023-10-27 09:22:20
 * Description:  This file is for board layer code decoupling
 *
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0     zhugengyu  2024/08/20    first release
 */

#ifndef FDRIVERS_PORT_H
#define FDRIVERS_PORT_H

#include <rtems/score/cpu.h>

#include "fkernel.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* cache */
void FDriverDCacheRangeFlush(uintptr_t adr, size_t len);

void FDriverDCacheRangeInvalidate(uintptr_t adr, size_t len);

void FDriverICacheRangeInvalidate(uintptr_t adr, size_t len);

/* memory barrier */
#define FDRIVER_DSB() _AARCH64_Data_synchronization_barrier()

#define FDRIVER_DMB() _AARCH64_Data_memory_barrier()

#define FDRIVER_ISB() _AARCH64_Instruction_synchronization_barrier()

/* time delay */
void FDriverUdelay(uint32_t usec);

void FDriverMdelay(uint32_t msec);

void FDriverSdelay(uint32_t sec);

#define DBG_NONE        0
#define DBG_ERROR       1
#define DBG_INFO        2
#define DBG_WARNING     3
#define DBG_LOG         4

#define DBG_COLOR_RESET     "\033[0m"
#define DBG_COLOR_RED       "\033[31m"
#define DBG_COLOR_GREEN     "\033[32m"
#define DBG_COLOR_YELLOW    "\033[33m"
#define DBG_COLOR_WHITE     "\033[37m"

#define DBG_LEVEL       DBG_ERROR

int debug_print(int level, const char* format, ...);

#if (DBG_LEVEL > DBG_NONE)
#if (DBG_LEVEL >= DBG_WARNING)
#define FT_DEBUG_PRINT_W(TAG, format, ...) debug_print(DBG_WARNING, format, ##__VA_ARGS__)
#else
#define FT_DEBUG_PRINT_W(TAG, format, ...)
#endif

#if (DBG_LEVEL >= DBG_INFO)
#define FT_DEBUG_PRINT_I(TAG, format, ...) debug_print(DBG_INFO, format, ##__VA_ARGS__)
#else
#define FT_DEBUG_PRINT_I(TAG, format, ...)
#endif

#if (DBG_LEVEL >= DBG_ERROR)
#define FT_DEBUG_PRINT_E(TAG, format, ...) debug_print(DBG_ERROR, format, ##__VA_ARGS__)
#else
#define FT_DEBUG_PRINT_E(TAG, format, ...)
#endif

#if (DBG_LEVEL >= DBG_LOG)
#define FT_DEBUG_PRINT_D(TAG, format, ...) debug_print(DBG_LOG, format, ##__VA_ARGS__)
#else
#define FT_DEBUG_PRINT_D(TAG, format, ...)
#endif

#else
#define FT_DEBUG_PRINT_W(TAG, format, ...)
#define FT_DEBUG_PRINT_I(TAG, format, ...)
#define FT_DEBUG_PRINT_E(TAG, format, ...)
#define FT_DEBUG_PRINT_D(TAG, format, ...)
#endif

#ifdef __cplusplus
}
#endif

#endif