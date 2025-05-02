#include <iostream>
#include "onic_port.h"

int OnicPort::port_init()
{
    struct rte_mempool *mbuf_pool;
    struct rte_eth_conf	    port_conf;
    struct rte_eth_txconf   tx_conf;
    struct rte_eth_rxconf   rx_conf;
    int                     diag, x;
    uint32_t                queue_base, nb_buff;

    printf("Setting up port :%d.\n", port_id);

    if (rte_pmd_qdma_get_device(port_id) == NULL) {
    printf("Port id %d already removed. "
    "Relaunch application to use the port again\n",
    port_id);
    return -1;
    }

    snprintf(pinfo.mem_pool, RTE_MEMPOOL_NAMESIZE,
    MBUF_POOL_NAME_PORT, port_id);

    /* Mbuf packet pool */
    nb_buff = ((pinfo.nb_descs) * pinfo.num_queues * 2);

    /* NUM_TX_PKTS should be added to every queue as that many descriptors
    * can be pending with application after Rx processing but before
    * consumed by application or sent to Tx
    */
    nb_buff += ((NUM_TX_PKTS) * pinfo.num_queues);

    /*
    * rte_mempool_create_empty() has sanity check to refuse large cache
    * size compared to the number of elements.
    * CACHE_FLUSHTHRESH_MULTIPLIER (1.5) is defined in a C file, so using a
    * constant number 2 instead.
    */
    nb_buff = RTE_MAX(nb_buff, MP_CACHE_SZ * 2);

    mbuf_pool = rte_pktmbuf_pool_create(pinfo.mem_pool, nb_buff,
    MP_CACHE_SZ, 0, pinfo.buff_size +
    RTE_PKTMBUF_HEADROOM,
    pinfo.socket_id);

    if (mbuf_pool == NULL)
    rte_exit(EXIT_FAILURE, " Cannot create mbuf pkt-pool\n");
    #ifdef DUMP_MEMPOOL_USAGE_STATS
    printf("%s(): %d: mpool = %p, mbuf_avail_count = %d,"
    " mbuf_in_use_count = %d,"
    "nb_buff = %u\n", __func__, __LINE__, mbuf_pool,
    rte_mempool_avail_count(mbuf_pool),
    rte_mempool_in_use_count(mbuf_pool), nb_buff);
    #endif //DUMP_MEMPOOL_USAGE_STATS

    /*
    * Make sure the port is configured.  Zero everything and
    * hope for sane defaults
    */
    memset(&port_conf, 0x0, sizeof(struct rte_eth_conf));
    memset(&tx_conf, 0x0, sizeof(struct rte_eth_txconf));
    memset(&rx_conf, 0x0, sizeof(struct rte_eth_rxconf));
    diag = rte_pmd_qdma_get_bar_details(port_id,
        &(pinfo.config_bar_idx),
        &(pinfo.user_bar_idx),
        &(pinfo.bypass_bar_idx));

    if (diag < 0)
    rte_exit(EXIT_FAILURE, "rte_pmd_qdma_get_bar_details failed\n");

    printf("QDMA Config bar idx: %d\n", pinfo.config_bar_idx);
    printf("QDMA AXI Master Lite bar idx: %d\n", pinfo.user_bar_idx);
    printf("QDMA AXI Bridge Master bar idx: %d\n", pinfo.bypass_bar_idx);

    /* configure the device to use # queues */
    diag = rte_eth_dev_configure(port_id, pinfo.num_queues, pinfo.num_queues,
    &port_conf);
    if (diag < 0)
    rte_exit(EXIT_FAILURE, "Cannot configure port %d (err=%d)\n",
        port_id, diag);

    diag = rte_pmd_qdma_get_queue_base(port_id, &queue_base);
    if (diag < 0)
    rte_exit(EXIT_FAILURE, "rte_pmd_qdma_get_queue_base : Querying of "
        "QUEUE_BASE failed\n");
    pinfo.queue_base = queue_base;

    for (x = 0; x < pinfo.num_queues; x++) {
    if (x < pinfo.st_queues) {
    diag = rte_pmd_qdma_set_queue_mode(port_id, x,
            RTE_PMD_QDMA_STREAMING_MODE);
    if (diag < 0)
        rte_exit(EXIT_FAILURE, "rte_pmd_qdma_set_queue_mode : "
                "Passing of QUEUE_MODE "
                "failed\n");
    } else {
    diag = rte_pmd_qdma_set_queue_mode(port_id, x,
            RTE_PMD_QDMA_MEMORY_MAPPED_MODE);
    if (diag < 0)
        rte_exit(EXIT_FAILURE, "rte_pmd_qdma_set_queue_mode : "
                "Passing of QUEUE_MODE "
                "failed\n");
    }

    diag = rte_eth_tx_queue_setup(port_id, x, pinfo.nb_descs, 0,
        &tx_conf);
    if (diag < 0)
    rte_exit(EXIT_FAILURE, "Cannot setup port %d "
            "TX Queue id:%d "
            "(err=%d)\n", port_id, x, diag);
    rx_conf.rx_thresh.wthresh = DEFAULT_RX_WRITEBACK_THRESH;
    diag = rte_eth_rx_queue_setup(port_id, x, pinfo.nb_descs, 0,
        &rx_conf, mbuf_pool);
    if (diag < 0)
    rte_exit(EXIT_FAILURE, "Cannot setup port %d "
            "RX Queue 0 (err=%d)\n", port_id, diag);
    }

    diag = rte_eth_dev_start(port_id);
    if (diag < 0)
    rte_exit(EXIT_FAILURE, "Cannot start port %d (err=%d)\n",
        port_id, diag);

    // Check BDF
    uint32_t b, d, f;
    rte_pmd_qdma_get_bdf(port_id, &b, &d, &f);
    printf("BDF(%02x:%02x.%01x) configured on port_id(%d)\n", b, d, f, port_id);

    return 0;
}

void OnicPort::port_close(){
	struct rte_mempool *mp;
	struct rte_pmd_qdma_dev_attributes dev_attr;
	int user_bar_idx;
	int reg_val;
	int ret = 0;

	if (rte_pmd_qdma_get_device(port_id) == NULL) {
		printf("Port id %d already removed. "
			"Relaunch application to use the port again\n",
			port_id);
		return;
	}

	user_bar_idx = pinfo.user_bar_idx;
	ret = rte_pmd_qdma_get_device_capabilities(port_id, &dev_attr);
	if (ret < 0) {
		printf("rte_pmd_qdma_get_device_capabilities failed for port: %d\n",
			port_id);
		return;
	}

	if ((dev_attr.device_type == RTE_PMD_QDMA_DEVICE_SOFT)
			&& (dev_attr.ip_type == RTE_PMD_EQDMA_SOFT_IP)) {
		PciWrite(user_bar_idx, C2H_CONTROL_REG,
				C2H_STREAM_MARKER_PKT_GEN_VAL,
				port_id);
		unsigned int retry = 50;
		while (retry) {
			usleep(500);
			reg_val = PciRead(user_bar_idx,
				C2H_STATUS_REG, port_id);
			if (reg_val & MARKER_RESPONSE_COMPLETION_BIT)
				break;

			printf("Failed to receive c2h marker completion, retry count = %u\n",
					(50 - (retry-1)));
			retry--;
		}
	}

	rte_eth_dev_stop(port_id);

	rte_pmd_qdma_dev_close(port_id);

	pinfo.num_queues = 0;

	mp = rte_mempool_lookup(pinfo.mem_pool);

	if (mp != NULL)
		rte_mempool_free(mp);
}

int OnicPort::port_reset(){
	int ret = 0;

	if (rte_pmd_qdma_get_device(port_id) == NULL) {
		printf("Port id %d already removed. "
			"Relaunch application to use the port again\n",
			port_id);
		return -1;
	}

	rte_spinlock_lock(&pinfo.port_update_lock);

	port_close();

	ret = rte_eth_dev_reset(port_id);
	if (ret < 0) {
		printf("Error: Failed to reset device for port: %d\n", port_id);
		rte_spinlock_unlock(&pinfo.port_update_lock);
		return -1;
	}

	ret = port_init();
	if (ret < 0)
		printf("Error: Failed to initialize port: %d\n", port_id);

	rte_spinlock_unlock(&pinfo.port_update_lock);

	if (!ret)
		printf("Port reset done successfully on port-id: %d\n",
			port_id);

	return ret;
}

int OnicPort::port_remove(){
	int ret = 0;

	/* Detach the port, it will invoke device remove/uninit */
	printf("Removing a device with port id %d\n", port_id);
	if (rte_pmd_qdma_get_device(port_id) == NULL) {
		printf("Port id %d already removed\n", port_id);
		return 0;
	}

	rte_spinlock_lock(&pinfo.port_update_lock);

	port_close();

	ret = rte_pmd_qdma_dev_remove(port_id);
	if (ret < 0)
		printf("Failed to remove device on port_id: %d\n", port_id);

	rte_spinlock_unlock(&pinfo.port_update_lock);

	if (!ret)
		printf("Port remove done successfully on port-id: %d\n",
			port_id);

	return ret;
}

void OnicPort::register_callbacks(){
    int ret = rte_eth_dev_callback_register(port_id, RTE_ETH_EVENT_INTR_RMV,
        dev_remove_callback, this);
    if (ret < 0)
    rte_exit(EXIT_FAILURE, "Failed to register dev_remove_callback\n");

    ret = rte_eth_dev_callback_register(port_id,
        RTE_ETH_EVENT_INTR_RESET,
        dev_reset_callback, this);
    if (ret < 0)
    rte_exit(EXIT_FAILURE, "Failed to register dev_reset_callback\n");
}

int OnicPort::dev_reset_callback(uint16_t port_id,
    enum rte_eth_event_type type,
    void *param __rte_unused, void *ret_param){
    int ret = 0;

    RTE_SET_USED(ret_param);
    printf("%s is received\n", __func__);

    if (type != RTE_ETH_EVENT_INTR_RESET) {
        printf("Error: Invalid event value. "
            "Expected = %d, Received = %d\n",
            RTE_ETH_EVENT_INTR_RESET, type);
        return -ENOMSG;
    }

    OnicPort* self = static_cast<OnicPort*>(param);
    ret = self->port_reset();
    if (ret < 0)
        printf("Error: Failed to reset port: %d\n", port_id);

    return ret;
}

int OnicPort::dev_remove_callback(uint16_t port_id,
    enum rte_eth_event_type type,
    void *param __rte_unused, void *ret_param){
    int ret = 0;

    RTE_SET_USED(ret_param);
    printf("%s is received\n", __func__);

    if (type != RTE_ETH_EVENT_INTR_RMV) {
        printf("Error: Invalid event value. "
            "Expected = %d, Received = %d\n",
            RTE_ETH_EVENT_INTR_RMV, type);
        return -ENOMSG;
    }

    OnicPort* self = static_cast<OnicPort*>(param);
    ret = self->port_remove();
    if (ret < 0)
        printf("Error: Failed to remove port: %d\n", port_id);

    return 0;
}

void OnicPort::unregister_callbacks(){
    rte_eth_dev_callback_unregister(RTE_ETH_ALL, RTE_ETH_EVENT_INTR_RMV,
        dev_remove_callback, NULL);

    rte_eth_dev_callback_unregister(RTE_ETH_ALL, RTE_ETH_EVENT_INTR_RESET,
        dev_reset_callback, NULL);
}
