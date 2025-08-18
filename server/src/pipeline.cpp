#include "pipeline.h"

#define BURST_SIZE (32)

int fpga_rx_thread(void *arg){
	auto *ctx = (struct ForwardingContext *)arg;
    RTE_LOG(INFO, USER1, "CTX(%u) Software forwarder Rx started on lcore %u\nmbuf addr: %p\n", ctx->ctx_id, rte_lcore_id(), ctx->rx_onic->mbuf_ring);

    struct rte_mbuf *mbufs[BURST_SIZE];

    // convert FPGA port to DPDK port id
    uint16_t rx_port_id = (ctx->rx_onic->get_ports()[ctx->rx_port]).get_port_id();
    uint16_t tx_port_id = (ctx->tx_onic->get_ports()[ctx->tx_port]).get_port_id();
    // track current Q
    uint16_t curr_Q = 0;

    RTE_LOG(INFO, USER1, "rx_port_id=%u\n", rx_port_id);
    struct rte_eth_dev_info di;
    rte_eth_dev_info_get(rx_port_id, &di);
    RTE_LOG(INFO, USER1, "driver=%s nb_rx_queues=%u\n",
            di.driver_name ? di.driver_name : "?", di.nb_rx_queues);

    while (!rte_atomic32_read(&ctx->stop_flag)) {

        // Check ring availability/alloc memory : continue, wait or exit??
        // ret = rte_mempool_get(ctx->stats_mp, (void **)&port_stats);
        // if (ret < 0) {
        //     rte_exit(EXIT_FAILURE, "Failed to allocate memory for forward_stats\n");
        // }

        // Receive packets
        uint16_t next_Q_idx = curr_Q % ctx->nb_rx_Qs; //round-robin Q select
        int32_t nb_rx = rte_eth_rx_burst(rx_port_id, 0, mbufs, BURST_SIZE);
        // rte_pmd_qdma_qstats(rx_port_id, next_Q);
        if (unlikely(nb_rx == 0)) {
            rte_pause();
            // printf("rx(%u) Q(%u) tx(%u)\n", rx_port_id, ctx->rx_Qs[next_Q_idx], tx_port_id);
            continue;
        }
        // rte_pmd_qdma_qstats(rx_port_id, ctx->rx_Qs[next_Q_idx]);
        // RTE_LOG(INFO, USER1, "CTX(%u) %d packets received on Q %d\n", ctx->ctx_id, nb_rx, ctx->rx_Qs[next_Q_idx]);
        curr_Q++;

        // Enqueue mbufs for tx
        if (rte_ring_enqueue_bulk(ctx->rx_onic->mbuf_ring, (void *const *)mbufs, nb_rx, NULL) != 0) {
            // Ring is full — handle overflow
            for (uint16_t i = 0; i < nb_rx; i++) {
                rte_pktmbuf_free(mbufs[i]);
            }
        }

    }
    RTE_LOG(INFO, USER1, "CTX(%u) Software forwarder Rx stopped on lcore %u\n", ctx->ctx_id, rte_lcore_id());
    return 0;
}

int fpga_rx_final_thread(void *arg){
    auto *ctx = (struct ForwardingContext *)arg;
    RTE_LOG(INFO, USER1, "CTX(%u) Software forwarder final Tx started on lcore %u\nmbuf addr: %p\n", ctx->ctx_id, rte_lcore_id(), ctx->rx_onic->mbuf_ring);

    struct rte_mbuf *mbufs[BURST_SIZE];
    uint16_t nb_rx = 0;
    uint16_t curr_Q = 0;
    // convert FPGA port to DPDK port id
    uint16_t rx_port_id = (ctx->rx_onic->get_ports()[ctx->rx_port]).get_port_id();
    uint16_t tx_port_id = (ctx->tx_onic->get_ports()[ctx->tx_port]).get_port_id();


    while (!rte_atomic32_read(&ctx->stop_flag)) {

        // Check ring availability/alloc memory : continue, wait or exit??
        // ret = rte_mempool_get(ctx->stats_mp, (void **)&port_stats);
        // if (ret < 0) {
        //     rte_exit(EXIT_FAILURE, "Failed to allocate memory for forward_stats\n");
        // }

        // Receive packets
        uint16_t next_Q = curr_Q % ctx->nb_rx_Qs; //round-robin Q select
        nb_rx = rte_eth_rx_burst(rx_port_id, ctx->rx_Qs[next_Q], mbufs, BURST_SIZE);
        if (unlikely(nb_rx == 0)) {
            rte_pause();
            continue;
        }
        curr_Q++;

        // Search for timestamp
        for(int i=0; i<nb_rx; i++){
            // Enqueue mbuf containing timestamps
            if (!is_vlan_packet(mbufs[i])){
                rte_pktmbuf_free(mbufs[i]); // Free useless packet
                continue;
            }

            // Pass stats packet to stats corex
            if (rte_ring_enqueue(ctx->stats_ring, mbufs[i]) != 0) {
                printf("Stats ring is full, unable to enqueue packet");
            }
            rte_pktmbuf_free(mbufs[i]); // stats_ring is full, must manually free
        }
    }
    return 0;
}

bool is_vlan_packet(struct rte_mbuf *mbuf) {
    struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);

    if (rte_be_to_cpu_16(eth_hdr->ether_type) == RTE_ETHER_TYPE_VLAN) {
        struct rte_vlan_hdr *vlan_hdr = (struct rte_vlan_hdr *)(eth_hdr + 1);
        uint16_t inner_type = rte_be_to_cpu_16(vlan_hdr->eth_proto);

        printf("VLAN Tag: 0x%04x, Inner Ethertype: 0x%04x\n",
               rte_be_to_cpu_16(vlan_hdr->vlan_tci), inner_type);

        return true;
    }

    return false;
}


int fpga_tx_thread(void *arg){
	auto *ctx = (struct ForwardingContext *)arg;
    RTE_LOG(INFO, USER1, "CTX(%u) Software forwarder Tx thread started on lcore %u\nmbuf addr: %p\n", ctx->ctx_id, rte_lcore_id(), ctx->rx_onic->mbuf_ring);

    struct rte_mbuf *mbufs[BURST_SIZE];
    unsigned int nb_rx = 0;
    uint16_t curr_Q = 0;

    // convert FPGA port to DPDK port id
    uint16_t tx_port_id = (ctx->tx_onic->get_ports()[ctx->tx_port]).get_port_id();

    while (!rte_atomic32_read(&ctx->stop_flag)) {

        // Check ring for new packets
        nb_rx = rte_ring_dequeue_burst(ctx->rx_onic->mbuf_ring, (void **)mbufs, BURST_SIZE, NULL);
        if (unlikely(nb_rx == 0)) {
            rte_pause();
            continue;
        }
        // printf("Sending 
        // Transmit packets
        // rte_pktmbuf_dump(stdout, mbufs[0], mbufs[0]->pkt_len);
        struct rte_mempool *mp = rte_mempool_lookup(ctx->tx_onic->get_ports()[ctx->tx_port].pinfo.mem_pool);
        printf("dst mempool available: %u\n", rte_mempool_avail_count(mp));
        uint16_t next_Q_idx = curr_Q % ctx->nb_tx_Qs; //round-robin Q select
        int32_t nb_tx = rte_eth_tx_burst(tx_port_id, ctx->tx_Qs[next_Q_idx], mbufs, nb_rx);
        curr_Q++;

        // Free any untransmitted packets
        if (unlikely(nb_tx < nb_rx)) {
            for (uint16_t i = nb_tx; i < nb_rx; i++) {
                rte_pktmbuf_free(mbufs[i]);
            }
            printf("Not sent %u packets on tx_port(%u) on Q(%u)\n", nb_rx, tx_port_id, ctx->tx_Qs[0]);
        }
        
    }
    return 0;
}

// int software_forwarder_thread(void *arg){
// 	auto *ctx = (struct ForwardingContext *)arg;
//     RTE_LOG(INFO, USER1, "Software forwarder started on lcore %u\n", rte_lcore_id());

//     struct rte_mbuf *mbufs[BURST_SIZE];
//     // struct rte_eth_stats *port_stats;
//     // int ret;

//     // convert FPGA port to DPDK port id
//     uint16_t rx_port_id = (ctx->rx_onic->get_ports()[ctx->rx_port]).get_port_id();
//     uint16_t tx_port_id = (ctx->tx_onic->get_ports()[ctx->tx_port]).get_port_id();

//     while (!rte_atomic32_read(&ctx->stop_flag)) {

//         // Check ring availability/alloc memory : continue, wait or exit??
//         // ret = rte_mempool_get(ctx->stats_mp, (void **)&port_stats);
//         // if (ret < 0) {
//         //     rte_exit(EXIT_FAILURE, "Failed to allocate memory for forward_stats\n");
//         // }

//         // Receive packets
//         int32_t nb_rx = rte_eth_rx_burst(rx_port_id, ctx->rx_Qs[0], mbufs, BURST_SIZE);
//         if (unlikely(nb_rx == 0)) {
//             rte_pause();
//             continue;
//         }

//         // Transmit packets
//         int32_t nb_tx = rte_eth_tx_burst(tx_port_id, ctx->tx_Qs[0], mbufs, nb_rx);
//         rte_atomic32_add(&ctx->rx_count, nb_rx);
//         rte_atomic32_add(&ctx->tx_count, nb_tx);
//         // // Send rx stats
//         // int ret = rte_eth_stats_get(rx_port, port_stats);
//         // if (ret) {
//         //     std::cerr << "Cannot get statistics for rx port " << rx_port << std::endl;
//         // }
//         // ret = rte_ring_enqueue(ctx->stats_ring, forward_stats);
//         // if (ret != 0) {
//         //     rte_exit(EXIT_FAILURE, "Failed to enqueue data onto the ring\n");
//         // }

//         // // Send tx stats
//         // ret = rte_eth_stats_get(tx_port, port_stats);
//         // if (ret) {
//         //     std::cerr << "Cannot get statistics for rx port " << rx_port << std::endl;
//         // }
//         // ret = rte_ring_enqueue(ctx->stats_ring, forward_stats);
//         // if (ret != 0) {
//         //     rte_exit(EXIT_FAILURE, "Failed to enqueue data onto the ring\n");
//         // }

//         // Free any untransmitted packets
//         if (unlikely(nb_tx < nb_rx)) {
//             for (uint16_t i = nb_tx; i < nb_rx; i++) {
//                 rte_pktmbuf_free(mbufs[i]);
//             }
//         }

//     }
//     return 0;
// }