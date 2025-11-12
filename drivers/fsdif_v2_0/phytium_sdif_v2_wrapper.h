/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2024 Phytium Technology Co., Ltd. All rights reserved.
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
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2025/05/06    init commit
 */

#ifndef _PHYTIUM_SDIF_V2_WRAPPER_H_
#define _PHYTIUM_SDIF_V2_WRAPPER_H_

void sdif_v2_prepare_init_data(FSdifMsgDataInit *data_init,
    boolean non_removable, uint32_t source_clk_hz);
int sdif_v2_prepare_init_ios(FSdifMsgCtrl *hc, FSdifMsgDataSetIos *data_ios);
int sdif_v2_prepare_init_volt(FSdifMsgCtrl *hc,
    FSdifMsgDataSwitchVolt *data_volt, boolean non_removable);
#define SDIF_IOS_POWER_MODE 0U
#define SDIF_IOS_BUS_WIDTH  1U
#define SDIF_IOS_TIMING	    2U
#define SDIF_IOS_CLOCK	    3U
void sdif_v2_prepare_update_ios(int type, int value, boolean non_removable,
    FSdifMsgDataSetIos *data_ios);
void sdif_v2_prepare_update_voltage(int value,
    FSdifMsgDataSwitchVolt *data_volt);
void sdif_v2_prepare_data_transfer(FSdifMsgDataStartData *data, uint32_t opcode,
    uint32_t argument, boolean non_removable, boolean write_data,
    uint32_t blksz, uint32_t blkcnt);
void sdif_v2_prepare_command_transfer(FSdifMsgDataStartCmd *cmd,
    uint32_t opcode, uint32_t argument, boolean non_removable);

#endif