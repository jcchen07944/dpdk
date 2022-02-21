/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Realtek Semiconductor Corp.
 */

#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <rte_errno.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_bus.h>
#include <rte_platform.h>
#include <rte_bus_platform.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_eal_paging.h>
#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_devargs.h>
#include <rte_vfio.h>

#include "private.h"


#define SYSFS_PLATFORM_DEVICES "/sys/bus/platform/devices"

const char *rte_platform_get_sysfs_path(void)
{
	const char *path = NULL;

#ifdef RTE_EXEC_ENV_LINUX
	path = getenv("SYSFS_PLATFORM_DEVICES");
	if (path == NULL)
		return SYSFS_PLATFORM_DEVICES;
#endif

	return path;
}

static struct rte_devargs *
platform_devargs_lookup(const struct rte_platform_addr *platform_addr)
{
	struct rte_devargs *devargs;
	struct rte_platform_addr addr;

	RTE_EAL_DEVARGS_FOREACH("platform", devargs) {
		devargs->bus->parse(devargs->name, &addr);
		if (!rte_platform_addr_cmp(platform_addr, &addr))
			return devargs;
	}
	return NULL;
}

void
platform_name_set(struct rte_platform_device *dev)
{
	struct rte_devargs *devargs;

	/* Each device has its internal, canonical name set. */
	rte_platform_device_name(&dev->addr, dev->name, sizeof(dev->name));
	devargs = platform_devargs_lookup(&dev->addr);
	dev->device.devargs = devargs;

	/* When using a blocklist, only blocked devices will have
	 * an rte_devargs. Allowed devices won't have one.
	 */
	if (devargs != NULL)
		/* If an rte_devargs exists, the generic rte_device uses the
		 * given name as its name.
		 */
		dev->device.name = dev->device.devargs->name;
	else
		/* Otherwise, it uses the internal, canonical form. */
		dev->device.name = dev->name;
}

/* map a particular resource from a file */
void *
platform_map_resource(void *requested_addr, int fd, off_t offset, size_t size,
		 int flags)
{
	void *mapaddr;

	/* Map the platform memory resource of device */
	mapaddr = rte_mem_map(requested_addr, size,
		PROT_READ | PROT_WRITE,
		MAP_SHARED | flags, fd, offset);
	if (mapaddr == NULL) {
		RTE_LOG(ERR, EAL,
			"%s(): cannot map resource(%d, %p, 0x%zx, 0x%llx): %s\n",
			__func__, fd, requested_addr, size,
			(unsigned long long)offset,
			rte_strerror(rte_errno));
	} else {
		RTE_LOG(DEBUG, EAL, "  Platform memory mapped at %p\n", mapaddr);
	}

	return mapaddr;
}

/* unmap a particular resource */
void
platform_unmap_resource(void *requested_addr, size_t size)
{
	if (requested_addr == NULL)
		return;

	/* Unmap the platform memory resource of device */
	if (rte_mem_unmap(requested_addr, size)) {
		RTE_LOG(ERR, EAL, "%s(): cannot mem unmap(%p, %#zx): %s\n",
			__func__, requested_addr, size,
			rte_strerror(rte_errno));
	} else
		RTE_LOG(DEBUG, EAL, "  Platform memory unmapped at %p\n",
				requested_addr);
}
/*
 * Match the platform Driver and Device using the compatible
 */
int
rte_platform_match(const struct rte_platform_driver *drv,
			const struct rte_platform_device *dev)
{
	const struct rte_platform_id *id_table;

	/* check if device's compatibles match the driver's ones */
	for (int i = 0; i < PLATFORM_MAX_COMPATIBLE; i++) {
		for (id_table = drv->id_table; strcmp(id_table->compatible, "") != 0;
			 id_table++) {
			if (dev->id[i].compatible == NULL)
				continue;
			if (strcmp(id_table->compatible, dev->id[i].compatible) != 0)
				continue;
			return 1;
		}
	}

	return 0;
}

/*
 * If compatible match, call the probe() function of the
 * driver.
 */
static int
rte_platform_probe_one_driver(struct rte_platform_driver *drv,
			struct rte_platform_device *dev)
{
	int ret;
	bool already_probed;

	if ((drv == NULL) || (dev == NULL))
		return -EINVAL;

	/* The device is not blocked; Check if driver supports it */
	if (!rte_platform_match(drv, dev))
		/* Match of device and driver failed */
		return 1;

	already_probed = rte_dev_is_probed(&dev->device);
	if (already_probed) {
		RTE_LOG(DEBUG, EAL, "Device %s is already probed\n",
				dev->device.name);
		return -EEXIST;
	}

	dev->intr_handle = rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_PRIVATE);
	if (dev->intr_handle == NULL) {
		RTE_LOG(ERR, EAL,
			"Failed to create interrupt instance for %s\n",
			dev->device.name);
		ret = -ENOMEM;
		goto alloc_error;
	}

	if (drv->drv_flags & RTE_PLATFORM_DRV_NEED_MAPPING) {
		ret = rte_platform_map_device(dev);
		if (ret)
			goto map_error;
	}

	dev->driver = drv;
	RTE_LOG(DEBUG, EAL, "  probe driver: %s\n", dev->addr.dts_path);

	/* call the driver probe() function */
	ret = drv->probe(drv, dev);
	if (ret)
		goto probe_error;
	else
		dev->device.driver = &drv->driver;

	return ret;

probe_error:
	dev->driver = NULL;
map_error:
alloc_error:
	rte_intr_instance_free(dev->intr_handle);
	dev->intr_handle = NULL;
	return ret;
}

/*
 * If compatible match, call the probe() function of all
 * registered driver for the given device. Return < 0 if initialization
 * failed, return 1 if no driver is found for this device.
 */
static int
platform_probe_all_drivers(struct rte_platform_device *dev)
{
	struct rte_platform_driver *drv = NULL;
	int ret = 0;

	if (dev == NULL)
		return -EINVAL;

	FOREACH_DRIVER_ON_PLATFORMBUS(drv) {
		ret = rte_platform_probe_one_driver(drv, dev);
		if (ret < 0)
			/* negative value is an error */
			return ret;
		if (ret > 0)
			/* positive value means driver doesn't support it */
			continue;
		return 0;
	}
	return 1;
}

/*
 * Scan the content of the platform bus, and call the probe() function for
 * all registered drivers that have a matching entry in its id_table
 * for discovered devices.
 */
static int
platform_probe(void)
{
	struct rte_platform_device *dev = NULL;
	size_t probed = 0, failed = 0;
	int ret = 0;

	FOREACH_DEVICE_ON_PLATFORMBUS(dev) {
		probed++;

		ret = platform_probe_all_drivers(dev);
		if (ret < 0) {
			if (ret != -EEXIST) {
				RTE_LOG(ERR, EAL, "Requested device %s cannot be used\n",
					dev->addr.dts_path);
				rte_errno = errno;
				failed++;
			}
			ret = 0;
		}
	}

	return (probed && probed == failed) ? -1 : 0;
}

/* register a driver */
void
rte_platform_register(struct rte_platform_driver *drv)
{
	TAILQ_INSERT_TAIL(&rte_platform_bus.driver_list, drv, next);
	drv->bus = &rte_platform_bus;
}

/* unregister a driver */
void
rte_platform_unregister(struct rte_platform_driver *drv)
{
	TAILQ_REMOVE(&rte_platform_bus.driver_list, drv, next);
	drv->bus = NULL;
}

/* Add a device to platform bus */
void
rte_platform_add_device(struct rte_platform_device *dev)
{
	TAILQ_INSERT_TAIL(&rte_platform_bus.device_list, dev, next);
}

/* Insert a device into a predefined position in platform bus */
void
rte_platform_insert_device(struct rte_platform_device *exist_platform_dev,
		      struct rte_platform_device *new_platform_dev)
{
	TAILQ_INSERT_BEFORE(exist_platform_dev, new_platform_dev, next);
}

static struct rte_device *
platform_find_device(const struct rte_device *start, rte_dev_cmp_t cmp,
		const void *data)
{
	const struct rte_platform_device *pstart;
	struct rte_platform_device *pdev;

	if (start != NULL) {
		pstart = RTE_DEV_TO_PLATFORM_CONST(start);
		pdev = TAILQ_NEXT(pstart, next);
	} else {
		pdev = TAILQ_FIRST(&rte_platform_bus.device_list);
	}
	while (pdev != NULL) {
		if (cmp(&pdev->device, data) == 0)
			return &pdev->device;
		pdev = TAILQ_NEXT(pdev, next);
	}
	return NULL;
}

struct rte_platform_bus rte_platform_bus = {
	.bus = {
		.scan = rte_platform_scan,
		.probe = platform_probe,
		.find_device = platform_find_device,
	},
	.device_list = TAILQ_HEAD_INITIALIZER(rte_platform_bus.device_list),
	.driver_list = TAILQ_HEAD_INITIALIZER(rte_platform_bus.driver_list),
};

RTE_REGISTER_BUS(platform, rte_platform_bus.bus);
