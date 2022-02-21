/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Realtek Semiconductor Corp.
 */

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>

#include <rte_string_fns.h>
#include <rte_log.h>
#include <rte_platform.h>
#include <rte_bus_platform.h>
#include <rte_common.h>
#include <rte_malloc.h>

#include "eal_filesystem.h"
#include "private.h"

void *platform_map_addr = NULL;

/*
 * Return the uioX char device used for a platform device. On success, return
 * the UIO number and fill dstbuf string with the path of the device in sysfs.
 * On error, return a negative value. In this case dstbuf is invalid.
 */
static int
platform_get_uio_dev(struct rte_platform_device *dev, char uio_path[])
{
	int uio_num = -1;
	struct dirent *e;
	DIR *dir;
	char dirname[PATH_MAX];

	/* depending on kernel version, uio can be located in uio/uioX
	 * or uio:uioX */
	strcpy(dirname, dev->dev_path);
	strcat(dirname, "/uio");
	dir = opendir(dirname);
	if (dir == NULL) {
		RTE_LOG(ERR, EAL, "Cannot opendir %s\n", dirname);
		return -1;
	}

	/* take the first file starting with "uio" */
	while ((e = readdir(dir)) != NULL) {
		/* format could be uio%d ...*/
		int shortprefix_len = sizeof("uio") - 1;
		/* ... or uio:uio%d */
		int longprefix_len = sizeof("uio:uio") - 1;
		char *endptr;

		if (strncmp(e->d_name, "uio", 3) != 0)
			continue;

		/* first try uio%d */
		errno = 0;
		uio_num = strtoull(e->d_name + shortprefix_len, &endptr, 10);
		if (errno == 0 && endptr != (e->d_name + shortprefix_len)) {
			snprintf(uio_path, PATH_MAX, "%s/uio%u", dirname,
						uio_num);
			break;
		}

		/* then try uio:uio%d */
		errno = 0;
		uio_num = strtoull(e->d_name + longprefix_len, &endptr, 10);
		if (errno == 0 && endptr != (e->d_name + longprefix_len)) {
			snprintf(uio_path, PATH_MAX, "%s/uio:uio%u",
						dirname, uio_num);
			break;
		}
	}
	closedir(dir);

	/* No uio resource found */
	if (e == NULL)
		return -1;

	return uio_num;
}

void
platform_uio_free_resource(struct rte_platform_device *dev,
		struct mapped_platform_resource *uio_res)
{
	int uio_cfg_fd = rte_intr_dev_fd_get(dev->intr_handle);

	rte_free(uio_res);

	if (uio_cfg_fd >= 0) {
		close(uio_cfg_fd);
		rte_intr_dev_fd_set(dev->intr_handle, -1);
	}

	if (rte_intr_fd_get(dev->intr_handle) >= 0) {
		close(rte_intr_fd_get(dev->intr_handle));
		rte_intr_fd_set(dev->intr_handle, -1);
		rte_intr_type_set(dev->intr_handle, RTE_INTR_HANDLE_UNKNOWN);
	}
}

int
platform_uio_alloc_resource(struct rte_platform_device *dev,
		struct mapped_platform_resource **uio_res)
{
	char uio_path[PATH_MAX];
	char devname[PATH_MAX]; /* contains the /dev/uioX */
	int uio_num, fd;

	/* find uio resource */
	uio_num = platform_get_uio_dev(dev, uio_path);
	if (uio_num < 0) {
		RTE_LOG(WARNING, EAL, "  %s is not managed by UIO driver, skipping\n",
					dev->name);
		return 1;
	}
	snprintf(devname, sizeof(devname), "/dev/uio%u", uio_num);

	/* save fd if in primary process */
	fd = open(devname, O_RDWR);
	if (fd < 0) {
		RTE_LOG(ERR, EAL, "Cannot open %s: %s\n",
			devname, strerror(errno));
		goto error;
	}

	if (rte_intr_fd_set(dev->intr_handle, fd))
		goto error;

	/* allocate the mapping details for secondary processes */
	*uio_res = rte_zmalloc("UIO_RES", sizeof(**uio_res), 0);
	if (*uio_res == NULL) {
		RTE_LOG(ERR, EAL,
			"%s(): cannot store uio mmap details\n", __func__);
		goto error;
	}

	strlcpy((*uio_res)->path, devname, sizeof((*uio_res)->path));
	memcpy(&(*uio_res)->platform_addr, &dev->addr,
			sizeof((*uio_res)->platform_addr));

	return 0;

error:
	platform_uio_free_resource(dev, *uio_res);
	return -1;
}


int
find_max_end_va(const struct rte_memseg_list *msl, void *arg)
{
	size_t sz = msl->len;
	void *end_va = RTE_PTR_ADD(msl->base_va, sz);
	void **max_va = arg;

	if (*max_va < end_va)
		*max_va = end_va;
	return 0;
}

void *
platform_find_max_end_va(void)
{
	void *va = NULL;

	rte_memseg_list_walk(find_max_end_va, &va);
	return va;
}

int
platform_uio_map_resource_by_index(struct rte_platform_device *dev,
		int res_idx, struct mapped_platform_resource *uio_res, int map_idx)
{
	int fd;
	void *mapaddr;
	struct platform_map *maps;

	maps = uio_res->maps;

	RTE_LOG(DEBUG, EAL, "%s:%d %s\n", __func__, __LINE__, uio_res->path);

	fd = open(uio_res->path, O_RDWR);
	if (fd < 0) {
		RTE_LOG(ERR, EAL, "Cannot open %s: %s", uio_res->path, strerror(errno));
		return -1;
	}

	/* try mapping somewhere close to the end of hugepages */
	if (platform_map_addr == NULL)
		platform_map_addr = platform_find_max_end_va();

	/* offset is special in uio it indicates which resource */
	// offset = idx * rte_mem_page_size();

	mapaddr = platform_map_resource(platform_map_addr, fd, 0,
			(size_t)dev->mem_resource[res_idx].len, 0);
	close(fd);
	if (mapaddr == NULL)
		return -1;

	platform_map_addr = RTE_PTR_ADD(mapaddr,
			(size_t)dev->mem_resource[res_idx].len);

	platform_map_addr = RTE_PTR_ALIGN(platform_map_addr,
			sysconf(_SC_PAGE_SIZE));

	maps[map_idx].phaddr = dev->mem_resource[res_idx].phys_addr;
	maps[map_idx].size = dev->mem_resource[res_idx].len;
	maps[map_idx].addr = mapaddr;
	dev->mem_resource[res_idx].addr = mapaddr;

	return 0;
}
