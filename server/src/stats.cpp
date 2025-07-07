#include "stats.h"
#include "onic.h"
#include <cstdint>

int StatsLog_run_producer(void *statslog) {
    return static_cast<StatsLog *>(statslog)->run_stats_producer();
}

int StatsLog::run_stats_producer(){
    RTE_LOG(INFO, USER1, "Stats producer started on lcore %u\n", rte_lcore_id());

    // convert FPGA port to DPDK port id
    OnicPort rx_port = (ctx->rx_onic->get_ports()[ctx->rx_port]);
    OnicPort tx_port = (ctx->tx_onic->get_ports()[ctx->tx_port]);

    uint16_t rx_port_id = rx_port.get_port_id();
    uint16_t tx_port_id = tx_port.get_port_id();
    int ret;

    std::ostringstream oss;
    oss << STATS_TABLE_NAME
        << "(ports-" << rte_lcore_id() << ")"
        << ",rx_port(" << rx_port_id << ")=" << to_string(rx_port.get_bdf()) 
        << ",tx_port(" << tx_port_id << ")="<< to_string(tx_port.get_bdf())
        << " ";

    std::string kafka_header = oss.str();
    std::string kafka_data;
    std::string kafka_message;

    kafka_data.reserve(1024);
    kafka_message.reserve(2048);

    while (!rte_atomic32_read(&ctx->stop_flag)) {
        // Get rx stats
        ret = rte_eth_stats_get(rx_port_id, &rx_stats);
        if (ret < 0) {
            printf("Error getting stats for port %u\n", rx_port_id);
            return -1;
        }
        // Get tx stats
        ret = rte_eth_stats_get(tx_port_id, &tx_stats);
        if (ret < 0) {
            printf("Error getting stats for port %u\n", tx_port_id);
            return -1;
        }
        curr_time = rte_get_tsc_cycles();
        uint64_t delta_ns = StatsLog::get_tsc_delta_ns(old_time, curr_time, hz);
        old_time = curr_time;
        kafka_message = kafka_header + \
                        rte_stats_to_string(rx_stats, "(R)") + "," + \
                        rte_stats_to_string(tx_stats, "(T)") + "," + \
                        "DELTA_NS=" + std::to_string(delta_ns) + "\n";
        produce_kafka_message(kafka_message);

        // produce latency stats
        extract_then_produce_latency_packets(ctx);
        
        rte_delay_ms(3000);
    }
    return 0;
}

int StatsLog_cmac_producer(void *statslog) {
    return static_cast<StatsLog *>(statslog)->run_cmac_producer();
}

int StatsLog::run_cmac_producer(){
    int core_id = rte_lcore_id();
    RTE_LOG(INFO, USER1, "CMAC stats producer started on lcore %u\n", core_id);

    // convert FPGA port to DPDK port id
    OnicPort rx_port = (ctx->rx_onic->get_ports()[ctx->rx_port]);
    OnicPort tx_port = (ctx->tx_onic->get_ports()[ctx->tx_port]);
    
    uint16_t rx_port_id = rx_port.get_port_id();
    uint16_t tx_port_id = tx_port.get_port_id();

    int ret;

    std::ostringstream oss;
    oss << STATS_TABLE_NAME
        << "(CMAC-" << core_id << ")"
        << ",rx_port=" << to_string(rx_port.get_bdf())
        << ",tx_port=" << to_string(tx_port.get_bdf())
        << " ";

    std::string kafka_header = oss.str();
    std::string kafka_data;
    std::string kafka_message;

    kafka_data.reserve(1024);
    kafka_message.reserve(2048);

    while (!rte_atomic32_read(&ctx->stop_flag)) {
        // Get FPGA level stats

        // Get rx stats
        CmacStats rx_stats = ctx->rx_onic->get_cmac_stats(rx_port_id); // ports are mapped directly to cmac ids
        CmacStats tx_stats = ctx->rx_onic->get_cmac_stats(tx_port_id); // ports are mapped directly to cmac ids

        curr_time = rte_get_tsc_cycles();
        uint64_t delta_ns = StatsLog::get_tsc_delta_ns(old_time, curr_time, hz);
        old_time = curr_time;
        kafka_message = kafka_header + \
                        rx_stats.to_string("R") + "," + \
                        tx_stats.to_string("T") + ","\
                        "DELTA_NS=" + std::to_string(delta_ns) + "\n";
        produce_kafka_message(kafka_message);   
        rte_delay_ms(3000);
    }
    return 0;
}

std::string StatsLog::rte_stats_to_string(const struct rte_eth_stats &stats, std::string direction) {
    std::ostringstream oss;
    oss << direction << "rx_packets=" << stats.ipackets
        << "," << direction << "rx_Bytes=" << stats.ibytes
        << "," << direction << "rx_fail_packets=" << stats.ierrors
        << "," << direction << "tx_packets=" << stats.opackets
        << "," << direction << "tx_Bytes=" << stats.obytes
        << "," << direction << "tx_fail_packets=" << stats.oerrors;

    return oss.str();
}

void StatsLog::extract_then_produce_latency_packets(const ForwardingContext *ctx){
    struct rte_ring *mbuf_ring = ctx->stats_ring;
    struct rte_mbuf *mbufs[BURST_SIZE];

    // Check ring for new packets
    unsigned int nb_rx = rte_ring_dequeue_burst(ctx->rx_onic->mbuf_ring, (void **)mbufs, BURST_SIZE, NULL);

    for(int i = 0; i < nb_rx; i++){
        Timestamps timestamp(mbufs[i]);
        std::string latency = timestamp.get_latency_str();
        produce_latency_message(latency);
    }
}

void StatsLog::produce_latency_message(std::string latency){
    static std::string kafka_header = std::string(LATENCY_TABLE_NAME) + ", ";
    std::string kafka_payload = kafka_header + latency + "\n";
    produce_kafka_message(kafka_payload, NULL, 0);
}

void StatsLog::create_kafka_topic(const char *topic){
    char hostname[128];
    char errstr[512];

    // 1. Create Kafka configuration
    rd_kafka_conf_t *conf = rd_kafka_conf_new();

    // 2. Set client ID (hostname)
    if (gethostname(hostname, sizeof(hostname))) {
        std::cerr << "Failed to get hostname: " << strerror(errno) << std::endl;
        exit(1);
    }

    if (rd_kafka_conf_set(conf, "client.id", hostname, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        std::cerr << "Kafka config error (client.id): " << errstr << std::endl;
        exit(1);
    }

    // 3. Set brokers (comma-separated list)
    if (rd_kafka_conf_set(conf, "bootstrap.servers", "localhost:9092", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        std::cerr << "Kafka config error (bootstrap.servers): " << errstr << std::endl;
        exit(1);
    }

    // 4. Topic configuration (acks=all for reliability)
    rd_kafka_topic_conf_t *topic_conf = rd_kafka_topic_conf_new();
    if (rd_kafka_topic_conf_set(topic_conf, "acks", "all", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        std::cerr << "Kafka topic config error (acks): " << errstr << std::endl;
        exit(1);
    }

    // 5. Create Kafka producer
    rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!rk) {
        std::cerr << "Failed to create Kafka producer: " << errstr << std::endl;
        exit(1);
    }

    // 6. Create topic object
    rkt = rd_kafka_topic_new(rk, topic, topic_conf);
    if (!rkt) {
        std::cerr << "Failed to create topic object: " << rd_kafka_err2str(rd_kafka_last_error()) << std::endl;
        exit(1);
    }
}

void StatsLog::produce_kafka_message(std::string payload, const char *key, size_t key_len) {
    if (rd_kafka_produce(
            rkt,
            RD_KAFKA_PARTITION_UA,  // Let Kafka choose partition
            RD_KAFKA_MSG_F_COPY,    // Copy payload (so you can reuse the buffer)
            (void *)payload.c_str(),        // Message payload
            payload.size(),            // Payload length
            key,                    // Optional key (NULL if unused)
            key_len,               // Key length (0 if unused)
            NULL                    // Opaque message data (optional)
        ) == -1) {
        // std::cerr << "Failed to produce to topic " << topic << ": "
                //   << rd_kafka_err2str(rd_kafka_last_error()) << std::endl;
    } else {
        // std::cout << "Successfully produced message to topic: " << topic << std::endl;
    }

}

void StatsLog::cleanup_kafka() {
    rd_kafka_flush(rk, 10 * 1000);  // Wait for messages to be sent
    rd_kafka_topic_destroy(rkt);
    rd_kafka_destroy(rk);
}
