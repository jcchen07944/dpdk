/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Realtek Semiconductor Corp.
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <inttypes.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>

#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_platform.h>
#include <rte_bus_platform.h>
#include <rte_branch_prediction.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <ethdev_platform.h>
#include <rte_string_fns.h>
#include <rte_malloc.h>
#include <rte_dev.h>

#include "rtk_logs.h"
#include "rtk_ethdev.h"

static const struct rte_platform_id rtk_compatible_table[] = {
	{ "rtk,rtl9311b-pmd" },
	{ "" },
};


static int
rtk_dev_configure(struct rte_eth_dev *dev)
{
	return 0;
}

static const struct eth_dev_ops rtk_eth_dev_ops = {
	.dev_configure        = rtk_dev_configure,
};

static int eth_rtk_platform_probe(struct rte_platform_driver *drv __rte_unused,
	struct rte_platform_device *dev)
{
	struct rte_eth_dev *eth_dev;
	struct rtk_hw *hw;

	RTE_LOG(DEBUG, EAL, "%s:%d Probe realtek PMD\n", __func__, __LINE__);

	PMD_INIT_FUNC_TRACE();

	eth_dev = rte_eth_platform_allocate(dev, sizeof(struct rtk_hw));
	if (!eth_dev)
		goto eth_alloc_error;
	eth_dev->dev_ops = &rtk_eth_dev_ops;
	eth_dev->rx_pkt_burst = &rtk_recv_pkts;
	eth_dev->tx_pkt_burst = &rtk_xmit_pkts;
	eth_dev->tx_pkt_prepare = rtk_prep_pkts;

	hw = eth_dev->data->dev_private;
	hw->hw_addr = (void *)dev->mem_resource[0].addr;
	RTE_LOG(DEBUG, EAL, "%s:%d Read REG = %x\n", __func__, __LINE__,
				RTK_READ_REG(hw, 0x020002B0));
	//RTK_WRITE_REG(hw, 0x020002B0, 0xA0150B88);

	
	// Read MAC address from hardware, save to hw & eth_dev
	// eth_dev->data->mac_addrs = rte_zmalloc("rtk", RTE_ETHER_ADDR_LEN *
	// 				       RTK_MAX_MAC_ADDRS, 0);
	// if (eth_dev->data->mac_addrs == NULL) {
	// 	PMD_INIT_LOG(ERR,
	// 		     "Failed to allocate %d bytes needed to store MAC addresses",
	// 		     RTE_ETHER_ADDR_LEN * RTK_MAX_MAC_ADDRS);
	// 	return -ENOMEM;
	// }
	// /* Copy the permanent MAC address */
	// rte_ether_addr_copy((struct rte_ether_addr *)hw->perm_addr,
	// 		&eth_dev->data->mac_addrs[0]);


	// Set initial link status, like duplex/speed/auto-nego

	return 0;

eth_alloc_error:
	return -1;
}

static int eth_rtk_platform_remove(struct rte_platform_device *dev)
{
	PMD_INIT_FUNC_TRACE();

	return 0;
}

static struct rte_platform_driver rte_rtk_pmd = {
	.id_table = rtk_compatible_table,
	.drv_flags = RTE_PLATFORM_DRV_NEED_MAPPING,
	.probe = eth_rtk_platform_probe,
	.remove = eth_rtk_platform_remove,
};

RTE_PMD_REGISTER_PLATFORM(net_rtk, rte_rtk_pmd);
RTE_PMD_REGISTER_KMOD_DEP(net_rtk, "* uio_pdrv_genirq");
RTE_LOG_REGISTER_SUFFIX(rtk_logtype_init, init, NOTICE);
// RTE_LOG_REGISTER_SUFFIX(rtk_logtype_driver, driver, NOTICE);
