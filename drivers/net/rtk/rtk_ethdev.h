/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Realtek Semiconductor Corp.
 */

#ifndef _RTK_ETHDEV_H_
#define _RTK_ETHDEV_H_

struct rtk_hw {

};

uint16_t rtk_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			   uint16_t nb_pkts);
uint16_t rtk_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			   uint16_t nb_pkts);
uint16_t rtk_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts);

#endif /* _RTK_ETHDEV_H_ */
