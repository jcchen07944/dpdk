/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Realtek Semiconductor Corp.
 */

#ifndef _PLATFORM_PRIVATE_H_
#define _PLATFORM_PRIVATE_H_

#include <stdbool.h>
#include <stdio.h>

#include <rte_bus_platform.h>
#include <rte_os_shim.h>
#include <rte_platform.h>

extern struct rte_platform_bus rte_platform_bus;

struct rte_platform_driver;
struct rte_platform_device;

/**
 * Scan the content of the platform bus, and the devices in the devices
 * list
 *
 * @return
 *  0 on success, negative on error
 */
int rte_platform_scan(void);

int
uevent_find_entry(const char *filename, const char *key, char value[]);

/**
 * Set the name of a platform device.
 */
void
platform_name_set(struct rte_platform_device *dev);

/**
 * Validate whether a device with given platform address should be ignored or
 * not.
 *
 * @param addr
 *	platform address of device to be validated
 * @return
 *	true: if device is to be ignored,
 *	false: if device is to be scanned,
 */
bool rte_platform_ignore_device(const struct rte_platform_addr *addr);

/**
 * Add a platform device to the Platform Bus (append to Platform Device list).
 * This function also updates the bus references of the Platform Device (and
 * the generic device object embedded within.
 *
 * @param dev
 *	Platform device to add
 * @return void
 */
void rte_platform_add_device(struct rte_platform_device *dev);

/**
 * Insert a platform device in the Platform Bus at a particular location in
 * the device list. It also updates the Platform Bus reference of the new
 * devices to be inserted.
 *
 * @param exist_platform_dev
 *	Existing platform device in Platform Bus
 * @param new_platform_dev
 *	Platform device to be added before exist_platform_dev
 * @return void
 */
void rte_platform_insert_device(struct rte_platform_device *exist_platform_dev,
		struct rte_platform_device *new_platform_dev);

/**
 * A structure describing a platform device mapping.
 */
struct platform_map {
	void *addr;
	uint64_t size;
	uint64_t phaddr;
};

/**
 * A structure describing a mapped platform device resource.
 * For multi-process we need to reproduce all platform mappings in secondary
 * processes, so save them in a tailq.
 */
struct mapped_platform_resource {
	TAILQ_ENTRY(mapped_platform_resource) next;

	struct rte_platform_addr platform_addr;
	char path[PATH_MAX];
	int nb_maps;
	struct platform_map maps[PLATFORM_MAX_RESOURCE];
};

/** mapped platform device list */
TAILQ_HEAD(mapped_platform_res_list, mapped_platform_resource);

/**
 * Map a particular resource from a file.
 *
 * @param requested_addr
 *      The starting address for the new mapping range.
 * @param fd
 *      The file descriptor.
 * @param offset
 *      The offset for the mapping range.
 * @param size
 *      The size for the mapping range.
 * @param additional_flags
 *      The additional rte_mem_map() flags for the mapping range.
 * @return
 *   - On success, the function returns a pointer to the mapped area.
 *   - On error, NULL is returned.
 */
void *platform_map_resource(void *requested_addr, int fd, off_t offset,
		size_t size, int additional_flags);

/**
 * Unmap a particular resource.
 *
 * @param requested_addr
 *      The address for the unmapping range.
 * @param size
 *      The size for the unmapping range.
 */
void platform_unmap_resource(void *requested_addr, size_t size);

/**
 * Map the platform resource of a platform device in virtual memory
 *
 * This function is private to EAL.
 *
 * @return
 *   0 on success, negative on error
 */
int platform_uio_map_resource(struct rte_platform_device *dev);

/**
 * Unmap the platform resource of a platform device
 *
 * This function is private to EAL.
 */
void platform_uio_unmap_resource(struct rte_platform_device *dev);

/**
 * Allocate uio resource for platform device
 *
 * This function is private to EAL.
 *
 * @param dev
 *   Platform device to allocate uio resource
 * @param uio_res
 *   Pointer to uio resource.
 *   If the function returns 0, the pointer will be filled.
 * @return
 *   0 on success, negative on error
 */
int platform_uio_alloc_resource(struct rte_platform_device *dev,
		struct mapped_platform_resource **uio_res);

/**
 * Free uio resource for platform device
 *
 * This function is private to EAL.
 *
 * @param dev
 *   Platform device to free uio resource
 * @param uio_res
 *   Pointer to uio resource.
 */
void platform_uio_free_resource(struct rte_platform_device *dev,
		struct mapped_platform_resource *uio_res);

int
find_max_end_va(const struct rte_memseg_list *msl, void *arg);

void *
platform_find_max_end_va(void);

int
platform_uio_map_resource_by_index(struct rte_platform_device *dev,
		int res_idx, struct mapped_platform_resource *uio_res, int map_idx);

/*
 * Match the Platform Driver and Device using the ID Table
 *
 * @param platform_drv
 *      Platform driver from which ID table would be extracted
 * @param platform_dev
 *      Platform device to match against the driver
 * @return
 *      1 for successful match
 *      0 for unsuccessful match
 */
int
rte_platform_match(const struct rte_platform_driver *platform_drv,
	      const struct rte_platform_device *platform_dev);

#endif /* _PLATFORM_PRIVATE_H_ */
