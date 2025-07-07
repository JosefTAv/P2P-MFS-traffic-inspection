#pragma once

#include <rte_ring.h>
#include "forward_context.h"

int fpga_rx_thread(void *arg);

int fpga_rx_final_thread(void *arg);

bool is_vlan_packet(struct rte_mbuf *mbuf);

int fpga_tx_thread(void *arg);

int software_forwarder_thread(void *arg);