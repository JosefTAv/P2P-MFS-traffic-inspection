#pragma once

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <librdkafka/rdkafka.h>
#include "onic.h"
#include "forward_context.h"
#include "pipeline.h"
#include <rte_metrics.h>

#define BURST_SIZE 32
#define NUM_MBUFS 4096
#define MBUF_CACHE_SIZE 250

// Timestamp packet constants: unit = Bytes
#define TIMESTAMP_OFFSET (6+6+2+2+2) //dst MAC + src MAC + TPID + VLAN + Ethertype 
#define ETHERTYPE_LEN (2)
#define NB_SYNC_LEN (8) 
#define CURR_TICK_LEN (8)
#define TIMESTAMP_PKT_SIZE (60)

#define NB_HOPS (3)

#define LEN_NB_SYNC_NS (1000) // period of one nb_sync packet
#define LEN_TIME_TICK_NS (4) // period of one tick at 250Mhz

#define CUSTOM_VLAN_ID (0x0ABC)

// Kafka constants
#define STATS_TABLE_NAME ("Forwarder_stats")
#define LATENCY_TABLE_NAME ("Latency_stats")
#define KAFKA_HEADER_LEN (1024)
#define KAFKA_STATS_LEN (1024)

struct ForwardStats {
    int rx_port_id;
    int tx_port_id;

    struct rte_mbuf mbuf_latency;
};

struct Timestamps {
    uint64_t timestamp_nb_sync[NB_HOPS];
    uint64_t timestamp_curr_tick[NB_HOPS];

    Timestamps(rte_mbuf *mbuf){
        
        char *pkt_data = rte_pktmbuf_append(mbuf, TIMESTAMP_PKT_SIZE);
        struct rte_ether_hdr *eth = (struct rte_ether_hdr *)pkt_data;
        
        // check the packet is proper VLAN: redundant??
        assert(0x8100 == rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN)); // VLAN tag
        struct rte_vlan_hdr *vlan = (struct rte_vlan_hdr *)(eth + 1); // VLAN header is after Eth header
        assert(CUSTOM_VLAN_ID == rte_cpu_to_be_16(CUSTOM_VLAN_ID)); // VLAN ID 0xABC
        
        // Extract timestamps
        uint8_t *payload = rte_pktmbuf_mtod_offset(mbuf, uint8_t *, TIMESTAMP_OFFSET);
        for(int i=0; i<NB_HOPS; i++){
            memcpy(&timestamp_nb_sync[i], payload, sizeof(uint64_t));
            payload += NB_SYNC_LEN;
            memcpy(&timestamp_nb_sync[i], payload, sizeof(uint64_t));
            payload += CURR_TICK_LEN;
        }
    }

    std::array<uint64_t, NB_HOPS - 1>  calc_hop_latencies() const { // return latency between each hop in ns
        std::array<uint64_t, NB_HOPS - 1> latencies{};

        for(int i=0; i<NB_HOPS-1; i++){
            latencies[i] = 
                (timestamp_nb_sync[i]-timestamp_nb_sync[i+1])*LEN_NB_SYNC_NS + 
                (timestamp_curr_tick[i]-timestamp_curr_tick[i+1])*LEN_TIME_TICK_NS;
        }
        return latencies;
    }

    std::string get_latency_str() const {
        std::array<uint64_t, NB_HOPS - 1> latencies = calc_hop_latencies();
        std::ostringstream oss;
        for(int i=0; i<NB_HOPS-1; i++){
            oss << "hop(" << i << ")=" << latencies[i] << ",";
        }
    
        std::string result = oss.str();
        if (!result.empty()) 
            result.pop_back();  // Remove trailing comma
        return result;
    }
};

struct ForwardingContext;

class StatsLog {
private:
    int nb_Qs;
    rd_kafka_t *rk;
    rd_kafka_topic_t *rkt;
    const char *topic;
    const ForwardingContext *ctx;
    ForwardStats *forward_stats;
    struct rte_eth_stats rx_stats;
    struct rte_eth_stats tx_stats;

    uint64_t hz = rte_get_tsc_hz();  // cycles per second
    uint64_t old_time = 0;
    uint64_t curr_time = 0;

    void extract_then_produce_latency_packets(const ForwardingContext *ctx);
    void produce_latency_message(std::string latency);
public:
    StatsLog(const ForwardingContext *ctx, const char *topic) : ctx(ctx), topic(topic) {
        create_kafka_topic(topic);

        // Check how many Qs will be logged
        auto it_rx = std::find(ctx->rx_Qs.begin(), ctx->rx_Qs.end(), -1);
        auto it_tx = std::find(ctx->tx_Qs.begin(), ctx->tx_Qs.end(), -1);
        int nb_rx_Qs = (it_rx != ctx->rx_Qs.end()) ? std::distance(ctx->rx_Qs.begin(), it_rx) : ctx->rx_Qs.size();
        int nb_tx_Qs = (it_tx != ctx->tx_Qs.end()) ? std::distance(ctx->tx_Qs.begin(), it_tx) : ctx->tx_Qs.size();
        nb_Qs = std::min(nb_rx_Qs, nb_tx_Qs);
    };
    ~StatsLog() { cleanup_kafka(); };

    int run_stats_producer();
    int run_cmac_producer();

    static std::string rte_stats_to_string(const struct rte_eth_stats &stats, std::string direction);
    void create_kafka_topic(const char *topic);
    void produce_kafka_message(std::string payload, const char *key=NULL, size_t key_len=0);
    void cleanup_kafka();

    static inline uint64_t get_tsc_delta_ns(uint64_t old_time, uint64_t curr_time, uint64_t hz){
        return ((curr_time - old_time) * (uint64_t)1e9) / hz;
    }
};

extern "C" {
    int StatsLog_run_producer(void *statslog);
    int StatsLog_cmac_producer(void *statslog);
}
