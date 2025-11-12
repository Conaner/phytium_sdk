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

#define FSDIF_MSG_LINUX_MMC_FLAGS 1U
#include "opt_sdif_v2_bm.h"

#ifdef __rtems__
#include <machine/rtems-bsd-kernel-space.h>
#endif

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/rman.h>
#include <sys/taskqueue.h>
#include <sys/time.h>

#include "phytium_sdif_v2_wrapper.h"

void
sdif_v2_prepare_init_data(FSdifMsgDataInit *data_init, boolean non_removable,
    uint32_t source_clk_hz)
{
	memset(data_init, 0U, sizeof(*data_init));

	if (non_removable) {
		data_init->caps = FSDIF_MMC_CAP_4_BIT_DATA | FSDIF_MMC_CAP_8_BIT_DATA |
		    FSDIF_MMC_CAP_MMC_HIGHSPEED | FSDIF_MMC_CAP_NONREMOVABLE |
		    FSDIF_MMC_CAP_1_8V_DDR | FSDIF_MMC_CAP_CMD23 | FSDIF_MMC_CAP_HW_RESET;
	} else {
		data_init->caps = FSDIF_MMC_CAP_4_BIT_DATA | FSDIF_MMC_CAP_SD_HIGHSPEED |
		    FSDIF_MMC_CAP_UHS | FSDIF_MMC_CAP_CMD23;
	}

	data_init->clk_rate = source_clk_hz;
}

int
sdif_v2_prepare_init_ios(FSdifMsgCtrl *hc, FSdifMsgDataSetIos *data_ios)
{
	memset(data_ios, 0U, sizeof(*data_ios));

	data_ios->ios_clock = 0U;
	data_ios->ios_timing = FSDIF_MMC_TIMING_LEGACY;
	data_ios->ios_bus_width = FSDIF_MMC_BUS_WIDTH_1;
	data_ios->ios_power_mode = FSDIF_MMC_POWER_UP;

	if (FSDIF_SUCCESS != FSdifMsgSetIos(hc, data_ios)) {
		return (ENXIO);
	}

	hc->cur_ios.ios_power_mode = FSDIF_MMC_POWER_ON;
	return 0;
}

int
sdif_v2_prepare_init_volt(FSdifMsgCtrl *hc, FSdifMsgDataSwitchVolt *data_volt,
    boolean non_removable)
{
	memset(data_volt, 0U, sizeof(*data_volt));

	if (non_removable) {
		data_volt->signal_voltage = FSDIF_MMC_SIGNAL_VOLTAGE_180;
	} else {
		data_volt->signal_voltage = FSDIF_MMC_SIGNAL_VOLTAGE_330;
	}

	if (FSDIF_SUCCESS != FSdifMsgSetVoltage(hc, data_volt)) {
		return (ENXIO);
	}

	return 0;
}

void
sdif_v2_prepare_update_voltage(int value, FSdifMsgDataSwitchVolt *data_volt)
{
	if (5 == value) { /* vdd_180 */
		data_volt->signal_voltage = FSDIF_MMC_SIGNAL_VOLTAGE_180;
	} else if ((20 == value) /* vdd_330 */ || (21 == value) /* vdd_340 */) {
		data_volt->signal_voltage = FSDIF_MMC_SIGNAL_VOLTAGE_330;
	} else {
		panic("unhandled voltage %d\n", value);
	}
}

void
sdif_v2_prepare_update_ios(int type, int value, boolean non_removable,
    FSdifMsgDataSetIos *data_ios)
{
	/* convert freebsd mmc command to iop command */
	switch (type) {
	case SDIF_IOS_POWER_MODE: /* ios->power_mode */
	{
		if (0 == value) { /* power_off */
			data_ios->ios_power_mode = FSDIF_MMC_POWER_OFF;
		} else if (1 == value) { /* power_up */
			data_ios->ios_power_mode = FSDIF_MMC_POWER_UP;
		} else if (2 == value) { /* power_on */
			data_ios->ios_power_mode = FSDIF_MMC_POWER_ON;
		} else {
			panic("unhandled ios %d/%d\n", type, value);
		}
	} break;
	case SDIF_IOS_BUS_WIDTH: /* ios->bus_width */
	{
		if (0 == value) { /* bus_width_1 */
			data_ios->ios_bus_width = FSDIF_MMC_BUS_WIDTH_1;
		} else if (2 == value) { /* bus_width_4 */
			data_ios->ios_bus_width = FSDIF_MMC_BUS_WIDTH_4;
		} else if (3 == value) { /* bus_width_8 */
			data_ios->ios_bus_width = FSDIF_MMC_BUS_WIDTH_8;
		} else {
			panic("unhandled ios %d/%d\n", type, value);
		}
	} break;
	case SDIF_IOS_TIMING: /* ios->timing */
	{
		if (0 == value) { /* bus_timing_normal */
			data_ios->ios_timing = FSDIF_MMC_TIMING_LEGACY;
		} else if (1 == value) { /* bus_timing_hs */
			if (non_removable) {
				data_ios->ios_timing = FSDIF_MMC_TIMING_MMC_HS;
			} else {
				data_ios->ios_timing = FSDIF_MMC_TIMING_SD_HS;
			}
		} else if (2 == value) { /* bus_timing_uhs_sdr12 */
			data_ios->ios_timing = FSDIF_MMC_TIMING_UHS_SDR12;
		} else if (3 == value) { /* bus_timing_uhs_sdr25 */
			data_ios->ios_timing = FSDIF_MMC_TIMING_UHS_SDR25;
		} else if (4 == value) { /* bus_timing_uhs_sdr50 */
			data_ios->ios_timing = FSDIF_MMC_TIMING_UHS_SDR50;
		} else if (5 == value) { /* bus_timing_uhs_ddr50 */
			data_ios->ios_timing = FSDIF_MMC_TIMING_UHS_DDR50;
		} else if (6 == value) { /* bus_timing_uhs_sdr104 */
			data_ios->ios_timing = FSDIF_MMC_TIMING_UHS_SDR104;
		} else if (7 == value) { /* bus_timing_mmc_ddr52 */
			data_ios->ios_timing = FSDIF_MMC_TIMING_MMC_DDR52;
		} else if (8 == value) { /* bus_timing_mmc_hs200 */
			data_ios->ios_timing = FSDIF_MMC_TIMING_MMC_HS200;
		} else if (9 == value) { /* bus_timing_mmc_hs400 */
			data_ios->ios_timing = FSDIF_MMC_TIMING_MMC_HS400;
			data_ios->ios_bus_width = FSDIF_MMC_BUS_WIDTH_8;
		} else {
			panic("unhandled ios %d/%d\n", type, value);
		}
	} break;
	case SDIF_IOS_CLOCK: /* ios->clock */
		data_ios->ios_clock = value;
		break;
	default:
		break;
	}
}

static uint32_t
sdif_v2_prepare_sd_command_flags(uint32_t opcode, uint32_t argument)
{
	uint32_t flags = 0U;

	switch (opcode) {
	case 0U: /* FSDIF_MMC_GO_IDLE_STATE 0 */
		flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_NONE | FSDIF_MMC_CMD_BC;
		break;
	case 8U: /* SD_SEND_IF_COND 8 */
		flags |= FSDIF_MMC_RSP_SPI_R7 | FSDIF_MMC_RSP_R7 | FSDIF_MMC_CMD_BCR;
		break;
	case 41U: /* SD_APP_OP_COND 41 */
		flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_R3 | FSDIF_MMC_CMD_BCR;
		break;
	case 11U: /* SD_SWITCH_VOLTAGE 11 */
		flags |= FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_AC;
		break;
	case 2U: /* FSDIF_MMC_ALL_SEND_CID 2 */
		flags |= FSDIF_MMC_RSP_R2 | FSDIF_MMC_CMD_AC;
		break;
	case 3U: /* SD_SEND_RELATIVE_ADDR 3 */
		flags |= FSDIF_MMC_RSP_R6 | FSDIF_MMC_CMD_BCR;
		break;
	case 9U: /* FSDIF_MMC_SEND_CSD 9 */
		flags |= FSDIF_MMC_RSP_R2 | FSDIF_MMC_CMD_AC;
		break;
	case 7U: /* FSDIF_MMC_SELECT_CARD 7 */
		if (argument) {
			flags |= FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_AC;
		} else {
			flags |= FSDIF_MMC_RSP_NONE | FSDIF_MMC_CMD_AC;
		}
		break;
	case 55U: /* FSDIF_MMC_APP_CMD 55 */
		if (argument) {
			flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_BCR;
		} else {
			flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_AC;
		}
		break;
	case 51U: /* SD_APP_SEND_SCR 51 */
		flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_ADTC;
		break;
	case 6U: /* SD_APP_SET_BUS_WIDTH 6 */
		flags |= FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_AC;
		break;
	case 13U: /* SD_APP_SD_STATUS 13 */
		flags |= FSDIF_MMC_RSP_SPI_R2 | FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_ADTC;
		break;
	case 16U: /* FSDIF_MMC_SET_BLOCKLEN 16 */
		flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_AC;
		break;
	case 23U: /* FSDIF_MMC_SET_BLOCK_COUNT 23 */
		flags |= FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_AC;
		break;
	case 24U: /* FSDIF_MMC_WRITE_BLOCK 24 */
		flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_ADTC;
		break;
	case 25U: /* FSDIF_MMC_WRITE_MULTIPLE_BLOCK 25 */
		flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_ADTC;
		break;
	case 17U: /* FSDIF_MMC_READ_SINGLE_BLOCK 17 */
		flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_ADTC;
		break;
	case 18U: /* FSDIF_MMC_READ_MULTIPLE_BLOCK 18 */
		flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_ADTC;
		break;
	default:
		panic("unhandled command-%d !!!\n", opcode);
		break;
	}

	return flags;
}

static uint32_t
sdif_v2_prepare_mmc_command_flags(uint32_t opcode, uint32_t argument)
{
	uint32_t flags = 0U;

	switch (opcode) {
	case 0U: /* FSDIF_MMC_GO_IDLE_STATE 0 */
		flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_NONE | FSDIF_MMC_CMD_BC;
		break;
	case 1U: /* FSDIF_MMC_SEND_OP_COND 1 */
		flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_R3 | FSDIF_MMC_CMD_BCR;
		break;
	case 2U: /* FSDIF_MMC_ALL_SEND_CID 2 */
		flags |= FSDIF_MMC_RSP_R2 | FSDIF_MMC_CMD_AC;
		break;
	case 3U: /* FSDIF_MMC_SET_RELATIVE_ADDR 3 */
		flags |= FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_AC;
		break;
	case 9U: /* FSDIF_MMC_SEND_CSD 9 */
		flags |= FSDIF_MMC_RSP_R2 | FSDIF_MMC_CMD_AC;
		break;
	case 7U: /* FSDIF_MMC_SELECT_CARD 7 */
		if (argument) {
			flags |= FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_AC;
		} else {
			flags |= FSDIF_MMC_RSP_NONE | FSDIF_MMC_CMD_AC;
		}
		break;
	case 8U: /* FSDIF_MMC_SEND_EXT_CSD 8 */
		flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_ADTC;
		break;
	case 6U: /* FSDIF_MMC_SWITCH 6 */
		flags |= FSDIF_MMC_CMD_AC | FSDIF_MMC_RSP_SPI_R1B | FSDIF_MMC_RSP_R1B;
		break;
	case 13U: /* FSDIF_MMC_SEND_STATUS 13 */
		flags |= FSDIF_MMC_RSP_SPI_R2 | FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_AC;
		break;
	case 16U: /* FSDIF_MMC_SET_BLOCKLEN 16 */
		flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_AC;
		break;
	case 23U: /* FSDIF_MMC_SET_BLOCK_COUNT 23 */
		flags |= FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_AC;
		break;
	case 24U: /* FSDIF_MMC_WRITE_BLOCK 24 */
		flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_ADTC;
		break;
	case 25U: /* FSDIF_MMC_WRITE_MULTIPLE_BLOCK 25 */
		flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_ADTC;
		break;
	case 17U: /* FSDIF_MMC_READ_SINGLE_BLOCK 17 */
		flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_ADTC;
		break;
	case 18U: /* FSDIF_MMC_READ_MULTIPLE_BLOCK 18 */
		flags |= FSDIF_MMC_RSP_SPI_R1 | FSDIF_MMC_RSP_R1 | FSDIF_MMC_CMD_ADTC;
		break;
	default:
		panic("unhandled command-%d !!!\n", opcode);
		break;
	}

	return flags;
}

static uint32_t
sdif_v2_prepare_command_flags(uint32_t opcode, uint32_t argument,
    boolean non_removable)
{
	return ((non_removable) ?
		sdif_v2_prepare_mmc_command_flags(opcode, argument) :
		sdif_v2_prepare_sd_command_flags(opcode, argument));
}

static uint32_t
sdif_v2_prepare_raw_command(uint32_t opcode, uint32_t argument,
    boolean non_removable, boolean with_data, boolean write_data)
{
	uint32_t raw_cmd = FSDIF_CMD_INDX_SET(opcode);
	uint32_t flags = sdif_v2_prepare_command_flags(opcode, argument,
	    non_removable);

	if (0U == opcode) { /* FSDIF_MMC_GO_IDLE_STATE 0 */
		raw_cmd |= FSDIF_CMD_INIT;
	}

	if (FSDIF_MMC_RSP_PRESENT & flags) {
		raw_cmd |= FSDIF_CMD_RESP_EXP;

		if (FSDIF_MMC_RSP_136 & flags) {
			/* need 136 bits long response */
			raw_cmd |= FSDIF_CMD_RESP_LONG;
		}

		if (FSDIF_MMC_RSP_CRC & flags) {
			/* most cmds need CRC */
			raw_cmd |= FSDIF_CMD_RESP_CRC;
		}
	}

	if (!(non_removable) && (11U == opcode)) { /* SD_SWITCH_VOLTAGE 11 */
		/* CMD11 need switch voltage */
		raw_cmd |= FSDIF_CMD_VOLT_SWITCH;
	}

	if (with_data) {
		raw_cmd |= FSDIF_CMD_DAT_EXP;

		if (write_data) {
			raw_cmd |= FSDIF_CMD_DAT_WRITE;
		}
	}

	return raw_cmd |= FSDIF_CMD_START;
}

void
sdif_v2_prepare_data_transfer(FSdifMsgDataStartData *data, uint32_t opcode,
    uint32_t argument, boolean non_removable, boolean write_data,
    uint32_t blksz, uint32_t blkcnt)
{
	memset(data, 0U, sizeof(*data));

	data->cmd_arg = argument;
	data->raw_cmd = sdif_v2_prepare_raw_command(opcode, argument,
	    non_removable, TRUE, write_data);
	data->data_flags = ((write_data) ? FSDIF_MMC_DATA_WRITE : FSDIF_MMC_DATA_READ);
	data->adtc_type = FSDIF_BLOCK_RW_ADTC;
	data->adma_addr = 0U; /* we do not know the descriptor addr here */
	data->mrq_data_blksz = blksz;
	data->mrq_data_blocks = blkcnt;
}

void
sdif_v2_prepare_command_transfer(FSdifMsgDataStartCmd *cmd, uint32_t opcode,
    uint32_t argument, boolean non_removable)
{
	memset(cmd, 0U, sizeof(*cmd));

	cmd->opcode = opcode;
	cmd->cmd_arg = argument;
	cmd->raw_cmd = sdif_v2_prepare_raw_command(opcode, argument,
	    non_removable, FALSE, FALSE);
	cmd->flags = sdif_v2_prepare_command_flags(opcode, argument,
	    non_removable);
}
