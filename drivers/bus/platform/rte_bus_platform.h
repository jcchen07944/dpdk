/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Realtek Semiconductor Corp.
 */

#ifndef _RTE_BUS_PLATFORM_H_
#define _RTE_BUS_PLATFORM_H_

/**
 * @file
 * Platform device & driver interface
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_debug.h>
#include <rte_interrupts.h>
#include <rte_dev.h>
#include <rte_bus.h>
#include <rte_platform.h>

/** Pathname of platform devices directory. */
const char *rte_platform_get_sysfs_path(void);

/* Forward declarations */
struct rte_platform_device;
struct rte_platform_driver;

/** List of platform devices */
RTE_TAILQ_HEAD(rte_platform_device_list, rte_platform_device);
/** List of platform drivers */
RTE_TAILQ_HEAD(rte_platform_driver_list, rte_platform_driver);

/* Platform Bus iterators */
#define FOREACH_DEVICE_ON_PLATFORMBUS(p)	\
		RTE_TAILQ_FOREACH(p, &(rte_platform_bus.device_list), next)

#define FOREACH_DRIVER_ON_PLATFORMBUS(p)	\
		RTE_TAILQ_FOREACH(p, &(rte_platform_bus.driver_list), next)

struct rte_devargs;

enum rte_platform_kernel_driver {
	RTE_PLATFORM_KDRV_UNKNOWN = 0,
	RTE_PLATFORM_KDRV_UIO_GENERIC
};

/**
 * A structure describing a platform device.
 */
struct rte_platform_device {
	RTE_TAILQ_ENTRY(rte_platform_device) next;
	                                     /**< Next probed platform device. */
	char name[RTE_DEV_NAME_MAX_LEN+1];   /**< Platform device name */
	char dev_path[PATH_MAX];             /**< Path of device in sysfs */
	struct rte_device device;            /**< Inherit core device */
	struct rte_platform_addr addr;       /**< Platform device location. */
	struct rte_platform_id id[PLATFORM_MAX_COMPATIBLE];
	                                     /**< Platform device id. */
	struct rte_mem_resource mem_resource[PLATFORM_MAX_RESOURCE];
	                                     /**< Platform Memory Resource */
	struct rte_intr_handle *intr_handle; /**< Interrupt handle */
	struct rte_platform_driver *driver;  /**< Platform driver used in probing */
	enum rte_platform_kernel_driver kdrv;
};

/**
 * @internal
 * Helper macro for drivers that need to convert to struct rte_platform_device.
 */
#define RTE_DEV_TO_PLATFORM(ptr) \
	container_of(ptr, struct rte_platform_device, device)

#define RTE_DEV_TO_PLATFORM_CONST(ptr) \
	container_of(ptr, const struct rte_platform_device, device)

#define RTE_ETH_DEV_TO_PLATFORM(eth_dev) \
	RTE_DEV_TO_PLATFORM((eth_dev)->device)

/**
 * Initialisation function for the driver called during platform probing.
 */
typedef int (rte_platform_probe_t)(struct rte_platform_driver *,
			    struct rte_platform_device *);

/**
 * Uninitialisation function for the driver called during hotplugging.
 */
typedef int (rte_platform_remove_t)(struct rte_platform_device *);

/**
 * Driver-specific DMA mapping. After a successful call the device
 * will be able to read/write from/to this segment.
 *
 * @param dev
 *   Pointer to the platform device.
 * @param addr
 *   Starting virtual address of memory to be mapped.
 * @param iova
 *   Starting IOVA address of memory to be mapped.
 * @param len
 *   Length of memory segment being mapped.
 * @return
 *   - 0 On success.
 *   - Negative value and rte_errno is set otherwise.
 */
typedef int (platform_dma_map_t)(struct rte_platform_device *dev, void *addr,
			    uint64_t iova, size_t len);

/**
 * Driver-specific DMA un-mapping. After a successful call the device
 * will not be able to read/write from/to this segment.
 *
 * @param dev
 *   Pointer to the platform device.
 * @param addr
 *   Starting virtual address of memory to be unmapped.
 * @param iova
 *   Starting IOVA address of memory to be unmapped.
 * @param len
 *   Length of memory segment being unmapped.
 * @return
 *   - 0 On success.
 *   - Negative value and rte_errno is set otherwise.
 */
typedef int (platform_dma_unmap_t)(struct rte_platform_device *dev, void *addr,
			      uint64_t iova, size_t len);

/**
 * A structure describing a platform driver.
 */
struct rte_platform_driver {
	RTE_TAILQ_ENTRY(rte_platform_driver) next;  /**< Next in list. */
	struct rte_driver driver;                 /**< Inherit core driver. */
	struct rte_platform_bus *bus;             /**< Platform bus reference. */
	rte_platform_probe_t *probe;              /**< Device probe function. */
	rte_platform_remove_t *remove;            /**< Device remove function. */
	platform_dma_map_t *dma_map;              /**< device dma map function. */
	platform_dma_unmap_t *dma_unmap;          /**< device dma unmap function. */
	const struct rte_platform_id *id_table;   /**< ID table, NULL terminated. */
	uint32_t drv_flags;                       /**< Flags RTE_PLATFORM_DRV_*. */
};

/**
 * Structure describing the platform bus
 */
struct rte_platform_bus {
	struct rte_bus bus;               /**< Inherit the generic class */
	struct rte_platform_device_list device_list;  /**< List of platform devices */
	struct rte_platform_driver_list driver_list;  /**< List of platform drivers */
};

#define RTE_PLATFORM_DRV_NEED_MAPPING 0x0001

/**
 * Map the platform device resources in user space virtual memory address
 *
 * Note that driver should not call this function when flag
 * RTE_PLATFORM_DRV_NEED_MAPPING is set, as EAL will do that for
 * you when it's on.
 *
 * @param dev
 *   A pointer to a rte_platform_device structure describing the device
 *   to use
 *
 * @return
 *   0 on success, negative on error and positive if no driver
 *   is found for the device.
 */
int rte_platform_map_device(struct rte_platform_device *dev);

/**
 * Unmap this device
 *
 * @param dev
 *   A pointer to a rte_platform_device structure describing the device
 *   to use
 */
void rte_platform_unmap_device(struct rte_platform_device *dev);

/**
 * Register a platform driver.
 *
 * @param driver
 *   A pointer to a rte_platform_driver structure describing the driver
 *   to be registered.
 */
void rte_platform_register(struct rte_platform_driver *driver);

/** Helper for platform device registration from driver (eth, crypto) instance */
#define RTE_PMD_REGISTER_PLATFORM(nm, platform_drv) \
RTE_INIT(platforminitfn_ ##nm) \
{\
	(platform_drv).driver.name = RTE_STR(nm);\
	rte_platform_register(&platform_drv); \
} \
RTE_PMD_EXPORT_NAME(nm, __COUNTER__)

/**
 * Unregister a platform driver.
 *
 * @param driver
 *   A pointer to a rte_platform_driver structure describing the driver
 *   to be unregistered.
 */
void rte_platform_unregister(struct rte_platform_driver *driver);

/**
 * Read platform config space.
 *
 * @param device
 *   A pointer to a rte_platform_device structure describing the device
 *   to use
 * @param buf
 *   A data buffer where the bytes should be read into
 * @param len
 *   The length of the data buffer.
 * @param offset
 *   The offset into platform config space
 * @return
 *  Number of bytes read on success, negative on error.
 */
int rte_platform_read_config(const struct rte_platform_device *device,
		void *buf, size_t len, off_t offset);

/**
 * Write platform config space.
 *
 * @param device
 *   A pointer to a rte_platform_device structure describing the device
 *   to use
 * @param buf
 *   A data buffer containing the bytes should be written
 * @param len
 *   The length of the data buffer.
 * @param offset
 *   The offset into platform config space
 */
int rte_platform_write_config(const struct rte_platform_device *device,
		const void *buf, size_t len, off_t offset);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BUS_PLATFORM_H_ */
