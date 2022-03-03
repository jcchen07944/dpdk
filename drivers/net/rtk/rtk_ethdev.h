/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Realtek Semiconductor Corp.
 */

#ifndef _RTK_ETHDEV_H_
#define _RTK_ETHDEV_H_

#include <rte_byteorder.h>
#include <rte_io.h>

struct rtk_hw {
	uint8_t *hw_addr;
};

// #define RTK_WRITE_FLUSH(a) RTK_READ_REG(a, RTK_STATUS)

#define RTK_PLATFORM_REG(reg)	rte_read32(reg)

#define RTK_PLATFORM_REG16(reg)	rte_read16(reg)

#define RTK_PLATFORM_REG_WRITE(reg, value)			\
	rte_write32((rte_cpu_to_le_32(value)), reg)

#define RTK_PLATFORM_REG_WRITE_RELAXED(reg, value)		\
	rte_write32_relaxed((rte_cpu_to_le_32(value)), reg)

#define RTK_PLATFORM_REG_WRITE16(reg, value)		\
	rte_write16((rte_cpu_to_le_16(value)), reg)

#define RTK_PLATFORM_REG_ADDR(hw, reg) \
	((volatile uint32_t *)((char *)(hw)->hw_addr + (reg)))

#define RTK_PLATFORM_REG_ARRAY_ADDR(hw, reg, index) \
	RTK_PLATFORM_REG_ADDR((hw), (reg) + ((index) << 2))

static inline uint32_t rtk_read_addr(volatile void *addr)
{
	return rte_le_to_cpu_32(RTK_PLATFORM_REG(addr));
}

static inline uint16_t rtk_read_addr16(volatile void *addr)
{
	return rte_le_to_cpu_16(RTK_PLATFORM_REG16(addr));
}

#define RTK_READ_REG(hw, reg) \
	rtk_read_addr(RTK_PLATFORM_REG_ADDR((hw), (reg)))

#define RTK_WRITE_REG(hw, reg, value) \
	RTK_PLATFORM_REG_WRITE(RTK_PLATFORM_REG_ADDR((hw), (reg)), (value))

#define RTK_READ_REG_ARRAY(hw, reg, index) \
	RTK_PLATFORM_REG(RTK_PLATFORM_REG_ARRAY_ADDR((hw), (reg), (index)))

#define RTK_WRITE_REG_ARRAY(hw, reg, index, value) \
	RTK_PLATFORM_REG_WRITE(RTK_PLATFORM_REG_ARRAY_ADDR((hw), (reg), (index)), (value))

#define RTK_READ_REG_ARRAY_DWORD RTK_READ_REG_ARRAY
#define RTK_WRITE_REG_ARRAY_DWORD RTK_WRITE_REG_ARRAY

#define	RTK_ACCESS_PANIC(x, hw, reg, value) \
	rte_panic("%s:%u\t" RTE_STR(x) "(%p, 0x%x, 0x%x)", \
		__FILE__, __LINE__, (hw), (reg), (unsigned int)(value))


uint16_t rtk_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			   uint16_t nb_pkts);
uint16_t rtk_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			   uint16_t nb_pkts);
uint16_t rtk_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts);

#endif /* _RTK_ETHDEV_H_ */
