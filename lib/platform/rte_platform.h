/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Realtek Semiconductor Corp.
 */

#ifndef _RTE_PLATFORM_H_
#define _RTE_PLATFORM_H_

/**
 * @file
 *
 * RTE Platform Library
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/types.h>

/** Maximum number of platform resources. */
#define PLATFORM_MAX_RESOURCE 6
#define PLATFORM_MAX_COMPATIBLE 4
#define PLATFORM_MAX_COMPATIBLE_NAME 64

struct rte_platform_id {
	char compatible[PLATFORM_MAX_COMPATIBLE_NAME];
};

struct rte_platform_addr {
	char dts_path[PATH_MAX];
};

/**
 * Utility function to write a platform device name, this device name can later
 * be used to retrieve the corresponding rte_platform_addr using
 * eal_parse_platform_* BDF helpers.
 *
 * @param addr
 *	The platform Bus-Device-Function address
 * @param output
 *	The output buffer string
 * @param size
 *	The output buffer size
 */
void rte_platform_device_name(const struct rte_platform_addr *addr,
		     char *output, size_t size);

/**
 * Utility function to compare two platform device addresses.
 *
 * @param addr
 *	The platform Bus-Device-Function address to compare
 * @param addr2
 *	The platform Bus-Device-Function address to compare
 * @return
 *	0 on equal platform address.
 *	Positive on addr1 is greater than addr2.
 *	Negative on addr1 is less than addr2, or error.
 */
int rte_platform_addr_cmp(const struct rte_platform_addr *addr1,
		     const struct rte_platform_addr *addr2);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PLATFORM_H_ */
