/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Realtek Semiconductor Corp.
 */

#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>

#include <rte_errno.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_bus.h>
#include <rte_eal_paging.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_debug.h>

#include "rte_platform.h"

void
rte_platform_device_name(const struct rte_platform_addr *addr,
		char *output, size_t size)
{
	RTE_VERIFY(addr != NULL);
	RTE_VERIFY(size >= strlen(addr->dts_path));
	RTE_VERIFY(snprintf(output, size, "%s", addr->dts_path) >= 0);
}

int
rte_platform_addr_cmp(const struct rte_platform_addr *addr1,
	     const struct rte_platform_addr *addr2)
{
	RTE_VERIFY(addr1 != NULL);
	RTE_VERIFY(addr2 != NULL);

	return strcmp(addr1->dts_path, addr2->dts_path);
}
