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

#include "opt_sdif_bm.h"
#ifndef __rtems__
#include "opt_acpi.h"
#include "opt_platform.h"
#endif

#ifdef __rtems__
#include <machine/rtems-bsd-kernel-space.h>

#include <rtems/bsd/local/opt_platform.h>
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
#include <sys/queue.h>
#include <sys/rman.h>
#include <sys/taskqueue.h>
#include <sys/time.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcbrvar.h>

#ifdef FDT
#ifdef __rtems__
#include <bsp/fdt.h>
#include <libfdt.h>
#endif

#ifndef __rtems__
#include "opt_mmccam.h"
#endif

#include <dev/fdt/fdt_common.h>
#ifndef __rtems__
#include <dev/mmc/mmc_fdt_helpers.h>
#endif
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#ifndef __rtems__
#include "mmcbr_if.h"
#else
#include <rtems/bsd/local/mmcbr_if.h>
#endif
#include "phytium_sdif_var.h"

static struct ofw_compat_data compat_data[] = {
	{ "phytium,mci", 1 },
	{ NULL, 0 },
};

static int
sdif_parse_fdt(struct phytium_sdif_softc *sc)
{
	pcell_t dts_value[3];
	phandle_t node;
	int len;

	node = ofw_bus_get_node(sc->dev);

	/* Set some defaults for freq and supported mode */
	sc->host.f_min = SD_MMC_CARD_ID_FREQUENCY;
	sc->host.host_ocr = MMC_OCR_320_330 | MMC_OCR_330_340;
	sc->host.caps = MMC_CAP_HSPEED | MMC_CAP_SIGNALING_330 |
	    MMC_CAP_WAIT_WHILE_BUSY | MMC_CAP_4_BIT_DATA;
	if (device_has_property(sc->dev, "non-removable")) {
		sc->hc_cfg.non_removable = 1;
		dprintf("card non-removable\n");
		sc->host.host_ocr |= MMC_OCR_LOW_VOLTAGE;
		sc->host.caps |= MMC_CAP_SIGNALING_180 | MMC_CAP_SIGNALING_120 |
		    MMC_CAP_8_BIT_DATA;

		if (device_has_property(sc->dev, "hs200")) {
			sc->host.caps |= MMC_CAP_MMC_HS200;
		}

		if (device_has_property(sc->dev, "hs400")) {
			sc->host.caps |= MMC_CAP_MMC_HS400;
		}
	} else {
		sc->hc_cfg.non_removable = 0;
		dprintf("card removable\n");
	}

	if ((len = OF_getproplen(node, "bus-frequency")) > 0) {
		OF_getencprop(node, "bus-frequency", dts_value, len);
		sc->host.f_max = dts_value[0];
	} else {
		sc->host.f_max = SD_SDR50_MAX;
	}

#ifndef __rtems__
	mmc_fdt_parse(sc->dev, node, &sc->mmc_helper, &sc->host);
#endif

	if (device_has_property(sc->dev, "use-hold")) {
		sc->use_hold = 1;
	} else {
		sc->use_hold = 0;
	}

	if (device_has_property(sc->dev, "pre-fix-timing")) {
		sc->pre_fix_timing = 1;
	} else {
		sc->pre_fix_timing = 0;
	}

	if (device_has_property(sc->dev, "non-dma")) {
		sc->hc_cfg.trans_mode = FSDIF_PIO_TRANS_MODE;
	} else {
		sc->hc_cfg.trans_mode = FSDIF_IDMA_TRANS_MODE;
	}

	if (device_has_property(sc->dev, "auto-stop")) {
		sc->auto_stop = TRUE;
	} else {
		sc->auto_stop = FALSE;
	}

	/* clock-frequency */
	if ((len = OF_getproplen(node, "clock-frequency")) > 0) {
		OF_getencprop(node, "clock-frequency", dts_value, len);
		sc->hc_cfg.src_clk_rate = dts_value[0];
	}

	if (sc->hc_cfg.src_clk_rate == 0) {
		device_printf(sc->dev, "No bus speed provided\n");
		goto fail;
	}

	return (0);

fail:
	return (ENXIO);
}

static int
phytium_sdif_fdt_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data) {
		return (ENXIO);
	}

	dprintf("Phytium SDIF Host (FDT) probing\r\n");
	device_set_desc(dev, "Phytium SDIF Host (FDT)");

	struct phytium_sdif_softc *sc = device_get_softc(dev);

#ifndef __rtems__
	sc->mmc_set_power = mmc_fdt_set_power;
#endif
	sc->dev = dev;

	int error = sdif_parse_fdt(sc);
	if (error != 0) {
		device_printf(dev, "Can't get FDT property.\n");
		return (ENXIO);
	}

	dprintf("Phytium SDIF Host (FDT) probed\r\n");
	return (BUS_PROBE_VENDOR);
}

static device_method_t phytium_sdif_fdt_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, phytium_sdif_fdt_probe),

	DEVMETHOD_END
};

DEFINE_CLASS_1(phytium_sdif_fdt, phytium_sdif_fdt_driver,
    phytium_sdif_fdt_methods, sizeof(struct phytium_sdif_softc), sdif_driver);

DRIVER_MODULE(phytium_sdif_fdt, simplebus, phytium_sdif_fdt_driver, 0, 0);
DRIVER_MODULE(phytium_sdif_fdt, ofwbus, phytium_sdif_fdt_driver, 0, 0);
#ifndef MMCCAM
MMC_DECLARE_BRIDGE(phytium_sdif_fdt);
#endif
#endif