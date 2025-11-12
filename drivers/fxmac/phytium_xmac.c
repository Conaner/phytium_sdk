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
 * 1.0   huangjin   2025/05/06    init commit
 */

/* 裸机头文件 */
#ifdef __rtems__
#include <phytium/faarch.h>
#include <phytium/fxmac.h>
#include <phytium/fxmac_hw.h>
#include <phytium/fxmac_phy.h>
#include <phytium/fxmac_bd.h>
#include <phytium/eth_ieee_reg.h>
#else
#include "faarch.h"
#include "fdrivers_port.h"
#include "fxmac.h"
#include "fxmac_hw.h"
#include "phytium/fxmac_phy.h"
#include "phytium/fxmac_bd.h"
#include "phytium/eth_ieee_reg.h"
#endif

/* 取消重复定义的宏 */
#undef max
#undef min
#undef ALIGN
#undef rounddown
#undef roundup
#undef UL

/* 包含freebsd命名空间 */
#ifdef __rtems__
#include <machine/rtems-bsd-kernel-space.h>
#include <rtems/bsd/local/opt_platform.h>
#endif

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_mib.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif


#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#if BUS_SPACE_MAXADDR > BUS_SPACE_MAXADDR_32BIT
#define XMAC64
#endif

#include <rtems/bsd/local/miibus_if.h>

#ifdef __rtems__
#pragma GCC diagnostic ignored "-Wpointer-sign"
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
#include <rtems/bsd/bsd.h>
#endif /* __rtems__ */

#define IF_XMAC_NAME "xmac"
#define XMAC_HC_DEVSTR "Phytium XMAC Host"

/* 描述符相关宏定义 */
#define XMAC_NUM_RX_DESCS	512	/* size of receive descriptor ring */
#define XMAC_NUM_TX_DESCS	512	/* size of transmit descriptor ring */
/* Default for sysctl rxbufs.  Must be < XMAC_NUM_RX_DESCS of course. */
#define DEFAULT_NUM_RX_BUFS	256	/* number of receive bufs to queue. */
#define TX_MAX_DMA_SEGS		8	/* maximum segs in a tx mbuf dma */
#define FXMAC_DMA_CFG_RX_BUF_SIZE(sz)	((((sz) + 63) / 64) << 16)

#define XMAC_CKSUM_ASSIST	(CSUM_IP | CSUM_TCP | CSUM_UDP | CSUM_TCP_IPV6 | CSUM_UDP_IPV6)

#define HWQUIRK_NONE		0
#define HWQUIRK_NEEDNULLQS	1
#define HWQUIRK_RXHANGWAR	2
#define HWQUIRK_VERSION2    4

static struct ofw_compat_data compat_data[] = {
	{ "phytium,xmac", 		HWQUIRK_NONE },
	{ "phytium,xmac,2",		HWQUIRK_VERSION2 },
	{ NULL, HWQUIRK_NONE }
};

struct phytium_xmac_rx_desc {
    uint32_t    addr;
    uint32_t    ctl;
    uint32_t    addrhi;
    uint32_t    unused;
};

struct phytium_xmac_tx_desc {
    uint32_t    addr;
    uint32_t    ctl;
    uint32_t    addrhi;
    uint32_t    unused;
};

/* FXmac OS适配层结构体 */
typedef struct
{
	/* 控制器实例 */
  	FXmac instance;

	/* 功能标志位 */
  	uint32_t feature;

	/* MAC地址 */
  	uint8_t hwaddr[ETHER_ADDR_LEN];
} OsFXmacPort;

struct phytium_xmac_softc {
	if_t				ifp;
	struct mtx			sc_mtx;
	device_t 			dev;
	device_t			miibus;
	u_int				version;
	u_int				mii_media_active;	/* last active media */
	int					if_old_flags;
	struct resource		*mem_res;
	struct resource		*irq_res;
	void				*intrhand;
	struct callout		tick_ch;
	uint32_t			net_ctl_shadow;
	uint32_t			net_cfg_shadow;

	int					phy_contype;

	/* 描述符环的DMA约束标签 */
	bus_dma_tag_t		desc_dma_tag;
	/* 数据缓冲区的DMA约束标签 */
	bus_dma_tag_t		mbuf_dma_tag;
	
	/* receive descriptor ring */
	struct phytium_xmac_rx_desc		*rxring;
	bus_addr_t				rxring_physaddr;
	struct mbuf				*rxring_m[XMAC_NUM_RX_DESCS];
	int						rxring_hd_ptr;	/* where to put rcv bufs */
	int						rxring_tl_ptr;	/* where to get receives */
	int						rxring_queued;	/* how many rcv bufs queued */
	bus_dmamap_t			rxring_dma_map;
	int						rxbufs;			/* tunable number rcv bufs */
	int						rxhangwar;		/* rx hang work-around */
	u_int					rxoverruns;		/* rx overruns */
	u_int					rxnobufs;		/* rx buf ring empty events */
	u_int					rxdmamapfails;	/* rx dmamap failures */
	uint32_t				rx_frames_prev;	

	/* transmit descriptor ring */
	struct phytium_xmac_tx_desc		*txring;
	bus_addr_t				txring_physaddr;
	struct mbuf				*txring_m[XMAC_NUM_TX_DESCS];
	int						txring_hd_ptr;	/* where to put next xmits */
	int						txring_tl_ptr;	/* next xmit mbuf to free */
	int						txring_queued;	/* num xmits segs queued */
	bus_dmamap_t			txring_dma_map;
	u_int					txfull;			/* tx ring full events */
	u_int					txdefrags;		/* tx calls to m_defrag() */
	u_int					txdefragfails;	/* tx m_defrag() failures */
	u_int					txdmamapfails;	/* tx dmamap failures */

	OsFXmacPort fxmac_port;
};

#define XMAC_LOCK(sc)			mtx_lock(&(sc)->sc_mtx)
#define XMAC_UNLOCK(sc)			mtx_unlock(&(sc)->sc_mtx)
#define XMAC_LOCK_INIT(sc)		mtx_init(&(sc)->sc_mtx, device_get_nameunit((sc)->dev), MTX_NETWORK_LOCK, MTX_DEF)
#define XMAC_LOCK_DESTROY(sc)	mtx_destroy(&(sc)->sc_mtx)
#define XMAC_ASSERT_LOCKED(sc)	mtx_assert(&(sc)->sc_mtx, MA_OWNED)

static devclass_t xmac_devclass;

static int phytium_xmac_probe(device_t dev);
static int phytium_xmac_attach(device_t dev);
static int phytium_xmac_detach(device_t dev);
static void phytium_xmac_tick(void *);
static void phytium_xmac_intr(void *);
static void phytium_xmac_mediachange(struct phytium_xmac_softc *, struct mii_data *);

static void
phytium_xmac_get_mac(struct phytium_xmac_softc *sc, u_char eaddr[])
{
	int i;

	/* See if boot loader gave us a MAC address already. */
	for (i = 0; i < 4; i++) {
		uint32_t low = FXMAC_READREG32(sc->fxmac_port.instance.config.base_address, (FXMAC_GEM_SA1B + i * 8));
		uint32_t high = FXMAC_READREG32(sc->fxmac_port.instance.config.base_address, (FXMAC_GEM_SA1T + i * 8)) & 0xffff;
		if (low != 0 || high != 0) {
			eaddr[0] = low & 0xff;
			eaddr[1] = (low >> 8) & 0xff;
			eaddr[2] = (low >> 16) & 0xff;
			eaddr[3] = (low >> 24) & 0xff;
			eaddr[4] = high & 0xff;
			eaddr[5] = (high >> 8) & 0xff;
			break;
		}
	}

	/* No MAC from boot loader?  Assign a random one. */
	if (i == 4) {
		rtems_bsd_get_mac_address(device_get_name(sc->dev),
		    device_get_unit(sc->dev), eaddr);

		device_printf(sc->dev, "no mac address found, assigning "
		    "random: %02x:%02x:%02x:%02x:%02x:%02x\n", eaddr[0],
		    eaddr[1], eaddr[2], eaddr[3], eaddr[4], eaddr[5]);
	}

	/* Move address to first slot and zero out the rest. */
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_SA1B,
	 (eaddr[3] << 24) | (eaddr[2] << 16) | (eaddr[1] << 8) | eaddr[0]);
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_SA1T,
	 (eaddr[5] << 8) | eaddr[4]);

	for (i = 1; i < 4; i++) {
		FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, (FXMAC_GEM_SA1B + i * 8), 0);
		FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, (FXMAC_GEM_SA1T + i * 8), 0);
	}
}

/*
 * phytium_xmac_mac_hash():  map 48-bit address to a 6-bit hash. The 6-bit hash
 * corresponds to a bit in a 64-bit hash register.  Setting that bit in the
 * hash register enables reception of all frames with a destination address
 * that hashes to that 6-bit value.
 */
static int
phytium_xmac_mac_hash(u_char eaddr[])
{
	int hash;
	int i, j;

	hash = 0;
	for (i = 0; i < 6; i++)
		for (j = i; j < 48; j += 6)
			if ((eaddr[j >> 3] & (1 << (j & 7))) != 0)
				hash ^= (1 << i);

	return hash;
}

static u_int
phytium_xmac_hash_maddr(void *arg, struct sockaddr_dl *sdl, u_int cnt)
{
	uint32_t *hashes = arg;
	int index;

	index = phytium_xmac_mac_hash(LLADDR(sdl));
	if (index > 31)
		hashes[0] |= (1U << (index - 32));
	else
		hashes[1] |= (1U << index);

	return (1);
}

/* 在 rx flags 或组播地址发生任何更改后，需重新配置哈希寄存器及网络配置寄存器位 */
static void
phytium_xmac_rx_filter(struct phytium_xmac_softc *sc)
{
	if_t ifp = sc->ifp;
	uint32_t hashes[2] = { 0, 0 };

	sc->net_cfg_shadow &= ~(FXMAC_NWCFG_MCASTHASHEN_MASK |
	    FXMAC_NWCFG_BCASTDI_MASK | FXMAC_NWCFG_COPYALLEN_MASK);

	if ((if_getflags(ifp) & IFF_PROMISC) != 0)
		sc->net_cfg_shadow |= FXMAC_NWCFG_COPYALLEN_MASK;
	else {
		if ((if_getflags(ifp) & IFF_BROADCAST) == 0)
			sc->net_cfg_shadow |= FXMAC_NWCFG_BCASTDI_MASK;
		if ((if_getflags(ifp) & IFF_ALLMULTI) != 0) {
			hashes[0] = 0xffffffff;
			hashes[1] = 0xffffffff;
		} else
			if_foreach_llmaddr(ifp, phytium_xmac_hash_maddr, hashes);

		if (hashes[0] != 0 || hashes[1] != 0)
			sc->net_cfg_shadow |= FXMAC_NWCFG_MCASTHASHEN_MASK;
	}

	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_HASHH_OFFSET, hashes[0]);
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_HASHL_OFFSET, hashes[1]);
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWCFG_OFFSET, sc->net_cfg_shadow);
}

/* For bus_dmamap_load() callback. */
static void
phytium_xmac_getaddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{

	if (nsegs != 1 || error != 0)
		return;
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

/* 创建描述符环 */
static int
phytium_xmac_setup_descs(struct phytium_xmac_softc *sc)
{
	int i, err;

	/* 计算描述符环内存需求​ */
	int desc_rings_size = XMAC_NUM_RX_DESCS * sizeof(struct phytium_xmac_rx_desc) +
	    XMAC_NUM_TX_DESCS * sizeof(struct phytium_xmac_tx_desc);
	
	sc->txring = NULL;
	sc->rxring = NULL;

	/* 为desc设置总线DMA标签(desc_dma_tag) */
	err = bus_dma_tag_create(bus_get_dma_tag(sc->dev), 1,
#ifdef XMAC64
	    1ULL << 32,	/* Do not cross a 4G boundary. */
#else
	    0,
#endif
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    desc_rings_size, 1, desc_rings_size, 0,
	    busdma_lock_mutex, &sc->sc_mtx, &sc->desc_dma_tag);
	if (err)
		return (err);
		
	/* 为mbuf设置总线DMA标签(mbuf_dma_tag) */
	err = bus_dma_tag_create(bus_get_dma_tag(sc->dev), 1, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES,
	    TX_MAX_DMA_SEGS, MCLBYTES, 0, busdma_lock_mutex, &sc->sc_mtx,
	    &sc->mbuf_dma_tag);
	if (err)
		return (err);	
		
	/* 一次性分配DMA内存空间（包含发送、接收及空描述符队列），因为硬件仅提供一个寄存器来存储接收(rx)和发送(tx)描述符队列硬件地址的高32位 */
	err = bus_dmamem_alloc(sc->desc_dma_tag, (void **)&sc->rxring,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT | BUS_DMA_ZERO,
	    &sc->rxring_dma_map);
	if (err)
		return (err);
		
	/* 建立虚拟地址到物理地址的映射 */
	err = bus_dmamap_load(sc->desc_dma_tag, sc->rxring_dma_map,
	    (void *)sc->rxring, desc_rings_size,
	    phytium_xmac_getaddr, &sc->rxring_physaddr, BUS_DMA_NOWAIT);
	if (err)
		return (err);		

	/* 初始化 RX 描述符环 */
	for (i = 0; i < XMAC_NUM_RX_DESCS; i++) {
		sc->rxring[i].addr = FXMAC_RXBUF_NEW_MASK;
		sc->rxring[i].ctl = 0;
		sc->rxring_m[i] = NULL;
	}
	sc->rxring[XMAC_NUM_RX_DESCS - 1].addr |= FXMAC_RXBUF_WRAP_MASK;
	sc->rxring_hd_ptr = 0;
	sc->rxring_tl_ptr = 0;
	sc->rxring_queued = 0;

	/* 地址偏移到TX */
	sc->txring = (struct phytium_xmac_tx_desc *)(sc->rxring + XMAC_NUM_RX_DESCS);
	sc->txring_physaddr = sc->rxring_physaddr + XMAC_NUM_RX_DESCS * sizeof(struct phytium_xmac_rx_desc);

	/* 初始化 TX 描述符 */
	for (i = 0; i < XMAC_NUM_TX_DESCS; i++) {
		sc->txring[i].addr = 0;
		sc->txring[i].ctl = FXMAC_TXBUF_USED_MASK;
		sc->txring_m[i] = NULL;
	}
	sc->txring[XMAC_NUM_TX_DESCS - 1].ctl |= FXMAC_TXBUF_WRAP_MASK;
	sc->txring_hd_ptr = 0;
	sc->txring_tl_ptr = 0;
	sc->txring_queued = 0;

	if (sc->version == 2) {
		FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address,
			FXMAC_TAIL_QUEUE(0),
			(1U << 31) | 0U);
	}

	/* 多队列设置 */
	// FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_RXQ1BASE_OFFSET, sc->rxring_physaddr);
	// FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_TXQ1BASE_OFFSET, sc->txring_physaddr);

	return (0);
}

/* 使用mbuf填充接收描述符环 */
static void
phytium_xmac_fill_rqueue(struct phytium_xmac_softc *sc)
{
	struct mbuf *m = NULL;

	bus_dma_segment_t segs[1];

	XMAC_ASSERT_LOCKED(sc);

	while (sc->rxring_queued < sc->rxbufs) {
		/* 分配一个mbuf */
		m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		if (m == NULL)
			break;

		m->m_len = MCLBYTES;
		m->m_pkthdr.len = MCLBYTES;
		m->m_pkthdr.rcvif = sc->ifp;

		/* 插入物理地址 */
		sc->rxring_m[sc->rxring_hd_ptr] = m;

		/* 同步缓存与接收缓冲区数据 */
		rtems_cache_invalidate_multiple_data_lines(m->m_data, m->m_len);
		segs[0].ds_addr = mtod(m, bus_addr_t);

		/* 写入接收描述符 递增头指针 */
		sc->rxring[sc->rxring_hd_ptr].ctl = 0;

#if defined(__LP64__)
		sc->rxring[sc->rxring_hd_ptr].addrhi = segs[0].ds_addr >> 32;
#elif defined(__ILP32__)
		sc->rxring[sc->rxring_hd_ptr].addrhi = 0;
#endif

		if (sc->rxring_hd_ptr == XMAC_NUM_RX_DESCS - 1) {
			sc->rxring[sc->rxring_hd_ptr].addr = segs[0].ds_addr | (1 << 1);
			sc->rxring_hd_ptr = 0;
		} else
			sc->rxring[sc->rxring_hd_ptr++].addr = segs[0].ds_addr;

		sc->rxring_queued++;
	}
}

/* 从接收描述符环中提取已接收的数据包 */
static void
phytium_xmac_recv(struct phytium_xmac_softc *sc)
{
	if_t ifp = sc->ifp;
	struct mbuf *m, *m_hd, **m_tl;
	uint32_t ctl;

	XMAC_ASSERT_LOCKED(sc);

	/* 初始化 mbuf */
	m_hd = NULL;
	m_tl = &m_hd;

	/* 提取所有OWN位被置位的数据包 */
	while (sc->rxring_queued > 0 && (sc->rxring[sc->rxring_tl_ptr].addr & FXMAC_RXBUF_NEW_MASK) != 0) {
		ctl = sc->rxring[sc->rxring_tl_ptr].ctl;

		/* 提取已填充数据的mbuf */
		m = sc->rxring_m[sc->rxring_tl_ptr];
		sc->rxring_m[sc->rxring_tl_ptr] = NULL;

		/* 同步接收缓冲区的缓存一致性 */
		rtems_cache_invalidate_multiple_data_lines(m->m_data, m->m_len);

		/* 递增尾指针 */
		if (++sc->rxring_tl_ptr == XMAC_NUM_RX_DESCS)
			sc->rxring_tl_ptr = 0;
		sc->rxring_queued--;

		/* 检查 帧校验序列(FCS) 并确认整个数据包完整存储在单个mbuf簇内（mbuf簇远大于最大以太网帧） */
		if ((ctl & FXMAC_RXBUF_FCS_STATUS_MASK) != 0 ||
		    (ctl & (FXMAC_RXBUF_SOF_MASK | FXMAC_RXBUF_EOF_MASK)) !=
		    (FXMAC_RXBUF_SOF_MASK | FXMAC_RXBUF_EOF_MASK)) {
			/* 丢弃数据包 */
			m_free(m);
			if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
			continue;
		}

		/* 准备将数据包移交至上层协议栈 */
		m->m_data += ETHER_ALIGN;
		m->m_len = (ctl & FXMAC_RXBUF_LEN_MASK);
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len;

		/* 是否启用硬件校验和检查？需检查接收描述符中的状态位 */
		if ((if_getcapenable(ifp) & IFCAP_RXCSUM) != 0) {
			/* TCP or UDP checks out, IP checks out too. */
			if ((ctl & FXMAC_RXBUF_IDMATCH_MASK) ==
			    FXMAC_RXBUF_ID3MATCH_MASK ||
			    (ctl & FXMAC_RXBUF_IDMATCH_MASK) ==
			    FXMAC_RXBUF_IDMATCH_MASK) {
				m->m_pkthdr.csum_flags |=
				    CSUM_IP_CHECKED | CSUM_IP_VALID |
				    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
				m->m_pkthdr.csum_data = 0xffff;
			} else if ((ctl & FXMAC_RXBUF_IDMATCH_MASK) ==
			    FXMAC_RXBUF_ID2MATCH_MASK) {
				/* Only IP checks out. */
				m->m_pkthdr.csum_flags |=
				    CSUM_IP_CHECKED | CSUM_IP_VALID;
				m->m_pkthdr.csum_data = 0xffff;
			}
		}

		/* 将数据包加入下层发送队列 */
		/* 将 m 加入链表尾部 */
		*m_tl = m;
		/* 更新m_tl 为下一次插入做准备 */
		m_tl = &m->m_next;
	}

	/* 补充接收缓冲区 */
	phytium_xmac_fill_rqueue(sc);

	/* 解锁并上传数据包至协议栈 */
	XMAC_UNLOCK(sc);
	while (m_hd != NULL) {
		m = m_hd;
		m_hd = m_hd->m_next;
		m->m_next = NULL;
		if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);
		/* 交给协议栈 */
		if_input(ifp, m);
	}
	XMAC_LOCK(sc);
}

/* 找到已完成的发送数据包并释放其mbuf内存缓冲区 */
static void
phytium_xmac_clean_tx(struct phytium_xmac_softc *sc)
{
	struct mbuf *m;
	uint32_t ctl;

	XMAC_ASSERT_LOCKED(sc);

	/* free up finished transmits. */
	while (sc->txring_queued > 0 &&
	    ((ctl = sc->txring[sc->txring_tl_ptr].ctl) &
	    FXMAC_TXBUF_USED_MASK) != 0) {
		/* Free up the mbuf. */
		m = sc->txring_m[sc->txring_tl_ptr];
		sc->txring_m[sc->txring_tl_ptr] = NULL;
		m_freem(m);

		/* Check the status. */
		if ((ctl & FXMAC_TXBUF_EXH_MASK) != 0) {
			/* Serious bus error. log to console. */
#ifdef XMAC64
			device_printf(sc->dev,
			    "phytium_xmac_clean_tx: AHB error, addr=0x%x%08x\n",
			    sc->txring[sc->txring_tl_ptr].addrhi,
			    sc->txring[sc->txring_tl_ptr].addr);
#else
			device_printf(sc->dev,
			    "phytium_xmac_clean_tx: AHB error, addr=0x%x\n",
			    sc->txring[sc->txring_tl_ptr].addr);
#endif
		} else if ((ctl & (FXMAC_TXBUF_RETRY_MASK |
		    FXMAC_TXBUF_TCP_MASK)) != 0) {
			if_inc_counter(sc->ifp, IFCOUNTER_OERRORS, 1);
		} else
			if_inc_counter(sc->ifp, IFCOUNTER_OPACKETS, 1);

		/* 如果数据包跨越了多个发送描述符，则跳过中间描述符直到找到帧结束位置，确保只处理帧起始描述符 */
		while ((ctl & FXMAC_TXBUF_LAST_MASK) == 0) {
			if ((ctl & FXMAC_TXBUF_WRAP_MASK) != 0)
				sc->txring_tl_ptr = 0;
			else
				sc->txring_tl_ptr++;
			sc->txring_queued--;

			ctl = sc->txring[sc->txring_tl_ptr].ctl;

			sc->txring[sc->txring_tl_ptr].ctl =
			    ctl | FXMAC_TXBUF_USED_MASK;
		}

		/* 下一个描述符 */
		if ((ctl & FXMAC_TXBUF_WRAP_MASK) != 0)
			sc->txring_tl_ptr = 0;
		else
			sc->txring_tl_ptr++;
		sc->txring_queued--;

		if_setdrvflagbits(sc->ifp, 0, IFF_DRV_OACTIVE);
	}
}

static int
phytium_xmac_get_segs_for_tx(struct mbuf *m, bus_dma_segment_t segs[TX_MAX_DMA_SEGS], int *nsegs)
{
	int i = 0;

	do {
		if (m->m_len > 0) {
			segs[i].ds_addr = mtod(m, bus_addr_t);
			segs[i].ds_len = m->m_len;
			rtems_cache_flush_multiple_data_lines(m->m_data, m->m_len);
			++i;
		}

		m = m->m_next;

		if (m == NULL) {
			*nsegs = i;

			return (0);
		}
	} while (i < TX_MAX_DMA_SEGS);

	return (EFBIG);
}

/* 开始发送 */
static void
phytium_xmac_start_locked(if_t ifp)
{
	struct phytium_xmac_softc *sc = (struct phytium_xmac_softc *) if_getsoftc(ifp);
	struct mbuf *m;
	bus_dma_segment_t segs[TX_MAX_DMA_SEGS];
	uint32_t ctl;
	int i, nsegs, wrap, err;

	XMAC_ASSERT_LOCKED(sc);

	if ((if_getdrvflags(ifp) & IFF_DRV_OACTIVE) != 0)
		return;

	for (;;) {
		/* 检查描述符环中是否有可用空间 */
		if (sc->txring_queued >=
		    XMAC_NUM_TX_DESCS - TX_MAX_DMA_SEGS * 2) {
			/* 尝试腾出空间 */
			phytium_xmac_clean_tx(sc);

			/* 仍然没有空间 */
			if (sc->txring_queued >=
			    XMAC_NUM_TX_DESCS - TX_MAX_DMA_SEGS * 2) {
				if_setdrvflagbits(ifp, IFF_DRV_OACTIVE, 0);
				sc->txfull++;
				break;
			}
		}

		/* 获取下一个待发送数据包 */
		m = if_dequeue(ifp);
		if (m == NULL)
			break;
		
		err = phytium_xmac_get_segs_for_tx(m, segs, &nsegs);

		if (err == EFBIG) {
			/* 分段过多！请执行内存重组后重试 */
			struct mbuf *m2 = m_defrag(m, M_NOWAIT);

			if (m2 == NULL) {
				sc->txdefragfails++;
				m_freem(m);
				continue;
			}
			m = m2;

			err = phytium_xmac_get_segs_for_tx(m, segs, &nsegs);

			sc->txdefrags++;
		}
		if (err) {
			/* 放弃 */
			m_freem(m);
			sc->txdmamapfails++;
			continue;
		}
		sc->txring_m[sc->txring_hd_ptr] = m;

		/* 如果下一个数据包可能超出描述符环末端，则设置回绕标志 */
		wrap = sc->txring_hd_ptr + nsegs + TX_MAX_DMA_SEGS >= XMAC_NUM_TX_DESCS;

		/* 从后向前填充TX描述符，确保首个描述符的USED位最后被清除 */
		for (i = nsegs - 1; i >= 0; i--) {
			/* Descriptor address. */
			sc->txring[sc->txring_hd_ptr + i].addr =
			    segs[i].ds_addr;
#if defined(__LP64__)
			sc->txring[sc->txring_hd_ptr + i].addrhi =
				segs[i].ds_addr >> 32;
#elif defined(__ILP32__)
			sc->txring[sc->txring_hd_ptr + i].addrhi = 0;
#endif
			/* 描述符 control 字段 */
			ctl = segs[i].ds_len;
			if (i == nsegs - 1) {
				ctl |= FXMAC_TXBUF_LAST_MASK;
				if (wrap)
					ctl |= FXMAC_TXBUF_WRAP_MASK;
			}
			sc->txring[sc->txring_hd_ptr + i].ctl = ctl;

			if (i != 0)
				sc->txring_m[sc->txring_hd_ptr + i] = NULL;
		}

		if (wrap)
			sc->txring_hd_ptr = 0;
		else
			sc->txring_hd_ptr += nsegs;
		sc->txring_queued += nsegs;

		if (sc->version == 2) {
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address,
				FXMAC_TAIL_QUEUE(0),
				(1U << 31) | ((sc->txring_hd_ptr) & (XMAC_NUM_TX_DESCS - 1)));
		}

		/* 触发发送 */
		FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWCTRL_OFFSET, sc->net_ctl_shadow | FXMAC_NWCTRL_STARTTX_MASK);

		/* 如果存在 BPF监听器，则向其反射一份数据副本 */
		ETHER_BPF_MTAP(ifp, m);
	}
}

static void
phytium_xmac_start(if_t ifp)
{
	struct phytium_xmac_softc *sc = (struct phytium_xmac_softc *) if_getsoftc(ifp);

	XMAC_LOCK(sc);
	phytium_xmac_start_locked(ifp);
	XMAC_UNLOCK(sc);
}

static void
phytium_xmac_tick(void *arg)
{
	struct phytium_xmac_softc *sc = (struct phytium_xmac_softc *)arg;
	struct mii_data *mii;

	XMAC_ASSERT_LOCKED(sc);

	/* Poll the phy. */
	if (sc->miibus != NULL) {
		mii = device_get_softc(sc->miibus);
		mii_tick(mii);
	}

	/* Poll statistics registers. */
	// phytium_xmac_poll_hw_stats(sc);

	/* Check for receiver hang. */
	// if (sc->rxhangwar && sc->rx_frames_prev == sc->stats.rx_frames) {
	// 	/*
	// 	 * Reset receiver logic by toggling RX_EN bit.  1usec
	// 	 * delay is necessary especially when operating at 100mbps
	// 	 * and 10mbps speeds.
	// 	 */
	// 	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWCTRL_OFFSET, sc->net_ctl_shadow & ~FXMAC_NWCTRL_RXEN_MASK);
	// 	DELAY(1);
	// 	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWCTRL_OFFSET, sc->net_ctl_shadow);
	// }
	// sc->rx_frames_prev = sc->stats.rx_frames;

	/* Next callout in one second. */
	callout_reset(&sc->tick_ch, hz, phytium_xmac_tick, sc);
}

/* 中断处理程序 */
static void
phytium_xmac_intr(void *arg)
{
	struct phytium_xmac_softc *sc = (struct phytium_xmac_softc *)arg;
	if_t ifp = sc->ifp;
	uint32_t istatus;

	XMAC_LOCK(sc);

	if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0) {
		XMAC_UNLOCK(sc);
		return;
	}

	/* Read interrupt status and immediately clear the bits. */
	istatus = FXMAC_READREG32(sc->fxmac_port.instance.config.base_address, FXMAC_ISR_OFFSET);
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_ISR_OFFSET, istatus);

	/* Packets received. */
	if ((istatus & FXMAC_IXR_RXCOMPL_MASK) != 0)
	{
		phytium_xmac_recv(sc);
	}

	/* Free up any completed transmit buffers. */
	phytium_xmac_clean_tx(sc);

	/* Hresp not ok.  Something is very bad with DMA.  Try to clear. */
	if ((istatus & FXMAC_IXR_HRESPNOK_MASK) != 0) {
		device_printf(sc->dev,
		    "phytium_xmac_intr: hresp not okay! rx_status=0x%x\n",
		    FXMAC_READREG32(sc->fxmac_port.instance.config.base_address, FXMAC_RXSR_OFFSET));
		FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_RXSR_OFFSET, FXMAC_RXSR_HRESPNOK_MASK);
	}

	/* Receiver overrun. */
	if ((istatus & FXMAC_IXR_RXOVR_MASK) != 0) {
		/* Clear status bit. */
		FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_RXSR_OFFSET, FXMAC_RXSR_RXOVR_MASK);
		sc->rxoverruns++;
	}

	/* Receiver ran out of bufs. */
	if ((istatus & FXMAC_IXR_RXUSED_MASK) != 0) {
		FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWCTRL_OFFSET, sc->net_ctl_shadow |
		    FXMAC_NWCTRL_FLUSH_DPRAM_MASK);
			phytium_xmac_fill_rqueue(sc);
		sc->rxnobufs++;
	}

	/* Restart transmitter if needed. */
	if (!if_sendq_empty(ifp))
	{
		phytium_xmac_start_locked(ifp);
	}

	XMAC_UNLOCK(sc);
}

/* 重置硬件 */
static void
phytium_xmac_reset(struct phytium_xmac_softc *sc)
{
	u32 read_regs = 0;

	XMAC_ASSERT_LOCKED(sc);

	/* Determine data bus width from design configuration register. */
	read_regs = FXMAC_READREG32(sc->fxmac_port.instance.config.base_address, FXMAC_DESIGNCFG_DEBUG1_OFFSET);
    switch ((read_regs & FXMAC_DESIGNCFG_DEBUG1_BUS_WIDTH_MASK) >> 25)
    {
        case 4:
            sc->net_cfg_shadow = FXMAC_NWCFG_BUS_WIDTH_128_MASK;
			break;
        case 2:
			sc->net_cfg_shadow = FXMAC_NWCFG_BUS_WIDTH_64_MASK;
			break;
        default:
            sc->net_cfg_shadow = FXMAC_NWCFG_BUS_WIDTH_32_MASK;
    }

	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWCTRL_OFFSET, 0);
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWCFG_OFFSET, sc->net_cfg_shadow);
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWCTRL_OFFSET, FXMAC_NWCTRL_STATCLR_MASK);
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_TXSR_OFFSET, 0x1FF);//固定数值使用宏定义
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_RXSR_OFFSET, 0xF);
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_IDR_OFFSET, 0x7FFFEFF);
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_HASHL_OFFSET, 0);
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_HASHH_OFFSET, 0);

	/* manual says do this. */
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_TXQBASE_OFFSET, 0);
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_RXQBASE_OFFSET, 0);

	/* Get management port running even if interface is down. */
	sc->net_cfg_shadow |= FXMAC_NWCFG_CLOCK_DIV32_MASK;
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWCFG_OFFSET, sc->net_cfg_shadow);

	sc->net_ctl_shadow = FXMAC_NWCTRL_MDEN_MASK;
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWCTRL_OFFSET, sc->net_ctl_shadow);

	if (sc->version == 2) {
		FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address,
			FXMAC_TAIL_ENABLE,
			0x80000001);
	}
}

/* 启动硬件 */
static void phytium_xmac_config(struct phytium_xmac_softc *sc)
{
	if_t ifp = sc->ifp;
	uint32_t dma_cfg;
	u_char *eaddr = if_getlladdr(ifp);

	uint32_t status;
	uint32_t dmacrreg;
	int err;

	XMAC_ASSERT_LOCKED(sc);

	/* Program Net Config Register. */
	sc->net_cfg_shadow &= (FXMAC_NWCFG_CLOCK_DIV224_MASK |
		FXMAC_NWCFG_BUS_WIDTH_INVALID_MASK);
	sc->net_cfg_shadow |= (FXMAC_NWCFG_FCS_REMOVE_MASK |
		FXMAC_NWCFG_RX_BUF_OFFSET |
	    FXMAC_NWCFG_1000_MASK | FXMAC_NWCFG_1536RXEN_MASK |
	    FXMAC_NWCFG_FDEN_MASK | FXMAC_NWCFG_100_MASK);

	/* Check connection type, enable SGMII bits if necessary. */
	if (sc->phy_contype == MII_CONTYPE_SGMII)
	{
		sc->net_cfg_shadow |= FXMAC_NWCFG_SGMII_MODE_ENABLE_MASK;
		sc->net_cfg_shadow |= FXMAC_NWCFG_PCSSEL_MASK;
	}

	/* Enable receive checksum offloading? */
	if ((if_getcapenable(ifp) & IFCAP_RXCSUM) != 0)
		sc->net_cfg_shadow |=  FXMAC_NWCFG_RXCHKSUMEN_MASK;

	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWCFG_OFFSET, sc->net_cfg_shadow);

	/* Program DMA Config Register. */
	dma_cfg = FXMAC_DMA_CFG_RX_BUF_SIZE(MCLBYTES) |
	    FXMAC_DMACR_RXSIZE_MASK |
	    FXMAC_DMACR_TXSIZE_MASK |
	    FXMAC_DMACR_INCR16_AHB_AXI_BURST |
#ifdef XMAC64
	    FXMAC_DMACR_ADDR_WIDTH_64 |
#endif
	    FXMAC_DMACR_ORCE_DISCARD_ON_ERR_MASK;

	/* Enable transmit checksum offloading? */
	if ((if_getcapenable(ifp) & IFCAP_TXCSUM) != 0)
		dma_cfg |= FXMAC_DMACR_TCPCKSUM_MASK;

	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_DMACR_OFFSET, dma_cfg);

	/* Write the rx and tx descriptor ring addresses to the QBAR regs. */
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_RXQBASE_OFFSET, (uint32_t)sc->rxring_physaddr);
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_TXQBASE_OFFSET, (uint32_t)sc->txring_physaddr);
#ifdef XMAC64
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_MSBBUF_RXQBASE_OFFSET, (uint32_t)(sc->rxring_physaddr >> 32));
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_MSBBUF_TXQBASE_OFFSET, (uint32_t)(sc->txring_physaddr >> 32));
#endif

	/* Enable rx and tx. */
	sc->net_ctl_shadow |= (FXMAC_NWCTRL_TXEN_MASK | FXMAC_NWCTRL_RXEN_MASK);
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWCTRL_OFFSET, sc->net_ctl_shadow);

	/* Set receive address in case it changed. */
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_SA1B, (eaddr[3] << 24) | (eaddr[2] << 16) | (eaddr[1] << 8) | eaddr[0]);
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_SA1T, (eaddr[5] << 8) | eaddr[4]);

	/* 使能mask对应的中断 */
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_IER_OFFSET, FXMAC_IXR_RXCOMPL_MASK | FXMAC_IXR_RXOVR_MASK |
	    FXMAC_IXR_TXUSED_MASK | FXMAC_IXR_RXUSED_MASK |
	    FXMAC_IXR_HRESPNOK_MASK);
}

/* 启用网络接口并预填充接收环缓冲区 */
static void phytium_xmac_init_locked(struct phytium_xmac_softc *sc)
{
	struct mii_data *mii;

	XMAC_ASSERT_LOCKED(sc);

	if ((if_getdrvflags(sc->ifp) & IFF_DRV_RUNNING) != 0)
		return;

	/* 初始化 */
	phytium_xmac_config(sc);

	/* 初始化mbuf 填充接收队列 */
	phytium_xmac_fill_rqueue(sc);

	/* 设备网络接口flag */
	if_setdrvflagbits(sc->ifp, IFF_DRV_RUNNING, IFF_DRV_OACTIVE);

	if (sc->miibus != NULL)
	{
		mii = device_get_softc(sc->miibus);
		mii_mediachg(mii);
	}

	callout_reset(&sc->tick_ch, hz, phytium_xmac_tick, sc);
}

/* 网络栈初始化接口 */
static void phytium_xmac_init(void *arg)
{
	struct phytium_xmac_softc *sc = (struct phytium_xmac_softc *)arg;

	XMAC_LOCK(sc);
	phytium_xmac_init_locked(sc);
	XMAC_UNLOCK(sc);
}

/* ​​禁用网络接口，并释放发送或接收队列中的所有缓冲区 */
static void
phytium_xmac_stop(struct phytium_xmac_softc *sc)
{
	int i;

	XMAC_ASSERT_LOCKED(sc);

	callout_stop(&sc->tick_ch);

	/* Shut down hardware. */
	phytium_xmac_reset(sc);

	/* Clear out transmit queue. */
	memset(sc->txring, 0, XMAC_NUM_TX_DESCS * sizeof(struct phytium_xmac_tx_desc));
	for (i = 0; i < XMAC_NUM_TX_DESCS; i++) {
		sc->txring[i].ctl = FXMAC_TXBUF_USED_MASK;
		if (sc->txring_m[i]) {
			m_freem(sc->txring_m[i]);
			sc->txring_m[i] = NULL;
		}
	}
	sc->txring[XMAC_NUM_TX_DESCS - 1].ctl |= FXMAC_TXBUF_WRAP_MASK;
	sc->txring_hd_ptr = 0;
	sc->txring_tl_ptr = 0;
	sc->txring_queued = 0;

	/* Clear out receive queue. */
	memset(sc->rxring, 0, XMAC_NUM_RX_DESCS * sizeof(struct phytium_xmac_rx_desc));
	for (i = 0; i < XMAC_NUM_RX_DESCS; i++) {
		sc->rxring[i].addr = FXMAC_RXBUF_NEW_MASK;
		if (sc->rxring_m[i]) {
			m_freem(sc->rxring_m[i]);
			sc->rxring_m[i] = NULL;
		}
	}
	sc->rxring[XMAC_NUM_RX_DESCS - 1].addr |= FXMAC_RXBUF_WRAP_MASK;
	sc->rxring_hd_ptr = 0;
	sc->rxring_tl_ptr = 0;
	sc->rxring_queued = 0;

	/* Force next statchg or linkchg to program net config register. */
	sc->mii_media_active = 0;
}

static int
phytium_xmac_ioctl(if_t ifp, u_long cmd, caddr_t data)
{
	struct phytium_xmac_softc *sc = if_getsoftc(ifp);
	struct ifreq *ifr = (struct ifreq *)data;
	struct mii_data *mii;
	int error = 0, mask;

	switch (cmd) {
	case SIOCSIFFLAGS:
		XMAC_LOCK(sc);
		if ((if_getflags(ifp) & IFF_UP) != 0) {
			if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) != 0) {
				if (((if_getflags(ifp) ^ sc->if_old_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI)) != 0) {
						phytium_xmac_rx_filter(sc);
				}
			} else {
				phytium_xmac_init_locked(sc);
			}
		} else if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) != 0) {
			if_setdrvflagbits(ifp, 0, IFF_DRV_RUNNING);
			phytium_xmac_stop(sc);
		}
		sc->if_old_flags = if_getflags(ifp);
		XMAC_UNLOCK(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* Set up multi-cast filters. */
		if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) != 0) {
			XMAC_LOCK(sc);
			phytium_xmac_rx_filter(sc);
			XMAC_UNLOCK(sc);
		}
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		if (sc->miibus == NULL)
			return (ENXIO);
		mii = device_get_softc(sc->miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;

	case SIOCSIFCAP:
	XMAC_LOCK(sc);
		mask = if_getcapenable(ifp) ^ ifr->ifr_reqcap;

		if ((mask & IFCAP_TXCSUM) != 0) {
			if ((ifr->ifr_reqcap & IFCAP_TXCSUM) != 0) {
				/* Turn on TX checksumming. */
				if_setcapenablebit(ifp, IFCAP_TXCSUM | IFCAP_TXCSUM_IPV6, 0);
				if_sethwassistbits(ifp, XMAC_CKSUM_ASSIST, 0);

				FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_DMACR_OFFSET,
				    FXMAC_READREG32(sc->fxmac_port.instance.config.base_address, FXMAC_DMACR_OFFSET) |
				    FXMAC_DMACR_TCPCKSUM_MASK);
			} else {
				/* Turn off TX checksumming. */
				if_setcapenablebit(ifp, 0, IFCAP_TXCSUM | IFCAP_TXCSUM_IPV6);
				if_sethwassistbits(ifp, 0, XMAC_CKSUM_ASSIST);

				FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_DMACR_OFFSET,
				    FXMAC_READREG32(sc->fxmac_port.instance.config.base_address, FXMAC_DMACR_OFFSET) &
				    ~FXMAC_DMACR_TCPCKSUM_MASK);
			}
		}
		if ((mask & IFCAP_RXCSUM) != 0) {
			if ((ifr->ifr_reqcap & IFCAP_RXCSUM) != 0) {
				/* Turn on RX checksumming. */
				if_setcapenablebit(ifp, IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6, 0);
				sc->net_cfg_shadow |= FXMAC_NWCFG_RXCHKSUMEN_MASK;
				FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWCFG_OFFSET, sc->net_cfg_shadow);
			} else {
				/* Turn off RX checksumming. */
				if_setcapenablebit(ifp, 0, IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6);
				sc->net_cfg_shadow &= ~FXMAC_NWCFG_RXCHKSUMEN_MASK;
				FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWCFG_OFFSET, sc->net_cfg_shadow);
			}
		}
		if ((if_getcapenable(ifp) & (IFCAP_RXCSUM | IFCAP_TXCSUM)) ==
		    (IFCAP_RXCSUM | IFCAP_TXCSUM))
			if_setcapenablebit(ifp, IFCAP_VLAN_HWCSUM, 0);
		else
			if_setcapenablebit(ifp, 0, IFCAP_VLAN_HWCSUM);

		XMAC_UNLOCK(sc);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (error);
}

/* MII总线支持程序 */
static int
phytium_xmac_ifmedia_upd(if_t ifp)
{
	struct phytium_xmac_softc *sc = (struct phytium_xmac_softc *) if_getsoftc(ifp);
	struct mii_data *mii;
	struct mii_softc *miisc;
	int error = 0;

	mii = device_get_softc(sc->miibus);
	XMAC_LOCK(sc);
	if ((if_getflags(ifp) & IFF_UP) != 0) {
		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			PHY_RESET(miisc);
		error = mii_mediachg(mii);
	}
	XMAC_UNLOCK(sc);

	return (error);
}

static void
phytium_xmac_ifmedia_sts(if_t ifp, struct ifmediareq *ifmr)
{
	struct phytium_xmac_softc *sc = (struct phytium_xmac_softc *) if_getsoftc(ifp);
	struct mii_data *mii;

	mii = device_get_softc(sc->miibus);
	XMAC_LOCK(sc);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	XMAC_UNLOCK(sc);
}

/* MII总线支持程序 */
static void
phytium_xmac_child_detached(device_t dev, device_t child)
{
	struct phytium_xmac_softc *sc = device_get_softc(dev);

	if (child == sc->miibus)
		sc->miibus = NULL;
}

static int
phytium_xmac_miibus_readreg(device_t dev, int phy, int reg)
{
	struct phytium_xmac_softc *sc = device_get_softc(dev);
	int tries, val;
	u32 mgtcr;

	/* Construct mgtcr mask for the operation */
	mgtcr = FXMAC_PHYMNTNC_OP_MASK | FXMAC_PHYMNTNC_OP_R_MASK |
			(phy << FXMAC_PHYMNTNC_PHAD_SHFT_MSK) |
			(reg << FXMAC_PHYMNTNC_PREG_SHFT_MSK);

	/* Write mgtcr and wait for completion */
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_PHYMNTNC_OFFSET, mgtcr);			

	/* Wait for completion. */
	tries=0;
	while ((FXMAC_READREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWSR_OFFSET) & FXMAC_NWSR_MDIOIDLE_MASK) == 0) {
		DELAY(5);
		if (++tries > 200) {
			device_printf(dev, "phy read timeout: %d\n", reg);
			return (-1);
		}
	}

	val = FXMAC_READREG32(sc->fxmac_port.instance.config.base_address, FXMAC_PHYMNTNC_OFFSET) & 0xFFFF;

	// if (reg == MII_EXTSR)
		/*
		 * MAC does not support half-duplex at gig speeds.
		 * Let mii(4) exclude the capability.
		 */
		// val &= ~(EXTSR_1000XHDX | EXTSR_1000THDX);

	return (val);
}

static int
phytium_xmac_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct phytium_xmac_softc *sc = device_get_softc(dev);
	int tries;
	u32 mgtcr;

	/* Construct mgtcr mask for the operation */
	mgtcr = FXMAC_PHYMNTNC_OP_MASK | FXMAC_PHYMNTNC_OP_W_MASK |
			(phy << FXMAC_PHYMNTNC_PHAD_SHFT_MSK) |
			(reg << FXMAC_PHYMNTNC_PREG_SHFT_MSK) | (u32)data;	

	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_PHYMNTNC_OFFSET, mgtcr);

	/* Wait for completion. */
	tries = 0;
	while ((FXMAC_READREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWSR_OFFSET) & FXMAC_NWSR_MDIOIDLE_MASK) == 0) {
		DELAY(5);
		if (++tries > 200) {
			device_printf(dev, "phy write timeout: %d\n", reg);
			return (-1);
		}
	}

	return (0);
}

static void
phytium_xmac_miibus_statchg(device_t dev)
{
	struct phytium_xmac_softc *sc  = device_get_softc(dev);
	struct mii_data *mii = device_get_softc(sc->miibus);

	XMAC_ASSERT_LOCKED(sc);

	if ((mii->mii_media_status & (IFM_ACTIVE | IFM_AVALID)) ==
	    (IFM_ACTIVE | IFM_AVALID) &&
	    sc->mii_media_active != mii->mii_media_active)
		phytium_xmac_mediachange(sc, mii);
}

static void
phytium_xmac_miibus_linkchg(device_t dev)
{
	struct phytium_xmac_softc *sc  = device_get_softc(dev);
	struct mii_data *mii = device_get_softc(sc->miibus);

	XMAC_ASSERT_LOCKED(sc);

	if ((mii->mii_media_status & (IFM_ACTIVE | IFM_AVALID)) ==
	    (IFM_ACTIVE | IFM_AVALID) &&
	    sc->mii_media_active != mii->mii_media_active)
		{
			device_printf(dev, "link changed: %d\n");
			phytium_xmac_mediachange(sc, mii);
		}
}

static void phytium_xmac_set_high_speed_config(struct phytium_xmac_softc *sc, int speed)
{
	uint32_t reg_value;
    uint32_t set_speed = 0;
    switch (speed)
    {
        case 25000:
            set_speed = 2;
            break;
        case 10000:
            set_speed = 4;
            break;
        case 5000:
            set_speed = 3;
            break;
        case 2500:
            set_speed = 2;
            break;
        case 1000:
            set_speed = 1;
            break;
        default:
            set_speed = 0;
            break;
    }

	/* GEM_HSMAC(0x0050) provide rate to the external */
    reg_value = FXMAC_READREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_HSMAC);
    reg_value &= ~FXMAC_GEM_HSMACSPEED_MASK;
    reg_value |= (set_speed) &FXMAC_GEM_HSMACSPEED_MASK;
    FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_HSMAC, reg_value);

    reg_value = FXMAC_READREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_HSMAC);

	device_printf(sc->dev, "FXMAC_GEM_HSMAC = %d\n", reg_value);
}

static int
phytium_xmac_1p0_set_ref_clk(struct phytium_xmac_softc *sc,
							int interface_type,
							int interface_speed)
{
    if (interface_type == MII_CONTYPE_SGMII)
    {
		if (interface_speed == 1000) {
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_SRC_SEL_LN, 0x1); /*0x1c04*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_DIV_SEL0_LN, 0x4); /*0x1c08*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_DIV_SEL1_LN, 0x8); /*0x1c0c*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_PMA_XCVR_POWER_STATE, 0x1); /*0x1c10*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_TX_CLK_SEL0, 0x0); /*0x1c20*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_TX_CLK_SEL1, 0x0); /*0x1c24*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_TX_CLK_SEL2, 0x0); /*0x1c28*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_TX_CLK_SEL3, 0x1); /*0x1c2c*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_RX_CLK_SEL0, 0x1); /*0x1c30*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_RX_CLK_SEL1, 0x0); /*0x1c34*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_TX_CLK_SEL3_0, 0x0); /*0x1c70*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_TX_CLK_SEL4_0, 0x0); /*0x1c74*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_RX_CLK_SEL3_0, 0x0); /*0x1c78*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_RX_CLK_SEL4_0, 0x0); /*0x1c7c*/
		} else if (interface_speed == 100  || interface_speed == 10) {
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_SRC_SEL_LN, 0x1); /*0x1c04*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_DIV_SEL0_LN, 0x4); /*0x1c08*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_DIV_SEL1_LN, 0x8); /*0x1c0c*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_PMA_XCVR_POWER_STATE, 0x1); /*0x1c10*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_TX_CLK_SEL0, 0x0); /*0x1c20*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_TX_CLK_SEL1, 0x0); /*0x1c24*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_TX_CLK_SEL2, 0x1); /*0x1c28*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_TX_CLK_SEL3, 0x1); /*0x1c2c*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_RX_CLK_SEL0, 0x1); /*0x1c30*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_RX_CLK_SEL1, 0x0); /*0x1c34*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_TX_CLK_SEL3_0, 0x1); /*0x1c70*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_TX_CLK_SEL4_0, 0x0); /*0x1c74*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_RX_CLK_SEL3_0, 0x0); /*0x1c78*/
			FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_GEM_RX_CLK_SEL4_0, 0x1); /*0x1c7c*/
		}
    }

    phytium_xmac_set_high_speed_config(sc, interface_speed);

	return 0;
}

static int
phytium_xmac_2p0_set_ref_clk(struct phytium_xmac_softc *sc,
							int interface_type,
							int interface_speed)
{
	uint32_t reg_value;
	uint32_t set_speed = 0;

	if (interface_type == MII_CONTYPE_SGMII) {
		if (interface_speed == 1000) {
			set_speed = 1; /* HS_SPEED_1000M */
		} else if ((interface_speed == 100) || (interface_speed == 10)) {
			set_speed = 0; /* HS_SPEED_100M */
		}
	}

	/* FXMAC_GEM_HSMAC(0x0050) provide rate to the external */
	reg_value = FXMAC_READREG32(sc->fxmac_port.instance.config.base_address,
			FXMAC_GEM_HSMAC);
	reg_value &= ~FXMAC_GEM_HSMACSPEED_MASK;
	reg_value |= (set_speed)&FXMAC_GEM_HSMACSPEED_MASK;
	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address,
			FXMAC_GEM_HSMAC,
			reg_value);

	return 0;
}

/* Call to set reference clock and network config bits according to media. */
static void
phytium_xmac_mediachange(struct phytium_xmac_softc *sc,	struct mii_data *mii)
{
	int interface_speed = 1000;

	XMAC_ASSERT_LOCKED(sc);

	/* Update hardware to reflect media. */
	sc->net_cfg_shadow &= ~(FXMAC_NWCFG_100_MASK | FXMAC_NWCFG_1000_MASK | FXMAC_NWCFG_FDEN_MASK);

	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_1000_T:
		sc->net_cfg_shadow |= (FXMAC_NWCFG_100_MASK | FXMAC_NWCFG_1000_MASK);
		interface_speed = 1000;
		break;
	case IFM_100_TX:
		sc->net_cfg_shadow |= FXMAC_NWCFG_100_MASK;
		interface_speed = 100;
		break;
	default:
		break;
	}

	if ((mii->mii_media_active & IFM_FDX) != 0)
		sc->net_cfg_shadow |= FXMAC_NWCFG_FDEN_MASK;

	FXMAC_WRITEREG32(sc->fxmac_port.instance.config.base_address, FXMAC_NWCFG_OFFSET, sc->net_cfg_shadow);

	/* 根据速率设置时钟 */
	XMAC_UNLOCK(sc);
	if (sc->version == 1) {
		if (phytium_xmac_1p0_set_ref_clk(sc, sc->phy_contype,
			interface_speed)) {
			device_printf(sc->dev,
				"phytium_xmac_mediachange(v1): "
				"could not set ref clk for speed %d.\n",
				interface_speed);
		}
	} else if (sc->version == 2) {
		if (phytium_xmac_2p0_set_ref_clk(sc, sc->phy_contype,
		    interface_speed)) {
			device_printf(sc->dev,
			    "phytium_xmac_mediachange(v2): "
			    "could not set ref clk for speed %d.\n",
			    interface_speed);
		}
	} else {
		panic("phytium_xmac_mediachange: unknown version");
	}
	XMAC_LOCK(sc);

	sc->mii_media_active = mii->mii_media_active;
}

static int phytium_xmac_probe(device_t dev)
{
	/* 检查设备树 status */
	if (!ofw_bus_status_okay(dev))
	{
		return (ENXIO);
	}
		
	/* 匹配设备树 compatible */
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_str == NULL)
	{
		return (ENXIO);
	}
	
	/* 添加设备描述 */
	device_set_desc(dev, "Phytium XMAC Gigabit Ethernet Interface");

	return (0);
}

static int phytium_xmac_attach(device_t dev)
{
	struct phytium_xmac_softc *sc = device_get_softc(dev);
	if_t ifp = NULL;
	int rid, err;
	u_char eaddr[ETHER_ADDR_LEN];
	int hwquirks;

	sc->dev = dev;
	XMAC_LOCK_INIT(sc);

	/* 设置硬件专用选项 */
	hwquirks = ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	if ((hwquirks & HWQUIRK_VERSION2) != 0)
		sc->version = 2;
	else
		sc->version = 1;

	sc->rxhangwar = 0;

	/* 设置PHY类型 */
	sc->phy_contype = MII_CONTYPE_SGMII;

	/* 基地址 */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "could not allocate memory resources.\n");
		return (ENOMEM);
	}

	/* 中断号 */
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "could not allocate interrupt resource.\n");
		phytium_xmac_detach(dev);
		return (ENOMEM);
	}

	/* 获取控制器ID、基地址 */
	sc->fxmac_port.instance.config.instance_id = device_get_unit(dev);
	sc->fxmac_port.instance.config.base_address = rman_get_bushandle(sc->mem_res);
	device_printf(dev, "XMAC-%d register base: 0x%x\n", sc->fxmac_port.instance.config.instance_id, sc->fxmac_port.instance.config.base_address);

	/* 设置网络协议栈 */
	ifp = sc->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL)
	{
		device_printf(dev, "could not allocate ifnet structure\n");
		phytium_xmac_detach(dev);
		return (ENOMEM);
	}
	if_setsoftc(ifp, sc);
	if_initname(ifp, IF_XMAC_NAME, device_get_unit(dev));
	if_setflags(ifp, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);

	/* 注册初始化函数 */
	if_setinitfn(ifp, phytium_xmac_init);

	/* 注册特性 */
	if_setioctlfn(ifp, phytium_xmac_ioctl);

	/* 注册发送函数 */
	if_setstartfn(ifp, phytium_xmac_start);

	// if_setcapabilitiesbit(ifp, IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6 | IFCAP_VLAN_MTU | IFCAP_VLAN_HWCSUM, 0);
	if_setsendqlen(ifp, XMAC_NUM_TX_DESCS);
	if_setsendqready(ifp);

	/* 默认禁用硬件校验和功能 */
	if_sethwassist(ifp, 0);
	if_setcapenable(ifp, if_getcapabilities(ifp) & ~(IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6 | IFCAP_VLAN_HWCSUM));

	sc->if_old_flags = if_getflags(ifp);
	sc->rxbufs = DEFAULT_NUM_RX_BUFS;

	/* 重置硬件 */
	XMAC_LOCK(sc);
	phytium_xmac_reset(sc);
	XMAC_UNLOCK(sc);

	/* 将PHY（物理层设备）连接到mii总线 */
	err = mii_attach(dev, &sc->miibus, ifp,
		phytium_xmac_ifmedia_upd, phytium_xmac_ifmedia_sts, BMSR_DEFCAPMASK,
		MII_PHY_ANY, MII_OFFSET_ANY, 0);
	if (err)
		device_printf(dev, "warning: attaching PHYs failed\n");

	/* 设置TX RX描述符 */
	err = phytium_xmac_setup_descs(sc);
	if (err)
	{
		device_printf(dev, "could not set up dma mem for descs.\n");
		phytium_xmac_detach(dev);
		return (ENOMEM);
	}

	/* 获取MAC地址 */
	phytium_xmac_get_mac(sc, eaddr);

	/* 初始化定时器 */
	callout_init_mtx(&sc->tick_ch, &sc->sc_mtx, 0);

	/* 注册到协议栈 */
	ether_ifattach(ifp, eaddr);

	/* 设置中断 */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET | INTR_MPSAFE | INTR_EXCL, NULL, phytium_xmac_intr, sc, &sc->intrhand);
	if (err)
	{
		device_printf(dev, "could not set interrupt handler.\n");
		ether_ifdetach(ifp);
		phytium_xmac_detach(dev);
		return (err);
	}

	return (0);
}

static int phytium_xmac_detach(device_t dev)
{
	struct phytium_xmac_softc *sc = device_get_softc(dev);

	if (sc == NULL)
		return (ENODEV);

	if (device_is_attached(dev))
	{
		XMAC_LOCK(sc);
		phytium_xmac_stop(sc);
		XMAC_UNLOCK(sc);
		callout_drain(&sc->tick_ch);
		if_setflagbits(sc->ifp, 0, IFF_UP);
		ether_ifdetach(sc->ifp);
	}

	if (sc->miibus != NULL)
	{
		device_delete_child(dev, sc->miibus);
		sc->miibus = NULL;
	}

	/* Release DMA resources. */
	if (sc->rxring != NULL)
	{
		if (sc->rxring_physaddr != 0)
		{
			bus_dmamap_unload(sc->desc_dma_tag, sc->rxring_dma_map);
			sc->rxring_physaddr = 0;
			sc->txring_physaddr = 0;
		}
		bus_dmamem_free(sc->desc_dma_tag, sc->rxring, sc->rxring_dma_map);
		sc->rxring = NULL;
		sc->txring = NULL;
	}
	if (sc->txring != NULL)
	{
		if (sc->txring_physaddr != 0)
		{
			bus_dmamap_unload(sc->desc_dma_tag, sc->txring_dma_map);
			sc->txring_physaddr = 0;
		}
		bus_dmamem_free(sc->desc_dma_tag, sc->txring, sc->txring_dma_map);
		sc->txring = NULL;
	}
	if (sc->desc_dma_tag != NULL)
	{
		bus_dma_tag_destroy(sc->desc_dma_tag);
		sc->desc_dma_tag = NULL;
	}
	if (sc->mbuf_dma_tag != NULL)
	{
		bus_dma_tag_destroy(sc->mbuf_dma_tag);
		sc->mbuf_dma_tag = NULL;
	}

	bus_generic_detach(dev);

	XMAC_LOCK_DESTROY(sc);

	return (0);
}

/* 驱动方法 */
static device_method_t phytium_xmac_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		phytium_xmac_probe),
	DEVMETHOD(device_attach,	phytium_xmac_attach),
	DEVMETHOD(device_detach,	phytium_xmac_detach),

	/* Bus interface */
	DEVMETHOD(bus_child_detached,	phytium_xmac_child_detached),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	phytium_xmac_miibus_readreg),
	DEVMETHOD(miibus_writereg,	phytium_xmac_miibus_writereg),
	DEVMETHOD(miibus_statchg,	phytium_xmac_miibus_statchg),
	DEVMETHOD(miibus_linkchg,	phytium_xmac_miibus_linkchg),

	DEVMETHOD_END
};


/* 驱动信息 */
static driver_t phytium_xmac_driver = {
	"phytium_xmac",
	phytium_xmac_methods,
	sizeof(struct phytium_xmac_softc),
};

DRIVER_MODULE(phytium_xmac, simplebus, phytium_xmac_driver, NULL, NULL);

#ifdef __rtems__
DRIVER_MODULE(phytium_xmac, nexus, phytium_xmac_driver, NULL, NULL);
#endif /* __rtems__ */

DRIVER_MODULE(phytium_xmac, ofwbus, phytium_xmac_driver, NULL, NULL);

DRIVER_MODULE(miibus, phytium_xmac, miibus_driver, NULL, NULL);

MODULE_DEPEND(phytium_xmac, miibus, 1, 1, 1);
MODULE_DEPEND(phytium_xmac, ether, 1, 1, 1);

SIMPLEBUS_PNP_INFO(compat_data);
