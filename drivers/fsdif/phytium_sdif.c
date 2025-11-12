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
/***************************************************/
#include "opt_sdif_bm.h"

#ifdef __rtems__
#include <machine/rtems-bsd-kernel-space.h>

#include <rtems/bsd/local/opt_platform.h>
#endif

#ifndef __rtems__
#include "opt_mmccam.h"
#endif

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
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

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <dev/extres/clk/clk.h>
#include <dev/mmc/bridge.h>
#ifndef __rtems__
#include <dev/mmc/mmc_helpers.h>
#endif
#include <dev/mmc/mmcbrvar.h>
#include <dev/mmc/mmcreg.h>

#ifdef __rtems__
#include <bsp.h>
#include <rtems.h>
#include <rtems/bsd/local/mmcbr_if.h>
#include <rtems/irq-extension.h>
#else
#include "mmcbr_if.h"
#endif

#ifndef __rtems__
#include "opt_ddb.h"
#endif

#ifdef DDB
#include <ddb/ddb.h>

#define PHYTIUM_MAX_DDB_SC 4U
struct phytium_sdif_softc;
static struct phytium_sdif_softc *sdif_sc_ddb[PHYTIUM_MAX_DDB_SC] = { NULL };
#endif

#include "phytium_sdif_var.h"

static void
SDIF_LOCK(struct phytium_sdif_softc *sc)
{
	mtx_lock(&sc->sc_mtx);
#ifdef __rtems__
	sc->task_id = rtems_task_self();
#endif
}

#define SDIF_UNLOCK(_sc) mtx_unlock(&(_sc)->sc_mtx)
#define SDIF_LOCK_INIT(_sc)                                                  \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), "phytium_mmc", \
	    MTX_DEF)
#define SDIF_LOCK_DESTROY(_sc)	  mtx_destroy(&_sc->sc_mtx);
#define SDIF_ASSERT_LOCKED(_sc)	  mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define SDIF_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

#define IDMAC_DESC_SEGS		  1024U
#define IDMAC_DESC_SIZE		  (sizeof(FSdifIDmaDesc) * IDMAC_DESC_SEGS)

/*
 * Size field in DMA descriptor is 13 bits long (up to 4095 bytes),
 * but must be a multiple of the data bus size.Additionally, we must ensure
 * that bus_dmamap_load() doesn't additionally fragments buffer (because it
 * is processed with page size granularity). Thus limit fragment size to half
 * of page.
 * XXX switch descriptor format to array and use second buffer pointer for
 * second half of page
 */
#define IDMAC_MAX_SIZE 2048
/*
 * Busdma may bounce buffers, so we must reserve 2 descriptors
 * (on start and on end) for bounced fragments.
 */
#define SDIF_MAX_DATA	  (IDMAC_MAX_SIZE * (IDMAC_DESC_SEGS - 2)) / MMC_SECTOR_SIZE
#define SDIF_MAX_PIO_DATA (FSDIF_MAX_FIFO_CNT / MMC_SECTOR_SIZE)

#define SDIF_TIME_OUT	  6000000U /* 6s */

static struct resource_spec sdif_spec[] = { { SYS_RES_MEMORY, 0, RF_ACTIVE },
	{ SYS_RES_IRQ, 0, RF_ACTIVE }, { -1, 0 } };

static int
sdif_polling_card_busy(struct phytium_sdif_softc *sc, int timeout_us)
{
	int wait_counter = 0;

	while (FSdifCheckCardBusy(&sc->hc)) {
		if (wait_counter > timeout_us) {
			device_printf(sc->dev,
			    "Failed to wait card not busy\n");
			return EBUSY;
		}

		/* busy waiting here will actually faster */
		DELAY(100); /* wait 100us here */
		wait_counter += 100;
	}

	return 0;
}

static int
sdif_polling_request_done(struct phytium_sdif_softc *sc, int timeout_us)
{
	int wait_counter = 0;

	while (1) {
		if (!sc->req_pending) {
			return 0;
		}

		if (wait_counter++ > timeout_us) {
			device_printf(sc->dev,
			    "Wait command-%d response timeout %d %d\n",
			    sc->cmd_pkg.cmdidx, sc->cmd_done, sc->dto_rcvd);
			if (sc->cmd_pkg.data_p) {
				device_printf(sc->dev,
				    "Transfer %d blocks with buffer@%p failed\n",
				    sc->cmd_pkg.data_p->blkcnt,
				    sc->cmd_pkg.data_p->buf);
			}
			return EIO;
		}

		DELAY(1);
	}

	return 0;
}

static void
sdif_handle_card_present(struct phytium_sdif_softc *sc, bool is_present)
{
	bool was_present;

	if (dumping || SCHEDULER_STOPPED())
		return;

	was_present = sc->child != NULL;

	if (!was_present && is_present) {
		taskqueue_enqueue_timeout(taskqueue_swi_giant,
		    &sc->card_delayed_task, -(hz / 2));
	} else if (was_present && !is_present) {
		taskqueue_enqueue(taskqueue_swi_giant, &sc->card_task);
	}
}

static void
sdif_card_task(void *arg, int pending __unused)
{
	struct phytium_sdif_softc *sc = arg;

#ifdef MMCCAM
	mmc_cam_sim_discover(&sc->mmc_sim);
#else
	SDIF_LOCK(sc);

	if (FSdifCheckCardExists(&sc->hc)) {
		if (sc->child == NULL) {
			if (bootverbose)
				device_printf(sc->dev, "Card inserted !\n");

			sc->child = device_add_child(sc->dev, "mmc", -1);
			SDIF_UNLOCK(sc);
			if (sc->child) {
				device_set_ivars(sc->child, sc);
				(void)device_probe_and_attach(sc->child);
			}
		} else {
			SDIF_UNLOCK(sc);
		}
	} else {
		/* Card isn't present, detach if necessary */
		if (sc->child != NULL) {
			if (bootverbose)
				device_printf(sc->dev, "Card removed !\n");

			SDIF_UNLOCK(sc);
			device_delete_child(sc->dev, sc->child);
			sc->child = NULL;
		} else {
			SDIF_UNLOCK(sc);
		}
	}
#endif /* MMCCAM */
}

static void
sdif_dma_get1paddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	if (nsegs != 1)
		panic("%s: nsegs != 1 (%d)\n", __func__, nsegs);
	if (error != 0)
		panic("%s: error != 0 (%d)\n", __func__, error);

	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static int
sdif_dma_setup(struct phytium_sdif_softc *sc)
{
	int error;
	int nidx;
	int idx;
	bus_addr_t desc_paddr;

#ifndef __rtems__
	/*
	 * Set up TX descriptor ring, descriptors, and dma maps.
	 */
	error = bus_dma_tag_create(bus_get_dma_tag(sc->dev), /* Parent tag. */
	    4096, 0,		     /* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT, /* lowaddr */
	    BUS_SPACE_MAXADDR,	     /* highaddr */
	    NULL, NULL,		     /* filter, filterarg */
	    IDMAC_DESC_SIZE, 1,	     /* maxsize, nsegments */
	    IDMAC_DESC_SIZE,	     /* maxsegsize */
	    0,			     /* flags */
	    NULL, NULL,		     /* lockfunc, lockarg */
	    &sc->desc_tag);
	if (error != 0) {
		device_printf(sc->dev, "could not create ring DMA tag.\n");
		return (1);
	}

	error = bus_dmamem_alloc(sc->desc_tag, (void **)&sc->desc_ring,
	    BUS_DMA_COHERENT | BUS_DMA_WAITOK | BUS_DMA_ZERO, &sc->desc_map);
	if (error != 0) {
		device_printf(sc->dev, "could not allocate descriptor ring.\n");
		return (1);
	}

	error = bus_dmamap_load(sc->desc_tag, sc->desc_map, sc->desc_ring,
	    IDMAC_DESC_SIZE, sdif_dma_get1paddr, &sc->desc_ring_paddr, 0);
	if (error != 0) {
		device_printf(sc->dev, "could not load descriptor ring map.\n");
		return (1);
	}

	for (idx = 0; idx < IDMAC_DESC_SEGS; idx++) {
		sc->desc_ring[idx].attribute = FSDIF_IDMAC_DES0_CH;
		sc->desc_ring[idx].len = 0;
		nidx = (idx + 1) % IDMAC_DESC_SEGS;
		desc_paddr = sc->desc_ring_paddr +
		    (nidx * sizeof(FSdifIDmaDesc));
		sc->desc_ring[idx].desc_lo = LOWER_32_BITS(desc_paddr);
		sc->desc_ring[idx].desc_hi = UPPER_32_BITS(desc_paddr);
	}
	sc->desc_ring[idx - 1].desc_lo = LOWER_32_BITS(sc->desc_ring_paddr);
	sc->desc_ring[idx - 1].desc_hi = UPPER_32_BITS(sc->desc_ring_paddr);
	sc->desc_ring[idx - 1].attribute |= FSDIF_IDMAC_DES0_ER;

	error = bus_dma_tag_create(bus_get_dma_tag(sc->dev), /* Parent tag. */
	    8, 0,			      /* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	      /* lowaddr */
	    BUS_SPACE_MAXADDR,		      /* highaddr */
	    NULL, NULL,			      /* filter, filterarg */
	    IDMAC_MAX_SIZE * IDMAC_DESC_SEGS, /* maxsize */
	    IDMAC_DESC_SEGS,		      /* nsegments */
	    IDMAC_MAX_SIZE,		      /* maxsegsize */
	    0,				      /* flags */
	    NULL, NULL,			      /* lockfunc, lockarg */
	    &sc->buf_tag);
	if (error != 0) {
		device_printf(sc->dev, "could not create ring DMA tag.\n");
		return (1);
	}

	error = bus_dmamap_create(sc->buf_tag, 0, &sc->buf_map);
	if (error != 0) {
		device_printf(sc->dev, "could not create TX buffer DMA map.\n");
		return (1);
	}
#else
	sc->desc_ring = (void *)rtems_cache_coherent_allocate(IDMAC_DESC_SIZE,
	    MMC_SECTOR_SIZE, 0U);
	if (sc->desc_ring == NULL) {
		device_printf(sc->dev,
		    "could not create buffer DMA descriptor.\n");
		return (1);
	}

	sc->aligned_dma_buf_len = SDIF_MAX_DATA * MMC_SECTOR_SIZE;
	sc->aligned_dma_buf = (void *)rtems_cache_coherent_allocate(
	    sc->aligned_dma_buf_len, FSDIF_DMA_BUF_ALIGMENT, 0U);
	if (sc->aligned_dma_buf == NULL) {
		device_printf(sc->dev,
		    "could not create aligned buffer DMA.\n");
		return (1);
	}

	sc->desc_ring_paddr = (uintptr_t)sc->desc_ring;
#endif

	return (0);
}

static void
sdif_dma_destory(struct phytium_sdif_softc *sc)
{
#ifdef __rtems__
	rtems_cache_coherent_free(sc->aligned_dma_buf);
	sc->aligned_dma_buf = NULL;
	sc->aligned_dma_buf_len = 0U;
	rtems_cache_coherent_free(sc->desc_ring);
	sc->desc_ring = NULL;
	sc->desc_ring_paddr = 0U;
#endif
}

static int
sdif_dma_done(struct phytium_sdif_softc *sc, struct mmc_command *cmd)
{
#ifndef __rtems__
	struct mmc_data *data;

	data = cmd->data;

	if (data->flags & MMC_DATA_WRITE)
		bus_dmamap_sync(sc->buf_tag, sc->buf_map,
		    BUS_DMASYNC_POSTWRITE);
	else
		bus_dmamap_sync(sc->buf_tag, sc->buf_map, BUS_DMASYNC_POSTREAD);

	bus_dmamap_sync(sc->desc_tag, sc->desc_map, BUS_DMASYNC_POSTWRITE);

	bus_dmamap_unload(sc->buf_tag, sc->buf_map);
#endif

	return (0);
}

static int
sdif_dma_stop(struct phytium_sdif_softc *sc)
{
	return 0;
}

#ifndef __rtems__
static void
sdif_ring_setup(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct phytium_sdif_softc *sc;
	int idx;

	sc = arg;
	dprintf("nsegs %d seg0len %lu\n", nsegs, segs[0].ds_len);
	if (error != 0)
		panic("%s: error != 0 (%d)\n", __func__, error);

	for (idx = 0; idx < nsegs; idx++) {
		sc->desc_ring[idx].attribute = FSDIF_IDMAC_DES0_DIC |
		    FSDIF_IDMAC_DES0_CH;
		sc->desc_ring[idx].len = segs[idx].ds_len &
		    (FSDIF_IDMAC_MAX_BUF_SIZE - 1);

		if (segs[idx].ds_addr % FSDIF_DMA_BUF_ALIGMENT)
			panic("%s: unaligned dma buffer (0x%lx)\n", __func__,
			    segs[idx].ds_addr);

		sc->desc_ring[idx].addr_lo = LOWER_32_BITS(segs[idx].ds_addr);
		sc->desc_ring[idx].addr_hi = UPPER_32_BITS(segs[idx].ds_addr);

		if (idx == 0)
			sc->desc_ring[idx].attribute |= FSDIF_IDMAC_DES0_FD;

		if (idx == (nsegs - 1)) {
			sc->desc_ring[idx].attribute &= ~(
			    FSDIF_IDMAC_DES0_DIC | FSDIF_IDMAC_DES0_CH);
			sc->desc_ring[idx].attribute |= FSDIF_IDMAC_DES0_LD;
		}
		wmb();
		sc->desc_ring[idx].attribute |= FSDIF_IDMAC_DES0_OWN;
	}
}
#endif

static int
sdif_dma_prepare(struct phytium_sdif_softc *sc, FSdifData *trans_data,
    struct mmc_data *in_data)
{
	int err = 0;

#ifndef __rtems__
	if (in_data->data) {
		err = bus_dmamap_load(sc->buf_tag, sc->buf_map, in_data->data,
		    in_data->len, sdif_ring_setup, sc, BUS_DMA_NOWAIT);
		if (err != 0)
			panic("dmamap_load failed\n");

		/* Ensure the device can see the desc */
		bus_dmamap_sync(sc->desc_tag, sc->desc_map,
		    BUS_DMASYNC_PREWRITE);

		if (MMC_DATA_WRITE & in_data->flags)
			bus_dmamap_sync(sc->buf_tag, sc->buf_map,
			    BUS_DMASYNC_PREWRITE);
		else
			bus_dmamap_sync(sc->buf_tag, sc->buf_map,
			    BUS_DMASYNC_PREREAD);
	} else {
		panic("no data\n");
	}
#else
	if ((uintptr_t)in_data->data % FSDIF_DMA_BUF_ALIGMENT) {
		if (in_data->len > sc->aligned_dma_buf_len) {
			panic("request data length %d > aligned buf length %d",
			    in_data->len, sc->aligned_dma_buf_len);
		}

		if (MMC_DATA_WRITE & in_data->flags) {
			memcpy(sc->aligned_dma_buf, in_data->data,
			    in_data->len);
		} else {
			memset(sc->aligned_dma_buf, 0U, in_data->len);
		}

		trans_data->buf = sc->aligned_dma_buf;
		trans_data->buf_dma = (uintptr_t)sc->aligned_dma_buf;
	} else {
		trans_data->buf = in_data->data;
		trans_data->buf_dma = (uintptr_t)in_data->data;
	}

	if (FSDIF_SUCCESS != FSdifSetupDMADescriptor(&sc->hc, trans_data)) {
		device_printf(sc->dev, "setup DMA descriptor failed\n");
		return (ENXIO);
	}

	dprintf("%s %ld data with buf@%p\n",
	    (MMC_DATA_WRITE & in_data->flags) ? "write" : "read", in_data->len,
	    trans_data->buf);
#endif

	return err;
}

static int
sdif_set_block_count(struct phytium_sdif_softc *sc, struct mmc_request *req)
{
	device_t brdev = sc->dev;
	FSdifCmdData *cmd_data = &sc->cmd_pkg;
	if ((req == NULL) || (req->cmd->data) == NULL)
		panic("invalid command request !!!");
	size_t block_cnt = req->cmd->data->len / MMC_SECTOR_SIZE;
	int err = 0;
	FError ferr;

	memset(cmd_data, 0U, sizeof(*cmd_data));

	cmd_data->cmdidx = MMC_SET_BLOCK_COUNT;
	cmd_data->cmdarg = ((u32)block_cnt & 0x0000FFFF);
	if (sc->hc_cfg.non_removable) {
		/* part->type == EXT_CSD_PART_CONFIG_ACC_RPMB */
		cmd_data->cmdarg |= (1 << 31); /* reliable */
	}
	/* MMC_RSP_R1 */
	cmd_data->rawcmd = FSDIF_CMD_RESP_EXP | FSDIF_CMD_RESP_CRC |
	    FSDIF_CMD_INDX_SET(MMC_SET_BLOCK_COUNT);
	if (sc->use_hold) {
		cmd_data->rawcmd |= FSDIF_CMD_USE_HOLD_REG;
	}
	cmd_data->data_p = NULL;

	sc->dto_rcvd = 0U; /* data transfer over */
	sc->cmd_done = 0U; /* command finished */
	sc->req_pending = 1U;
	sc->expect_data = 0U;

	if (sc->hc.config.trans_mode == FSDIF_PIO_TRANS_MODE) {
		ferr = FSdifPIOTransfer(&sc->hc, cmd_data);
		if (FSDIF_SUCCESS != ferr) {
			device_printf(brdev,
			    "Failed to start PIO transfer(set_block) ferr=0x%x\n",
			    ferr);
			err = (ENXIO);
			goto exit;
		}

	} else {
		ferr = FSdifDMATransfer(&sc->hc, cmd_data);
		if (FSDIF_SUCCESS != ferr) {
			device_printf(brdev,
			    "Failed to start DMA transfer(set_block) ferr=0x%x\n",
			    ferr);
			err = (ENXIO);
			goto exit;
		}
	}

	err = sdif_polling_request_done(sc, SDIF_TIME_OUT);
	if (err) {
		goto exit;
	}

	err = sdif_polling_card_busy(sc, SDIF_TIME_OUT);
	if (err) {
		device_printf(brdev, "card busy wait timeout\n");
		goto exit;
	}

exit:
	return (err);
}

static int
sdif_pre_request(struct phytium_sdif_softc *sc, struct mmc_request *req)
{
	int err = 0;

	if ((!sc->auto_stop) &&
	    ((MMC_READ_MULTIPLE_BLOCK == req->cmd->opcode) ||
		(MMC_WRITE_MULTIPLE_BLOCK == req->cmd->opcode))) {
		if (req->cmd->data) {
			err = sdif_set_block_count(sc, req);
		}
	}

	if (sc->hc.config.non_removable) {
		/* ignore micro SD detect command, not in eMMC spec. */
		if ((ACMD_SD_SEND_OP_COND == req->cmd->opcode) ||
		    (MMC_APP_CMD == req->cmd->opcode)) {
			err = (ECANCELED);
		}

		/* ignore mmcsd_send_if_cond(CMD-8) which will failed for eMMC
		   but check cmd arg to let SEND_EXT_CSD (CMD-8) run */
		if ((SD_SEND_IF_COND == req->cmd->opcode) &&
		    (0x1aa == req->cmd->arg)) {
			err = (ECANCELED);
		}
	}

	return err;
}

static void
phytium_sdif_timeout(void *arg)
{
	struct phytium_sdif_softc *sc = (struct phytium_sdif_softc *)arg;
	struct mmc_request *req;

	if (sc->curcmd != NULL) {
		device_printf(sc->dev, "Controller timeout\n");
		req = sc->req;
		if (req == NULL) {
			panic("Spurious comand - no request\n");
		}

		/* if command finished with error, reset the controller */
		if (FSDIF_IDMA_TRANS_MODE == sc->hc.config.trans_mode) {
			sdif_dma_done(sc, req->cmd);
			sdif_dma_stop(sc);
		}

		FSdifRestart(&sc->hc);

		req->cmd->error = MMC_ERR_TIMEOUT;
		(*req->done)(req);
	} else {
#ifndef __rtems__
		/* callout in rtems seems not able to stop */
		device_printf(sc->dev,
		    "Spurious timeout - no active command\n");
#endif
	}
}

static uint32_t
sdif_prepare_raw_cmd(struct phytium_sdif_softc *sc, struct mmc_request *req)
{
	struct mmc_command *command = req->cmd;
	uint32_t opcode = command->opcode;

	u32 raw_cmd = FSDIF_CMD_INDX_SET(opcode);

	if (MMC_GO_INACTIVE_STATE == opcode) {
		raw_cmd |= FSDIF_CMD_STOP_ABORT;
	}

	if (FSDIF_SWITCH_VOLTAGE == opcode) {
		raw_cmd |= FSDIF_CMD_VOLT_SWITCH;
	}

	if (sc->hc.card_need_init) {
		raw_cmd |= FSDIF_CMD_INIT;
		sc->hc.card_need_init = FALSE;
	}

	/* Set up response handling. */
	if (MMC_RSP_PRESENT & command->flags) {
		raw_cmd |= FSDIF_CMD_RESP_EXP;
		if (MMC_RSP_136 & command->flags) {
			raw_cmd |= FSDIF_CMD_RESP_LONG;
		}
	}

	if (MMC_RSP_CRC & command->flags) {
		raw_cmd |= FSDIF_CMD_RESP_CRC;
	}

	if (command->data) {
		raw_cmd |= FSDIF_CMD_DAT_EXP;

		if (MMC_DATA_WRITE & command->data->flags) {
			raw_cmd |= FSDIF_CMD_DAT_WRITE;
		}

		if ((sc->auto_stop) &&
		    ((MMC_READ_MULTIPLE_BLOCK == command->opcode) ||
			(MMC_WRITE_MULTIPLE_BLOCK == command->opcode))) {
			raw_cmd |= FSDIF_CMD_SEND_STOP;
		}
	}

	if (sc->use_hold) {
		raw_cmd |= FSDIF_CMD_USE_HOLD_REG;
	}

	return raw_cmd;
}

static int
phytium_sdif_request(device_t brdev, device_t reqdev, struct mmc_request *req)
{
	struct phytium_sdif_softc *sc = device_get_softc(brdev);
	FSdifCmdData *cmd = &(sc->cmd_pkg);
	FSdifData *cmd_data = &(sc->dat_pkg);
	struct mmc_data *in_data = NULL;
	int err = 0;
	FError ferr;

	SDIF_LOCK(sc);

	err = sdif_polling_card_busy(sc, SDIF_TIME_OUT);
	if (err) {
		device_printf(brdev, "card busy wait timeout\n");
		goto err_exit;
	}

	err = sdif_pre_request(sc, req);
	if (err) {
		dprintf("*skip command-%d arg-0x%x \n", req->cmd->opcode,
		    req->cmd->arg);
		(*req->done)(req);
		goto err_exit;
	}

	sc->req = req;
	sc->curcmd = req->cmd;
	req->cmd->error = MMC_ERR_NONE;

	memset(cmd, 0U, sizeof(*cmd));

	cmd->cmdidx = req->cmd->opcode;
	cmd->cmdarg = req->cmd->arg;
	cmd->rawcmd = sdif_prepare_raw_cmd(sc, req);

	if (req->cmd->data) {
		in_data = req->cmd->data;

		memset(cmd_data, 0U, sizeof(*cmd_data));

		cmd_data->buf = in_data->data;
		cmd_data->blksz = (in_data->len < FSDIF_BLOCK_SIZE) ?
		    in_data->len :
		    FSDIF_BLOCK_SIZE;
		cmd_data->blkcnt = ((in_data->len / FSDIF_BLOCK_SIZE) < 1U) ?
		    1U :
		    (in_data->len / FSDIF_BLOCK_SIZE);
		cmd_data->datalen = in_data->len;
		cmd->data_p = cmd_data;

		dprintf("transfer data@%p len=%zu with command-%d arg-0x%x\n",
		    req->cmd->data, req->cmd->data->len, req->cmd->opcode,
		    req->cmd->arg);
	} else {
		dprintf("transfer command-%d arg-0x%x\n", req->cmd->opcode,
		    req->cmd->arg);
	}

	sc->dto_rcvd = 0U; /* data transfer over */
	sc->cmd_done = 0U; /* command finished */
	sc->req_pending = 1U;
	sc->expect_data = !!(in_data);

	if (sc->hc.config.trans_mode == FSDIF_PIO_TRANS_MODE) {
		ferr = FSdifPIOTransfer(&sc->hc, cmd);
		if (FSDIF_SUCCESS != ferr) {
			device_printf(brdev,
			    "Failed to start PIO transfer, ferr=0x%x\n", ferr);
			err = (ENXIO);
			req->cmd->error = MMC_ERR_FAILED;
			goto err_exit;
		}
	} else {
		if (in_data) {
			err = sdif_dma_prepare(sc, cmd_data, in_data);
		}

		if (err != 0) {
			req->cmd->error = MMC_ERR_NO_MEMORY;
			goto err_exit;
		}

		wmb();

		ferr = FSdifDMATransfer(&sc->hc, cmd);
		if (FSDIF_SUCCESS != ferr) {
			device_printf(brdev,
			    "Failed to start command-%d DMA transfer, ferr=0x%x\n",
			    req->cmd->opcode, ferr);
			err = (ENXIO);
			req->cmd->error = MMC_ERR_FAILED;
			if (req->cmd->data) {
				device_printf(brdev,
				    "Failed to transfer %d-blocks \n",
				    cmd_data->blkcnt);
			}
			goto err_exit;
		}
	}

	wmb();

	/* Start timeout callout. */
	callout_reset(&sc->timeout_callout, 10 * hz, phytium_sdif_timeout, sc);

err_exit:
	SDIF_UNLOCK(sc);
	return err;
}

static void
sdif_finish_command(struct phytium_sdif_softc *sc)
{
	struct mmc_request *req = sc->req;
	FSdifCmdData *cmd = &(sc->cmd_pkg);
	struct mmc_data *in_data;

	if (req == NULL) {
		dprintf("private command - no pending request\n");
		sc->req_pending = 0U;
		return;
	}

	if (!sc->req_pending) {
		panic("Spurious comand done - no pending state\n");
	}

	if (FSDIF_SUCCESS != FSdifGetCmdResponse(&sc->hc, cmd)) {
		panic("failed to get command response\n");
	}

	if (MMC_RSP_136 & req->cmd->flags) {
		req->cmd->resp[3] = cmd->response[0];
		req->cmd->resp[2] = cmd->response[1];
		req->cmd->resp[1] = cmd->response[2];
		req->cmd->resp[0] = cmd->response[3];
	} else if (MMC_RSP_PRESENT & req->cmd->flags) {
		req->cmd->resp[0] = cmd->response[0];
		req->cmd->resp[1] = 0;
		req->cmd->resp[2] = 0;
		req->cmd->resp[3] = 0;
	}

	if (sc->expect_data) {
		in_data = req->cmd->data;
#ifdef __rtems__
		FSdifData *trans_data = &(sc->dat_pkg);

		if ((MMC_DATA_READ & in_data->flags) &&
		    (in_data->data != trans_data->buf)) {
			memcpy(in_data->data, trans_data->buf, in_data->len);
		}
#endif

#ifdef DEBUG
		if ((MMC_DATA_READ & in_data->flags) && (in_data->data)) {
			FtDumpHexWord(in_data->data,
			    min(MMC_SECTOR_SIZE, in_data->len));
		}
#else
		(void)in_data;
#endif
	}

	sc->req = NULL;
	sc->curcmd = NULL;
	req->cmd->error = MMC_ERR_NONE;
	sc->req_pending = 0U;

	callout_stop(&sc->timeout_callout);
	dprintf("commond-%d finished\n", req->cmd->opcode);

	(*req->done)(req);
}

static void
sdif_card_detected(FSdif *const instance_p, void *args, u32 status,
    u32 dmac_status)
{
	struct phytium_sdif_softc *sc = (struct phytium_sdif_softc *)args;

	sdif_handle_card_present(sc, FSdifCheckCardExists(instance_p));
}

static void
sdif_command_done(FSdif *const instance_p, void *args, u32 status,
    u32 dmac_status)
{
	struct phytium_sdif_softc *sc = (struct phytium_sdif_softc *)args;

	sc->cmd_done = 1U;
	if (!sc->expect_data) {
		dprintf("command done\n");
		sdif_finish_command(sc);
	} else {
		if (sc->dto_rcvd) {
			dprintf("data and command done\n");
			sdif_finish_command(sc);
		}
	}
}

static void
sdif_data_done(FSdif *const instance_p, void *args, u32 status, u32 dmac_status)
{
	struct phytium_sdif_softc *sc = (struct phytium_sdif_softc *)args;
	struct mmc_command *cmd = sc->curcmd;

	sc->dto_rcvd = 1;
	if (sc->hc.config.trans_mode == FSDIF_IDMA_TRANS_MODE) {
		sdif_dma_done(sc, cmd);
	}

	if (sc->cmd_done) {
		dprintf("data done\n");
		sdif_finish_command(sc);
	}
}

static void
sdif_error_occurred(FSdif *const instance_p, void *args, u32 status,
    u32 dmac_status)
{
	struct phytium_sdif_softc *sc = (struct phytium_sdif_softc *)args;
	struct mmc_command *cmd = sc->curcmd;

	if ((sc->expect_data) && (cmd)) {
		if (sc->hc.config.trans_mode == FSDIF_IDMA_TRANS_MODE) {
			sdif_dma_done(sc, cmd);
			sdif_dma_stop(sc);
		}
	}
}

static void
sdif_intr(void *arg)
{
	struct phytium_sdif_softc *sc = arg;
	FSdifInterruptHandler(0, &(sc->hc));

#ifdef __rtems__
	rtems_status_code rs = rtems_event_transient_send(sc->task_id);
	BSD_ASSERT(rs == RTEMS_SUCCESSFUL);
#endif
}

static int
phytium_sdif_attach(device_t dev)
{
	struct phytium_sdif_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

#ifdef DDB
	int unit = device_get_unit(dev);
	if (unit < PHYTIUM_MAX_DDB_SC) {
		sdif_sc_ddb[unit] = sc;
	}
#endif

	sc->bus_width = 0xff;
	sc->card_clock = 0xff;
	sc->power_mode = 0xff;

	SDIF_LOCK_INIT(sc);
	callout_init_mtx(&sc->timeout_callout, &sc->sc_mtx, 0);

	/* Setup register space and interrupt resource */
	if (bus_alloc_resources(dev, sdif_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->hc_cfg.instance_id = device_get_unit(dev);
#ifndef __rtems__
	sc->hc_cfg.base_addr = (uintptr_t)rman_get_virtual(sc->res[0]);
#else
	sc->hc_cfg.base_addr = rman_get_bushandle(sc->res[0]);
	dprintf("SDIF-%d register base: 0x%x\n", sc->hc_cfg.instance_id,
	    sc->hc_cfg.base_addr);
#endif

	if (FSDIF_SUCCESS != FSdifCfgInitialize(&sc->hc, &sc->hc_cfg)) {
		device_printf(dev, "Failed to init sdif by configure\n");
		return (ENXIO);
	}

	if (sc->hc.config.trans_mode == FSDIF_IDMA_TRANS_MODE) {
		if (sdif_dma_setup(sc)) {
			device_printf(dev, "Failed to setup DMA\n");
			return (ENXIO);
		}

		FSdifSetIDMAList(&sc->hc, sc->desc_ring, sc->desc_ring_paddr,
		    IDMAC_DESC_SEGS);
	}

	dprintf("descriptor ring 0x%lx \n", sc->desc_ring_paddr);

	FSdifRegisterEvtHandler(&sc->hc, FSDIF_EVT_CARD_DETECTED,
	    sdif_card_detected, (void *)sc);
	FSdifRegisterEvtHandler(&sc->hc, FSDIF_EVT_ERR_OCCURE,
	    sdif_error_occurred, (void *)sc);
	FSdifRegisterEvtHandler(&sc->hc, FSDIF_EVT_CMD_DONE, sdif_command_done,
	    (void *)sc);
	FSdifRegisterEvtHandler(&sc->hc, FSDIF_EVT_DATA_DONE, sdif_data_done,
	    (void *)sc);

	/* Setup interrupt handler. */
	error = bus_setup_intr(dev, sc->res[1], INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, sdif_intr, sc, &sc->intr_cookie);
	if (error != 0) {
		device_printf(dev, "could not setup interrupt handler.\n");
		return (ENXIO);
	}

	/* Create hotplug task */
	if (!sc->hc_cfg.non_removable) {
		TASK_INIT(&sc->card_task, 0, sdif_card_task, sc);
		TIMEOUT_TASK_INIT(taskqueue_swi_giant, &sc->card_delayed_task,
		    0, sdif_card_task, sc);
	}

	/*
	 * Schedule a card detection as we won't get an interrupt
	 * if the card is inserted when we attach
	 */
	sdif_card_task(sc, 0);
	return (0);
}

static int
phytium_sdif_detach(device_t dev)
{
	struct phytium_sdif_softc *sc;
	int ret;

	sc = device_get_softc(dev);

	ret = device_delete_children(dev);
	if (ret != 0)
		return (ret);

	if (!sc->hc_cfg.non_removable) {
		taskqueue_drain(taskqueue_swi_giant, &sc->card_task);
	}
	taskqueue_drain_timeout(taskqueue_swi_giant, &sc->card_delayed_task);

	if (sc->intr_cookie != NULL) {
		ret = bus_teardown_intr(dev, sc->res[1], sc->intr_cookie);
		if (ret != 0)
			return (ret);
	}

	FSdifDeInitialize(&sc->hc);

	if (sc->hc.config.trans_mode == FSDIF_IDMA_TRANS_MODE) {
		sdif_dma_destory(sc);
	}

	bus_release_resources(dev, sdif_spec, sc->res);

	SDIF_LOCK_DESTROY(sc);
	callout_drain(&sc->timeout_callout);

#ifdef DDB
	int unit = device_get_unit(dev);
	if (unit < PHYTIUM_MAX_DDB_SC) {
		sdif_sc_ddb[unit] = NULL;
	}
#endif

	return (0);
}

static int
phytium_sdif_read_ivar(device_t bus, device_t child, int which,
    uintptr_t *result)
{
	struct phytium_sdif_softc *sc;

	sc = device_get_softc(bus);

	switch (which) {
	default:
		return (EINVAL);
	case MMCBR_IVAR_BUS_MODE:
		*(int *)result = sc->host.ios.bus_mode;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		*(int *)result = sc->host.ios.bus_width;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		*(int *)result = sc->host.ios.chip_select;
		break;
	case MMCBR_IVAR_CLOCK:
		*(int *)result = sc->host.ios.clock;
		break;
	case MMCBR_IVAR_F_MIN:
		*(int *)result = sc->host.f_min;
		break;
	case MMCBR_IVAR_F_MAX:
		*(int *)result = sc->host.f_max;
		break;
	case MMCBR_IVAR_HOST_OCR:
		*(int *)result = sc->host.host_ocr;
		break;
	case MMCBR_IVAR_MODE:
		*(int *)result = sc->host.mode;
		break;
	case MMCBR_IVAR_OCR:
		*(int *)result = sc->host.ocr;
		break;
	case MMCBR_IVAR_POWER_MODE:
		*(int *)result = sc->host.ios.power_mode;
		break;
	case MMCBR_IVAR_VDD:
		*(int *)result = sc->host.ios.vdd;
		break;
	case MMCBR_IVAR_VCCQ:
		*(int *)result = sc->host.ios.vccq;
		break;
	case MMCBR_IVAR_CAPS:
		*(int *)result = sc->host.caps;
		break;
	case MMCBR_IVAR_MAX_DATA:
		if (FSDIF_IDMA_TRANS_MODE == sc->hc.config.trans_mode) {
			*(int *)result = SDIF_MAX_DATA;
		} else {
			*(int *)result = SDIF_MAX_PIO_DATA;
		}
		break;
	case MMCBR_IVAR_TIMING:
		*(int *)result = sc->host.ios.timing;
		break;
	case MMCBR_IVAR_MAX_BUSY_TIMEOUT:
		/*
		 * Currently, hardcodes 1 s for all CMDs.
		 */
		*result = 1000000;
		break;
	}
	return (0);
}

static int
phytium_sdif_write_ivar(device_t bus, device_t child, int which,
    uintptr_t value)
{
	struct phytium_sdif_softc *sc;

	sc = device_get_softc(bus);

	switch (which) {
	default:
		return (EINVAL);
	case MMCBR_IVAR_BUS_MODE:
		sc->host.ios.bus_mode = value;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		sc->host.ios.bus_width = value;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		sc->host.ios.chip_select = value;
		break;
	case MMCBR_IVAR_CLOCK:
		sc->host.ios.clock = value;
		break;
	case MMCBR_IVAR_MODE:
		sc->host.mode = value;
		break;
	case MMCBR_IVAR_OCR:
		sc->host.ocr = value;
		break;
	case MMCBR_IVAR_POWER_MODE:
		sc->host.ios.power_mode = value;
		break;
	case MMCBR_IVAR_VDD:
		sc->host.ios.vdd = value;
		break;
	case MMCBR_IVAR_TIMING:
		sc->host.ios.timing = value;
		break;
	case MMCBR_IVAR_VCCQ:
		sc->host.ios.vccq = value;
		break;
	/* These are read-only */
	case MMCBR_IVAR_CAPS:
	case MMCBR_IVAR_HOST_OCR:
	case MMCBR_IVAR_F_MIN:
	case MMCBR_IVAR_F_MAX:
	case MMCBR_IVAR_MAX_DATA:
		return (EINVAL);
	}
	return (0);
}

static const FSdifTiming *
sdif_get_sd_timing(struct phytium_sdif_softc *sc, uint32_t target_clock)
{
	static const FSdifTiming sd_timing[] = {
		{
		    /* 400khz */
		    .use_hold = 1,
		    .clk_div = 0x7e7dfa,
		    .clk_src = 0x000502,
		    .shift = 0x0,
		    .pad_delay = NULL,
		},
		{
		    /* 25mhz */
		    .use_hold = 1,
		    .clk_div = 0x030204,
		    .clk_src = 0x000302,
		    .shift = 0x0,
		    .pad_delay = NULL,
		},
		{ /* 50mhz */
		    .use_hold = 1,
		    .clk_div = 0x030204,
		    .clk_src = 0x000502,
		    .shift = 0x0,
		    .pad_delay = NULL },
		{
		    /* 100mhz */
		    .use_hold = 0,
		    .clk_div = 0x010002,
		    .clk_src = 0x000202,
		    .shift = 0x0,
		    .pad_delay = NULL,
		},
	};

	if (target_clock <= FSDIF_CLK_SPEED_400KHZ) {
		return &sd_timing[0];
	} else if ((target_clock > FSDIF_CLK_SPEED_400KHZ) &&
	    (target_clock <= FSDIF_CLK_SPEED_25_MHZ)) {
		return &sd_timing[1];
	} else if ((target_clock > FSDIF_CLK_SPEED_25_MHZ) &&
	    (target_clock <= FSDIF_CLK_SPEED_50_MHZ)) {
		return &sd_timing[2];
	} else if ((target_clock > FSDIF_CLK_SPEED_50_MHZ) &&
	    (target_clock <= FSDIF_CLK_SPEED_100_MHZ)) {
		return &sd_timing[3];
	} else {
		return NULL;
	}
}

static const FSdifTiming *
sdif_get_mmc_timing(struct phytium_sdif_softc *sc, uint32_t target_clock)
{
	static const FSdifTiming mmc_timing[] = {
		{
		    /* 400khz */
		    .use_hold = 1,
		    .clk_div = 0x7e7dfa,
		    .clk_src = 0x000502,
		    .shift = 0x0,
		    .pad_delay = NULL,
		},
		{
		    /* 26mhz */
		    .use_hold = 1,
		    .clk_div = 0x030204,
		    .clk_src = 0x000302,
		    .shift = 0x0,
		    .pad_delay = NULL,
		},
		{
		    /* 52mhz */
		    .use_hold = 0,
		    .clk_div = 0x030204,
		    .clk_src = 0x000202,
		    .shift = 0x0,
		    .pad_delay = NULL,
		},
		{
		    /* 66mhz */
		    .use_hold = 0,
		    .clk_div = 0x010002,
		    .clk_src = 0x000202,
		    .shift = 0x0,
		    .pad_delay = NULL,
		},
		{
		    /* 100mhz */
		    .use_hold = 0,
		    .clk_div = 0x010002,
		    .clk_src = 0x000202,
		    .shift = 0x0,
		    .pad_delay = NULL,
		},
	};

	if (target_clock <= FSDIF_CLK_SPEED_400KHZ) {
		return &mmc_timing[0];
	} else if ((target_clock > FSDIF_CLK_SPEED_400KHZ) &&
	    (target_clock <= FSDIF_CLK_SPEED_26_MHZ)) {
		return &mmc_timing[1];
	} else if ((target_clock > FSDIF_CLK_SPEED_26_MHZ) &&
	    (target_clock <= FSDIF_CLK_SPEED_52_MHZ)) {
		return &mmc_timing[2];
	} else if ((target_clock > FSDIF_CLK_SPEED_52_MHZ) &&
	    (target_clock <= FSDIF_CLK_SPEED_66_MHZ)) {
		return &mmc_timing[3];
	} else if ((target_clock > FSDIF_CLK_SPEED_66_MHZ) &&
	    (target_clock <= FSDIF_CLK_SPEED_100_MHZ)) {
		return &mmc_timing[4];
	} else {
		return NULL;
	}
}

static int
phytium_sdif_update_ios(device_t brdev, device_t reqdev)
{
	struct phytium_sdif_softc *sc;
	boolean is_ddr;
	struct mmc_ios *ios;
	int err = 0;

	sc = device_get_softc(brdev);

	SDIF_LOCK(sc);

	ios = &sc->host.ios;

	dprintf("Setting up clk %u bus_width %d, timing: %d\n", ios->clock,
	    ios->bus_width, ios->timing);

	if (sc->bus_width != ios->bus_width) {
		if (ios->bus_width == bus_width_8)
			FSdifSetCardBusWidth(&(sc->hc), 8);
		else if (ios->bus_width == bus_width_4)
			FSdifSetCardBusWidth(&(sc->hc), 4);
		else
			FSdifSetCardBusWidth(&(sc->hc), 1);
		sc->bus_width = ios->bus_width;
	}

	if (sc->power_mode != ios->power_mode) {
		switch (ios->power_mode) {
		case power_off:
			FSdifSetCardPower(&(sc->hc), FALSE);
			break;
		case power_up:
			sc->hc.card_need_init = TRUE;
			FSdifSetCardPower(&(sc->hc), TRUE);
			break;
		case power_on:
			break;
		}
		sc->power_mode = ios->power_mode;
	}

	if (sc->card_clock != ios->clock) {
		if (ios->timing == bus_timing_uhs_ddr50 ||
		    ios->timing == bus_timing_mmc_ddr52 ||
		    ios->timing == bus_timing_mmc_hs400)
			is_ddr = TRUE;
		else
			is_ddr = FALSE;

		if (sc->pre_fix_timing == 0) {
			if (FSDIF_SUCCESS !=
			    FSdifSetClkFreqByCalc(&(sc->hc), is_ddr,
				ios->clock)) {
				device_printf(brdev,
				    "could not set clock to %dHz.\n",
				    ios->clock);
				err = (EIO);
				goto err_exit;
			}
		} else {
			/* use fixed timing when data transfer failed with CRC
			 * error somehow */
			if (sc->hc.config.non_removable) {
				sc->cur_timing = sdif_get_mmc_timing(sc,
				    ios->clock);
			} else {
				sc->cur_timing = sdif_get_sd_timing(sc,
				    ios->clock);
			}

			if (sc->cur_timing == NULL) {
				device_printf(brdev,
				    "could not find timing for %dHz.\n",
				    ios->clock);
				err = (EIO);
				goto err_exit;
			}

			sc->use_hold = sc->cur_timing->use_hold;

			if (FSDIF_SUCCESS !=
			    FSdifSetClkFreqByDict(&(sc->hc), is_ddr,
				sc->cur_timing, ios->clock)) {
				device_printf(brdev,
				    "could not set clock to %dHz.\n",
				    ios->clock);
				err = (EIO);
				goto err_exit;
			}
		}

		FSdifSetCardDDRMode(&(sc->hc), is_ddr);
		sc->card_clock = ios->clock;
		dprintf("Set sdif clock to %d ddr %s\n", ios->clock,
		    is_ddr ? "on" : "off");
	}

	if ((!sc->hc.config.non_removable) && (sc->voltage != ios->vdd)) {
		switch (ios->vdd) {
		case vdd_330:
		case vdd_340:
			FSdifSetVoltage1_8V(sc->hc.config.base_addr, FALSE);
			break;
		case vdd_180:
			FSdifSetVoltage1_8V(sc->hc.config.base_addr, TRUE);
			break;
		default:
			device_printf(brdev, "unhandled vdd-%d\n", ios->vdd);
			break;
		}
		sc->voltage = ios->vdd;
	}

#ifndef __rtems__
	if (sc->mmc_set_power) {
		sc->mmc_set_power(&sc->mmc_helper, ios->power_mode);
	}
#endif

err_exit:
	SDIF_UNLOCK(sc);
	return (err);
}

#ifndef MMCCAM
static int
phytium_sdif_get_ro(device_t brdev, device_t reqdev)
{
	dprintf("%s\n", __func__);
	return (0);
}

static int
phytium_sdif_acquire_host(device_t brdev, device_t reqdev)
{
	struct phytium_sdif_softc *sc;

	sc = device_get_softc(brdev);

	SDIF_LOCK(sc);
	while (sc->bus_busy)
		msleep(sc, &sc->sc_mtx, PZERO, "sdifah", hz / 5);
	sc->bus_busy++;
	SDIF_UNLOCK(sc);
	return (0);
}

static int
phytium_sdif_release_host(device_t brdev, device_t reqdev)
{
	struct phytium_sdif_softc *sc;

	sc = device_get_softc(brdev);

	SDIF_LOCK(sc);
	sc->bus_busy--;
	wakeup(sc);
	SDIF_UNLOCK(sc);
	return (0);
}
#endif /* !MMCCAM */

#ifdef MMCCAM
/* Note: this function likely belongs to the specific driver impl */
static int
phytium_sdif_switch_vccq(device_t dev, device_t child)
{
	device_printf(dev,
	    "This is a default impl of switch_vccq() that always fails\n");
	return EINVAL;
}

static int
phytium_sdif_get_tran_settings(device_t dev, struct ccb_trans_settings_mmc *cts)
{
	struct phytium_sdif_softc *sc;

	sc = device_get_softc(dev);

	cts->host_ocr = sc->host.host_ocr;
	cts->host_f_min = sc->host.f_min;
	cts->host_f_max = sc->host.f_max;
	cts->host_caps = sc->host.caps;
	if (FSDIF_IDMA_TRANS_MODE == sc->hc.config.trans_mode) {
		cts->host_max_data = SDIF_MAX_DATA;
	} else {
		cts->host_max_data = SDIF_MAX_PIO_DATA;
	}
	memcpy(&cts->ios, &sc->host.ios, sizeof(struct mmc_ios));

	return (0);
}

static int
phytium_sdif_set_tran_settings(device_t dev, struct ccb_trans_settings_mmc *cts)
{
	struct phytium_sdif_softc *sc;
	struct mmc_ios *ios;
	struct mmc_ios *new_ios;
	int res;

	sc = device_get_softc(dev);
	ios = &sc->host.ios;

	new_ios = &cts->ios;

	/* Update only requested fields */
	if (cts->ios_valid & MMC_CLK) {
		ios->clock = new_ios->clock;
		if (bootverbose)
			device_printf(sc->dev, "Clock => %d\n", ios->clock);
	}
	if (cts->ios_valid & MMC_VDD) {
		ios->vdd = new_ios->vdd;
		if (bootverbose)
			device_printf(sc->dev, "VDD => %d\n", ios->vdd);
	}
	if (cts->ios_valid & MMC_CS) {
		ios->chip_select = new_ios->chip_select;
		if (bootverbose)
			device_printf(sc->dev, "CS => %d\n", ios->chip_select);
	}
	if (cts->ios_valid & MMC_BW) {
		ios->bus_width = new_ios->bus_width;
		if (bootverbose)
			device_printf(sc->dev, "Bus width => %d\n",
			    ios->bus_width);
	}
	if (cts->ios_valid & MMC_PM) {
		ios->power_mode = new_ios->power_mode;
		if (bootverbose)
			device_printf(sc->dev, "Power mode => %d\n",
			    ios->power_mode);
	}
	if (cts->ios_valid & MMC_BT) {
		ios->timing = new_ios->timing;
		if (bootverbose)
			device_printf(sc->dev, "Timing => %d\n", ios->timing);
	}
	if (cts->ios_valid & MMC_BM) {
		ios->bus_mode = new_ios->bus_mode;
		if (bootverbose)
			device_printf(sc->dev, "Bus mode => %d\n",
			    ios->bus_mode);
	}
	if (cts->ios_valid & MMC_VCCQ) {
		ios->vccq = new_ios->vccq;
		if (bootverbose)
			device_printf(sc->dev, "VCCQ => %d\n", ios->vccq);
		res = phytium_sdif_switch_vccq(sc->dev, NULL);
		device_printf(sc->dev, "VCCQ switch result: %d\n", res);
	}

	return (phytium_sdif_update_ios(sc->dev, NULL));
}

static int
phytium_sdif_cam_request(device_t dev, union ccb *ccb)
{
	struct phytium_sdif_softc *sc;
	struct ccb_mmcio *mmcio;

	sc = device_get_softc(dev);
	mmcio = &ccb->mmcio;

	SDIF_LOCK(sc);

#ifdef DEBUG
	if (__predict_false(bootverbose)) {
		device_printf(sc->dev,
		    "CMD%u arg %#x flags %#x dlen %u dflags %#x\n",
		    mmcio->cmd.opcode, mmcio->cmd.arg, mmcio->cmd.flags,
		    mmcio->cmd.data != NULL ?
			(unsigned int)mmcio->cmd.data->len :
			0,
		    mmcio->cmd.data != NULL ? mmcio->cmd.data->flags : 0);
	}
#endif
	if (mmcio->cmd.data != NULL) {
		if (mmcio->cmd.data->len == 0 || mmcio->cmd.data->flags == 0)
			panic(
			    "data->len = %d, data->flags = %d -- something is b0rked",
			    (int)mmcio->cmd.data->len, mmcio->cmd.data->flags);
	}
	if (sc->ccb != NULL) {
		device_printf(sc->dev,
		    "Controller still has an active command\n");
		return (EBUSY);
	}
	sc->ccb = ccb;
	SDIF_UNLOCK(sc);
	phytium_sdif_request(sc->dev, NULL, NULL);

	return (0);
}

static void
phytium_sdif_cam_poll(device_t dev)
{
	struct phytium_sdif_softc *sc;

	sc = device_get_softc(dev);
	sdif_intr(sc);
}
#endif /* MMCCAM */

static device_method_t sdif_methods[] = { 
	DEVMETHOD(device_attach, phytium_sdif_attach),
	DEVMETHOD(device_detach, phytium_sdif_detach),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar, phytium_sdif_read_ivar),
	DEVMETHOD(bus_write_ivar, phytium_sdif_write_ivar),

#ifndef MMCCAM
	/* mmcbr_if */
	DEVMETHOD(mmcbr_update_ios, phytium_sdif_update_ios),
	DEVMETHOD(mmcbr_request, phytium_sdif_request),
	DEVMETHOD(mmcbr_get_ro, phytium_sdif_get_ro),
	DEVMETHOD(mmcbr_acquire_host, phytium_sdif_acquire_host),
	DEVMETHOD(mmcbr_release_host, phytium_sdif_release_host),
#endif

#ifdef MMCCAM
	/* MMCCAM interface */
	DEVMETHOD(mmc_sim_get_tran_settings, phytium_sdif_get_tran_settings),
	DEVMETHOD(mmc_sim_set_tran_settings, phytium_sdif_set_tran_settings),
	DEVMETHOD(mmc_sim_cam_request, phytium_sdif_cam_request),
	DEVMETHOD(mmc_sim_cam_poll, phytium_sdif_cam_poll),

	DEVMETHOD(bus_add_child, bus_generic_add_child),
#endif

	DEVMETHOD_END
};

/* Generic sdif driver */
DEFINE_CLASS_0(sdif, sdif_driver, sdif_methods,
    sizeof(struct phytium_sdif_softc));