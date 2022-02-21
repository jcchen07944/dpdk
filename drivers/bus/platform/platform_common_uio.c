/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Realtek Semiconductor Corp.
 */

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <rte_eal.h>
#include <rte_platform.h>
#include <rte_bus_platform.h>
#include <rte_tailq.h>
#include <rte_log.h>
#include <rte_malloc.h>

#include "private.h"

static struct rte_tailq_elem platform_tailq = {
	.name = "PLATFORM_RESOURCE_LIST",
};
EAL_REGISTER_TAILQ(platform_tailq)

int
platform_uio_map_resource(struct rte_platform_device *dev)
{
	int ret, map_idx = 0, i;
	uint64_t len;
	struct mapped_platform_resource *uio_res = NULL;
	struct mapped_platform_res_list *uio_res_list =
		RTE_TAILQ_CAST(platform_tailq.head, mapped_platform_res_list);

	if (rte_intr_fd_set(dev->intr_handle, -1))
		return -1;

	if (rte_intr_dev_fd_set(dev->intr_handle, -1))
		return -1;

	/* Allocate uio resource */
	ret = platform_uio_alloc_resource(dev, &uio_res);
	if (ret)
		return ret;

	/* Map all resource */
	for (i = 0; i != PLATFORM_MAX_RESOURCE; i++) {
		/* Skip mapping zero length reg */
		len = dev->mem_resource[i].len;
		if (len == 0)
			continue;

		ret = platform_uio_map_resource_by_index(dev, i,
				uio_res, map_idx);
		if (ret)
			return ret;

		map_idx++;
	}

	TAILQ_INSERT_TAIL(uio_res_list, uio_res, next);

	return 0;
}
