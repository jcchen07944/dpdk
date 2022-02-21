/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Realtek Semiconductor Corp.
 */

#include <string.h>
#include <dirent.h>

#include <rte_log.h>
#include <rte_bus.h>
#include <rte_platform.h>
#include <rte_bus_platform.h>
#include <rte_malloc.h>
#include <rte_devargs.h>
#include <rte_memcpy.h>

#include "eal_filesystem.h"

#include "private.h"

/**
 * @file
 * Platform device probing using Linux sysfs.
 */

#define UEVENT_VALUE_LEN_MAX 100

extern struct rte_platform_bus rte_platform_bus;

int
rte_platform_map_device(struct rte_platform_device *dev)
{
	switch (dev->kdrv) {
	case RTE_PLATFORM_KDRV_UIO_GENERIC:
		if (rte_eal_using_phys_addrs())
			return platform_uio_map_resource(dev);
		break;
	default:
		RTE_LOG(DEBUG, EAL,
			"  Not managed by a supported kernel driver, skipped\n");
		return 1;
	}

	return -1;
}

int
uevent_find_entry(const char *filename, const char *key, char value[])
{
	FILE *f;
	size_t len = 0;
	char *line = NULL, *v = NULL;
	bool found = false;

	if ((f = fopen(filename, "r")) == NULL) {
		RTE_LOG(ERR, EAL, "%s(): cannot open uevent file %s\n",
			__func__, filename);
		goto fopen_error;
	}

	while (getline(&line, &len, f) != -1) {
		if (strstr(line, key) != NULL) {
			line[strlen(line)-1] = '\0'; /* Skip end-of-line */
			found = true;
			break;
		}
	}

	if (!found) {
		RTE_LOG(ERR, EAL, "%s(): cannot parse \"%s\" in %s\n",
			__func__, key, filename);
		goto parse_error;
	}

	strtok(line, "=");
	v = strtok(NULL, "=");

	if (strlen(v) >= UEVENT_VALUE_LEN_MAX)
		goto value_len_error;

	strcpy(value, v);

	fclose(f);
	return 0;

value_len_error:
	RTE_LOG(ERR, EAL, "%s(): value of %s is too long\n", __func__, key);
parse_error:
	fclose(f);
fopen_error:
	return -1;
}

static uint32_t
cell_to_int(const unsigned char cell[])
{
	int num = 0;
	for (int i = 0; i < 4; i++)
		num += (cell[i] << (sizeof(char) * 8 * (3-i)));
	return num;
}

static int
platform_parse_memory(struct rte_platform_device *dev, char *filename, 
						uint32_t address_cells, uint32_t size_cells)
{
	FILE *file;
	uint32_t i, j;
	unsigned char cell[4];

	if (address_cells > 2 || size_cells > 2) {
		RTE_LOG(ERR, EAL, "%s(): #address_cells or #size_cells is too \
					large\n", __func__);
		return -1;
	}
	file = fopen(filename, "rt");
	if (!file) {
		RTE_LOG(ERR, EAL, "%s(): Cannot open %s\n", __func__,
					filename);
		return -1;
	}

	for (i = 0; i < PLATFORM_MAX_RESOURCE; i++) {
		dev->mem_resource[i].phys_addr = 0;
		dev->mem_resource[i].len = 0;
		dev->mem_resource[i].addr = NULL;
		for (j = 0; j < address_cells; j++) {
			memset(cell, 0, sizeof(cell));
			if (fread(&cell, 1, 4, file) != 4)
				goto out;
			dev->mem_resource[i].phys_addr += cell_to_int(cell) <<
											(32 * (address_cells - j - 1));
			RTE_LOG(DEBUG, EAL, "%s:%d phy_addr = %lx\n", __func__, __LINE__, dev->mem_resource[i].phys_addr);
		}
		for (j = 0; j < size_cells; j++) {
			memset(cell, 0, sizeof(cell));
			if (fread(&cell, 1, 4, file) != 4)
				goto out;
			dev->mem_resource[i].len += cell_to_int(cell) <<
											(32 * (size_cells - j - 1));
			RTE_LOG(DEBUG, EAL, "%s:%d len = %lx\n", __func__, __LINE__, dev->mem_resource[i].len);

		}
	}

out:
	fclose(file);
	return 0;
}

static int
platform_parse_dts_resource(struct rte_platform_device *dev)
{
	FILE *file;
	const char *dts_base = "/sys/firmware/devicetree/base";
	char dirname[PATH_MAX], filename[PATH_MAX], dts_temp[PATH_MAX];
	char *token;
	unsigned char cell[4];
	uint32_t address_cells = 1, size_cells = 1;

	/* Parsing #address_cells and #size_cells */
	strcpy(dts_temp, dev->addr.dts_path);
	token = strtok(dts_temp, "/");
	while (token != NULL) {
		snprintf(dirname, sizeof(dirname), "%s/%s", dts_base, token);

		snprintf(filename, sizeof(filename), "%s/#address_cells", dirname);
		file = fopen(filename, "rt");
		if (file) {
			memset(cell, 0, sizeof(cell));
			fread(&cell, 1, 4, file);
			address_cells = cell_to_int(cell);
			fclose(file);
		}

		snprintf(filename, sizeof(filename), "%s/#size_cells", dirname);
		file = fopen(filename, "rt");
		if (file) {
			memset(cell, 0, sizeof(cell));
			fread(&cell, 1, 4, file);
			address_cells = cell_to_int(cell);
			fclose(file);
		}

		token = strtok(NULL, "/");
	}

	snprintf(filename, sizeof(filename), "%s%s/reg", dts_base,
				dev->addr.dts_path);
	platform_parse_memory(dev, filename, address_cells, size_cells);

	return 0;
}

static void
platform_scan_one(const char *dirname)
{
	char filename[PATH_MAX];
	char value[UEVENT_VALUE_LEN_MAX];
	struct rte_platform_device *dev;
	int compatible_count;

	dev = malloc(sizeof(*dev));
	if (dev == NULL)
		goto malloc_error;

	memset(dev, 0, sizeof(*dev));
	dev->device.bus = &rte_platform_bus.bus;
	strcpy(dev->dev_path, dirname);

	snprintf(filename, sizeof(filename), "%s/uevent", dirname);

	memset(value, 0, sizeof(value));
	if (uevent_find_entry(filename, "OF_COMPATIBLE_N", value))
		goto parse_error;
	compatible_count = atoi(value);
	if (compatible_count <= 0 || compatible_count > PLATFORM_MAX_COMPATIBLE)
		goto parse_error;

	memset(value, 0, sizeof(value));
	if (uevent_find_entry(filename, "OF_FULLNAME", value))
		goto parse_error;
	strcpy(dev->addr.dts_path, value);

	platform_name_set(dev);

	platform_parse_dts_resource(dev);

	for (uint32_t i = 0; i < (uint32_t)compatible_count; i++) {
		char key[strlen("OF_COMPATIBLE_=") + 33];

		snprintf(key, sizeof(key), "OF_COMPATIBLE_%u=", i);
		memset(value, 0, sizeof(value));
		if (uevent_find_entry(filename, key, value))
			continue;
		strcpy(dev->id[i].compatible, value);
		RTE_LOG(DEBUG, EAL, "%s(): compatible%d = %s\n", __func__, i,
					dev->id[i].compatible);
	}

	dev->driver = NULL;
	memset(value, 0, sizeof(value));
	uevent_find_entry(filename, "DRIVER", value);
	if (!strcmp(value, "uio_pdrv_genirq"))
		dev->kdrv = RTE_PLATFORM_KDRV_UIO_GENERIC;
	else
		dev->kdrv = RTE_PLATFORM_KDRV_UNKNOWN;

	if (!TAILQ_EMPTY(&rte_platform_bus.device_list)) {
		struct rte_platform_device *dev2;
		int ret;

		TAILQ_FOREACH(dev2, &rte_platform_bus.device_list, next) {
			ret = rte_platform_addr_cmp(&dev->addr, &dev2->addr);
			if (ret > 0)
				continue;

			if (ret < 0) {
				rte_platform_insert_device(dev2, dev);
			} else { /* already registered */
				dev2->kdrv = dev->kdrv;
				for (int i = 0; i < PLATFORM_MAX_COMPATIBLE; i++)
					dev2->id[i] = dev->id[i];
				platform_name_set(dev2);
				memmove(dev2->mem_resource,
						dev->mem_resource,
						sizeof(dev->mem_resource));
				free(dev);
			}
			return;
		}
	}

	rte_platform_add_device(dev);

	return;

parse_error:
	free(dev);
malloc_error:
	return;
}

/*
 * Scan the content of the platform bus, and the devices in the devices
 * list
 */
int
rte_platform_scan(void)
{
	struct dirent *e;
	DIR *dir;
	char dirname[PATH_MAX];

	/* [TODO] for debug purposes, platform can be disabled */
	// if (!rte_eal_has_platform())
	// 	return 0;

	dir = opendir(rte_platform_get_sysfs_path());
	if (dir == NULL) {
		RTE_LOG(ERR, EAL, "%s(): opendir failed: %s\n",
			__func__, strerror(errno));
		return -1;
	}

	while ((e = readdir(dir)) != NULL) {
		if (e->d_name[0] == '.')
			continue;

		snprintf(dirname, sizeof(dirname), "%s/%s", 
						rte_platform_get_sysfs_path(), e->d_name);

		RTE_LOG(DEBUG, EAL, "%s:%d %s\n", __func__, __LINE__, dirname);
		platform_scan_one(dirname);
	}
	closedir(dir);
	return 0;
}