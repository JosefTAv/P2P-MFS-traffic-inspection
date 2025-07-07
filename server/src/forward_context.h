#pragma once

#define MAX_Q_PER_FORWARDER (3)

#include <rte_atomic.h>

// Custom headers
#include "onic_helper.h"
#include "onic_port.h"
#include "onic.h"

struct ForwardingContext {
    const Onic* rx_onic;
    const Onic* tx_onic;
	int rx_port;
	int tx_port;

    std::array<int, MAX_Q_PER_FORWARDER> rx_Qs = {-1};
    std::array<int, MAX_Q_PER_FORWARDER> tx_Qs = {-1};
    int nb_rx_Qs;
    int nb_tx_Qs;

    struct rte_ring *stats_ring;
    struct rte_eth_stats stats;

    rte_atomic32_t stop_flag;

    void print_schema() {
        std::cout   << "Forwarding " << to_string(rx_onic->get_ports()[rx_port].get_bdf())
                    << " --------> " << to_string(tx_onic->get_ports()[tx_port].get_bdf())
                    << std::endl
                    << "rx_port " << rx_port << " --------> " << "tx_port " << tx_port 
                    << std::endl;
    }
};