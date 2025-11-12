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

#ifndef _PHYTIUM_SDIF_VAR_H_
#define _PHYTIUM_SDIF_VAR_H_

struct phytium_sdif_softc {
	device_t dev;
#ifdef __rtems__
	rtems_id task_id;
#endif
	struct mtx sc_mtx;
	struct resource *res[2];
	void *intr_cookie;
	struct mmc_host host;
#ifndef __rtems__
	struct mmc_helper mmc_helper;
#endif
	struct mmc_request *req;
	struct mmc_command *curcmd;

	device_t child;
	struct task card_task;		       /* Card presence check task */
	struct timeout_task card_delayed_task; /* Card insert delayed task */

	int bus_busy;
	volatile int dto_rcvd;
	volatile int cmd_done;
	volatile int req_pending;
	int expect_data;
	struct callout timeout_callout; /* Card command/data response timeout */

	uint32_t use_hold;
	uint32_t pre_fix_timing;
	uint32_t auto_stop;

	uint32_t card_clock;
	uint32_t power_mode;
	uint32_t bus_width;
	uint32_t voltage;

#ifndef __rtems__
	void (*mmc_set_power)(struct mmc_helper *helper,
	    enum mmc_power_mode power_mode);
#endif

	/* FSdif */
	FSdif hc;
	FSdifConfig hc_cfg;
	FSdifCmdData cmd_pkg;
	FSdifData dat_pkg;
	const FSdifTiming *cur_timing;

	bus_dma_tag_t desc_tag;
	bus_dmamap_t desc_map;
	FSdifIDmaDesc *desc_ring;
	bus_addr_t desc_ring_paddr;
	bus_dma_tag_t buf_tag;
	bus_dmamap_t buf_map;

#ifdef __rtems__
	void *aligned_dma_buf;
	size_t aligned_dma_buf_len;
#endif
};

DECLARE_CLASS(sdif_driver);

#ifdef DEBUG
#define dprintf(fmt, args...) printf(fmt, ##args)
#else
#define dprintf(x, arg...)
#endif

#endif