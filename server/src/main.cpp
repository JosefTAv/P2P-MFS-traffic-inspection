#include "main.h"
#include "onic_port.h"
#include "stats.h"

#include <librdkafka/rdkafka.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_atomic.h>

#include <iostream>

#include <rte_metrics.h>

#define BURST_SIZE 32
#define MBUF_CACHE_SIZE 250

#define STATS_RING_SIZE 8192

int software_forwarder_thread(void *arg);
std::atomic<bool> sigkill{false};

void force_exit_handler(int) {
    printf(" Force exiting\n");
    sigkill= true;  // Signal threads to stop
}

unsigned int launch_software_forwarder(unsigned int prev_lcore_id, ForwardingContext &ctx);

void produce_kafka();
void produce_kafka(const char *topic, const char *payload, size_t payload_len, const char *key, size_t key_len);

#define NB_FORWARDS (4)
struct ForwardingContext ctx[NB_FORWARDS];

int main(int argc, char* argv[]){
	std::signal(SIGINT, force_exit_handler);  // Catch Ctrl+C

    const struct rte_memzone *mz = 0;
	int port_id   = 0;
	int ret = 0;
	int curr_avail_ports = 0;
    int num_ports = 0;

	/* Make sure the port is configured.  Zero everything and
	 * hope for same defaults
	 */

	printf("Onic rte eal init...\n");

	/* Make sure things are initialized ... */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	rte_log_set_global_level(RTE_LOG_DEBUG);

	num_ports = rte_eth_dev_count_avail();
	if (num_ports < 1)
		rte_exit(EXIT_FAILURE, "No Ethernet devices found."
			" Try updating the FPGA image.\n");

	/* Make sure things are defined ... */
	do_sanity_checks();

	/* Allocate aligned mezone */
	rte_pmd_qdma_compat_memzone_reserve_aligned();
	/******************************************************************************************************************
														Init Onics
	******************************************************************************************************************/

	PortInfo pinfos[] = {
		{0, 2, 1024, 2, 4096, 0, 2},
		{0, 2, 1024, 2, 4096, 0, 2}
	};
	int onic0_port_ids[] = {0, 1};
	int onic1_port_ids[] = {2, 3};

	int nb_ports = sizeof(onic0_port_ids)/sizeof(onic0_port_ids[0]);

	Onic onic0(pinfos, onic0_port_ids, nb_ports);
	Onic onic1(pinfos, onic1_port_ids, nb_ports);

    /******************************************************************************************************************
											Configure contexts and rings
	******************************************************************************************************************/

    struct rte_ring *stats_ring = rte_ring_create(
        "mbuf_ring",
        STATS_RING_SIZE,
        rte_socket_id(),
        RING_F_SC_DEQ // Single-consumer if only one thread dequeues
    );

    if (stats_ring == NULL) {
        rte_exit(EXIT_FAILURE, "Cannot create ring\n");
    }


    ctx[0] = ForwardingContext{
        .ctx_id = 0,
        .rx_onic = &onic0,
        .tx_onic = &onic0,
        .rx_port = 0,
        .tx_port = 1,

        .rx_Qs = {0, -1,-1},
        .tx_Qs = {0, -1,-1},
        .nb_rx_Qs = 1,
        .nb_tx_Qs = 1,
        
        .stats_ring = stats_ring,

        .stop_flag = RTE_ATOMIC32_INIT(0),
    };

    ctx[1] = ForwardingContext{
        .ctx_id = 1,
        .rx_onic = &onic0,
        .tx_onic = &onic0,
        .rx_port = 1,
        .tx_port = 0,

        .rx_Qs = {0, -1,-1},
        .tx_Qs = {0, -1,-1},
        .nb_rx_Qs = 1,
        .nb_tx_Qs = 1,
        
        .stats_ring = stats_ring,

        .stop_flag = RTE_ATOMIC32_INIT(0),
    };

    ctx[2] = ForwardingContext{
        .ctx_id = 2,
        .rx_onic = &onic1,
        .tx_onic = &onic1,
        .rx_port = 0,
        .tx_port = 1,

        .rx_Qs = {0, -1,-1},
        .tx_Qs = {0, -1,-1},
        .nb_rx_Qs = 1,
        .nb_tx_Qs = 1,
        
        .stats_ring = stats_ring,

        .stop_flag = RTE_ATOMIC32_INIT(0),
    };

    ctx[3] = ForwardingContext{
        .ctx_id = 3,
        .rx_onic = &onic1,
        .tx_onic = &onic1,
        .rx_port = 1,
        .tx_port = 0,

        .rx_Qs = {0, -1,-1},
        .tx_Qs = {0, -1,-1},
        .nb_rx_Qs = 1,
        .nb_tx_Qs = 1,
        
        .stats_ring = stats_ring,

        .stop_flag = RTE_ATOMIC32_INIT(0),
    };
    
	/******************************************************************************************************************
											Begin software forwarders
	******************************************************************************************************************/
    unsigned lcore_id = rte_get_next_lcore(-1, 1, 0);
    for(auto & i : ctx){
        i.print_schema();
        lcore_id = launch_software_forwarder(lcore_id, i);
    }
    // rte_eal_remote_launch(software_forwarder_thread, &ctx, lcore_id);

    /******************************************************************************************************************
											Begin stats producers
	******************************************************************************************************************/
    std::vector<StatsLog> stats;
    stats.reserve(NB_FORWARDS);
    for (int i = 0; i < NB_FORWARDS; i++) {
        // std::string topic_str = "Ports" + std::to_string(i);
        stats.emplace_back(&ctx[i], "Ports");
        lcore_id = rte_get_next_lcore(lcore_id, 1, 0);
        rte_eal_remote_launch(StatsLog_run_producer, &stats[i], lcore_id);
    }

    // StatsLog cmac_stats(&ctx, "CMACs");
    // lcore_id = rte_get_next_lcore(lcore_id, 1, 0);
    // rte_eal_remote_launch(StatsLog_cmac_producer, &cmac_stats, lcore_id);

    // // Main thread: Print stats every second
	// uint32_t old_rx_dpdk = 0;
	// uint32_t old_tx_dpdk = 0;

	// CmacStats oldStats;
    // CmacStats accumStats;

    // while (!sigkill) {
    //     CmacStats stats0 = onic0.get_cmac_stats(0);
	// 	CmacStats stats1 = onic0.get_cmac_stats(1);

	// 	if(old_rx_dpdk != rte_atomic32_read(&ctx.rx_count) || old_tx_dpdk != rte_atomic32_read(&ctx.tx_count)){
    //     	printf("(dpdk) Packets received: %d\n", rte_atomic32_read(&ctx.rx_count));
	// 		old_rx_dpdk = rte_atomic32_read(&ctx.rx_count);

	// 		printf("(dpdk) Packets sent: %d\n", rte_atomic32_read(&ctx.tx_count));
	// 		old_tx_dpdk = rte_atomic32_read(&ctx.tx_count);

	// 	}

    //     sleep(1);
    // }

    while (!sigkill);

    // Cleanup
    for(auto & i : ctx)
        rte_atomic32_set(&i.stop_flag, 1);

    unsigned nb_lcores = rte_lcore_count();  // Get the total number of cores
    for (lcore_id = 1; lcore_id < nb_lcores; lcore_id++) {  // Skip lcore 0 (main core)
        if (rte_lcore_is_enabled(lcore_id)) {  // Check if the lcore is enabled
            printf("Waiting for Lcore %u to finish...\n", lcore_id);
            rte_eal_wait_lcore(lcore_id);  // Wait for the lcore to finish
        }
    }
	/******************************************************************************************************************
											Kafka test
	******************************************************************************************************************/
	// const char *topic = "telegraf";
    // const char *payload = "TestTable,tag1=test ti=50,t2=\"1\"\n";
    // produce_kafka(topic, payload, strlen(payload), NULL, 0);
	rte_delay_ms(1000);

    return 0;
}

unsigned int launch_software_forwarder(unsigned int prev_lcore_id, ForwardingContext &ctx){
    prev_lcore_id = rte_get_next_lcore(prev_lcore_id, 1, 0);
    rte_eal_remote_launch(fpga_rx_thread, &ctx, prev_lcore_id);

    prev_lcore_id = rte_get_next_lcore(prev_lcore_id, 1, 0);
    rte_eal_remote_launch(fpga_tx_thread, &ctx, prev_lcore_id);

    return prev_lcore_id;
}

void produce_kafka(const char *topic, const char *payload, size_t payload_len, const char *key, size_t key_len) {
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
    rd_kafka_t *rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!rk) {
        std::cerr << "Failed to create Kafka producer: " << errstr << std::endl;
        exit(1);
    }

    // 6. Create topic object
    rd_kafka_topic_t *rkt = rd_kafka_topic_new(rk, topic, topic_conf);
    if (!rkt) {
        std::cerr << "Failed to create topic object: " << rd_kafka_err2str(rd_kafka_last_error()) << std::endl;
        exit(1);
    }

    // 7. Produce message
    if (rd_kafka_produce(
            rkt,
            RD_KAFKA_PARTITION_UA,  // Let Kafka choose partition
            RD_KAFKA_MSG_F_COPY,    // Copy payload (so you can reuse the buffer)
            (void *)payload,        // Message payload
            payload_len,            // Payload length
            key,                    // Optional key (NULL if unused)
            key_len,               // Key length (0 if unused)
            NULL                    // Opaque message data (optional)
        ) == -1) {
        std::cerr << "Failed to produce to topic " << topic << ": "
                  << rd_kafka_err2str(rd_kafka_last_error()) << std::endl;
    } else {
        std::cout << "Successfully produced message to topic: " << topic << std::endl;
    }

    // 8. Wait for messages to be delivered (optional but recommended)
    rd_kafka_flush(rk, 10 * 1000);  // Wait up to 10 seconds

    // 9. Cleanup
    rd_kafka_topic_destroy(rkt);
    rd_kafka_destroy(rk);
}
