#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_cycles.h>

#include <cstring> // for strcmp

#define NUM_MBUFS 4096
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 1

#define CUSTOM_VLAN_ID (0x0ABC)
#define DEFAULT_FILLER (0xFF)

static const uint16_t rss_symmetric_key[10] = {0};

int main(int argc, char **argv) {
    if (argc < 1 || strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "help") == 0) {
        printf("Usage: ./my_app [options]\n");
        return 0;
    }

	int ret = 0;
    int num_ports = 0;
    unsigned port_id = 0;
    uint64_t period = 2550; //nb cycles per period
    uint16_t pkt_size = 60;

	/* Make sure things are initialized ... */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	rte_log_set_global_level(RTE_LOG_DEBUG);

	num_ports = rte_eth_dev_count_avail();
	if (num_ports < 1)
		rte_exit(EXIT_FAILURE, "No Ethernet devices found."
			" Try updating the FPGA image.\n");
    uint64_t hz = rte_get_timer_hz();

    argc -= ret;
    argv += ret;

    if (argc > 1) {
        port_id = atoi(argv[1]);
    }
    if (argc > 2) {
        period = strtoull(argv[2], NULL, 10);
    }
    if (argc > 3) {
        pkt_size = atoi(argv[3]);
    }

    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
        NUM_MBUFS * num_ports, MBUF_CACHE_SIZE, 0,
        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (!mbuf_pool) rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    struct rte_eth_conf conf = {
        .rxmode = {.max_lro_pkt_size = RTE_ETHER_MAX_LEN},
        .txmode = {.mq_mode = RTE_ETH_MQ_TX_NONE},
    };

    ret = rte_eth_dev_configure(port_id, 0, 1, &conf);
    if (ret < 0) rte_exit(EXIT_FAILURE, "Config failed\n");

    ret = rte_eth_tx_queue_setup(port_id, 0, 512, rte_eth_dev_socket_id(port_id), NULL);
    if (ret < 0) rte_exit(EXIT_FAILURE, "TX queue setup failed\n");

    ret = rte_eth_dev_start(port_id);
    if (ret < 0) rte_exit(EXIT_FAILURE, "Port start failed\n");

    printf("Sending one packet every %" PRIu64 " cycles (%g µs at %.2fGHz)\n",
           period, (double)1e6*period/hz, hz/1e9);

    uint64_t next_tsc = rte_get_timer_cycles();

    // Build Ethernet + VLAN header
    struct rte_ether_hdr *eth;
    struct rte_vlan_hdr *vlan;
    char *pkt_data;

    struct rte_mbuf *m = rte_pktmbuf_alloc(mbuf_pool);
    pkt_data = rte_pktmbuf_append(m, pkt_size);
    memset(pkt_data, DEFAULT_FILLER, pkt_size); // fill rest with dummy payload

    eth = (struct rte_ether_hdr *)pkt_data;

    // Set arbitrary MACs
    struct rte_ether_addr dst_mac = {{0x52, 0x54, 0x00, 0x12, 0x34, 0x56}};
    struct rte_ether_addr src_mac = {{0x52, 0x54, 0x00, 0x65, 0x43, 0x21}};
    rte_ether_addr_copy(&dst_mac, &eth->dst_addr);
    rte_ether_addr_copy(&src_mac, &eth->src_addr);

    // VLAN tag
    eth->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN); // 0x8100

    vlan = (struct rte_vlan_hdr *)(eth + 1); // VLAN header is after Eth header
    vlan->vlan_tci = rte_cpu_to_be_16(CUSTOM_VLAN_ID); // VLAN ID 0xABC
    vlan->eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4); // Payload Ethertype

    uint64_t count = 0;
    while (1) {
        uint64_t cur = rte_get_timer_cycles();
        if (cur < next_tsc) {
            rte_pause();
            continue;
        }
        next_tsc += period;

        struct rte_mbuf *txm = rte_pktmbuf_clone(m, mbuf_pool);
        if (!txm) {
            fprintf(stderr, "Failed to clone mbuf\n");
            continue;
        }

        const uint16_t sent = rte_eth_tx_burst(port_id, 0, &txm, 1);
        if (sent < 1) {
            rte_pktmbuf_free(txm);
            fprintf(stderr, "Packet dropped\n");
        }
        count++;
    }

    // never reached
    rte_eth_dev_stop(port_id);
    rte_eth_dev_close(port_id);
    rte_mempool_free(mbuf_pool);
    return 0;
}
