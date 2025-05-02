#include "main.h"
#include "onic_port.h"

#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_atomic.h>

#define BURST_SIZE 32
#define NUM_MBUFS 4096
#define MBUF_CACHE_SIZE 250

struct ForwardingContext {
    const OnicPort* rx_port;
    const OnicPort* tx_port;
	int inQ;
	int outQ;
    rte_atomic32_t stop_flag;
    rte_atomic32_t rx_count;
	rte_atomic32_t tx_count;
};

int software_forwarder_thread(void *arg);
std::atomic<bool> sigkill{false};

void force_exit_handler(int) {
    sigkill= true;  // Signal threads to stop
}

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
	rte_delay_ms(1000);
	// Onic onic1(pinfos, onic1_port_ids, nb_ports);

	static struct ForwardingContext ctx = {
		.rx_port = &onic0.get_ports()[0],
		.tx_port = &onic0.get_ports()[0],
		.inQ = 0,
		.outQ = 0,
		.stop_flag = RTE_ATOMIC32_INIT(0),
		.rx_count = RTE_ATOMIC32_INIT(0),
		.tx_count = RTE_ATOMIC32_INIT(0)
	};
	/******************************************************************************************************************
											Begin software forwarders
	******************************************************************************************************************/

	unsigned lcore_id = rte_get_next_lcore(-1, 1, 0);
    rte_eal_remote_launch(software_forwarder_thread, &ctx, lcore_id);

    // Main thread: Print stats every second
	uint32_t old_rx = 0;
	uint32_t old_tx = 0;
    while (!sigkill) {
		if(old_rx != rte_atomic32_read(&ctx.rx_count)){
        	printf("Packets received: %d\n", rte_atomic32_read(&ctx.rx_count));
			old_rx = rte_atomic32_read(&ctx.rx_count);
		}
		if(old_tx != rte_atomic32_read(&ctx.tx_count)){
			printf("Packets sent: %d\n", rte_atomic32_read(&ctx.tx_count));
			old_tx = rte_atomic32_read(&ctx.tx_count);
		}

        sleep(1);
    }
	rte_atomic32_set(&ctx.stop_flag, 1);

    // Cleanup
    rte_eal_wait_lcore(lcore_id);

	rte_delay_ms(1000);

    return 0;
}

int software_forwarder_thread(void *arg){
	auto *ctx = (struct ForwardingContext *)arg;
	struct rte_mbuf *mbufs[BURST_SIZE];

    RTE_LOG(INFO, USER1, "Worker started on lcore %u\n", rte_lcore_id());

    while (!rte_atomic32_read(&ctx->stop_flag)) {
        // Receive packets
        uint16_t nb_rx = rte_eth_rx_burst(ctx->rx_port->get_port_id(), ctx->inQ, mbufs, BURST_SIZE);
        if (unlikely(nb_rx == 0)) {
            rte_pause();
            continue;
        }

        // Update stats (atomic)
        rte_atomic32_add(&ctx->rx_count, nb_rx);

        // Transmit packets
        uint16_t nb_tx = rte_eth_tx_burst(ctx->tx_port->get_port_id(), ctx->outQ, mbufs, nb_rx);
		rte_atomic32_add(&ctx->tx_count, nb_tx);

        // Free any untransmitted packets
        if (unlikely(nb_tx < nb_rx)) {
            for (uint16_t i = nb_tx; i < nb_rx; i++) {
                rte_pktmbuf_free(mbufs[i]);
            }
        }
    }
    return 0;
}
