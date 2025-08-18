#pragma once

#include <array>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rte_ethdev.h>
#include "../tools/dpdk-stable/drivers/net/qdma/rte_pmd_qdma.h"
#include "../tools/dpdk-stable/drivers/net/qdma/qdma_dpdk_compat.h"

#include <rte_spinlock.h>

#include "onic_helper.h"

#define QDMA_MAX_PORTS	256

#define PORT_0 0

#define NUM_DESC_PER_RING 1024
#ifdef PERF_BENCHMARK
#define NUM_RX_PKTS (NUM_DESC_PER_RING - 2)
#else
#define NUM_RX_PKTS 32
#endif

/* Tandem boot feature involves DMA transfer of
 * second stage bootloader size greater than 2MB
 * from Host to Slave Boot Interface(SBI) buffer.
 * Increased the limit of pending tx packets that need
 * to process by application to avoid transfer timeouts.
 */
#ifdef TANDEM_BOOT_SUPPORTED
#define NUM_TX_PKTS 128
#else
#define NUM_TX_PKTS 32
#endif

#define MAX_NUM_QUEUES 4096
#define DEFAULT_NUM_QUEUES 64
#define RX_TX_MAX_RETRY			1500
#define DEFAULT_RX_WRITEBACK_THRESH	(64)

#define MP_CACHE_SZ     512
#define MBUF_POOL_NAME_PORT   "mbuf_pool_%d"

/* AXI Master Lite bar(user bar) registers */
#define C2H_ST_QID_REG    0x0
#define C2H_ST_LEN_REG    0x4
#define C2H_CONTROL_REG              0x8
#define ST_LOOPBACK_EN               0x1
#define ST_C2H_START_VAL             0x2
#define ST_C2H_IMMEDIATE_DATA_EN     0x4
#define C2H_CONTROL_REG_MASK         0xF
#define H2C_CONTROL_REG    0xC
#define H2C_STATUS_REG    0x10
#define C2H_PACKET_COUNT_REG    0x20
#define C2H_STATUS_REG                    0x18
#define C2H_STREAM_MARKER_PKT_GEN_VAL     0x22
#define MARKER_RESPONSE_COMPLETION_BIT    0x1

// Default to -1 when not init
struct PortInfo {
    unsigned int queue_base;
    unsigned int num_queues;
    unsigned int nb_descs;
    unsigned int st_queues;
    unsigned int buff_size;
    unsigned int socket_id;
    int config_bar_idx; // IP default is 0
    int user_bar_idx; // IP default is 2
    int bypass_bar_idx; // IP default is 4
    rte_spinlock_t port_update_lock;
    char mem_pool[RTE_MEMPOOL_NAMESIZE];

    PortInfo(
        unsigned int queue_base = -1,
        unsigned int num_queues = -1,
        unsigned int nb_descs = -1,
        unsigned int st_queues = -1,
        unsigned int buff_size = -1,
        int config_bar_idx = -1,
        int user_bar_idx = -1,
        int bypass_bar_idx = -1,
        unsigned int socket_id = rte_socket_id())
        :
            queue_base(queue_base),
            num_queues(num_queues),
            nb_descs(nb_descs),
            st_queues(st_queues),
            buff_size(buff_size),
            config_bar_idx(config_bar_idx),
            user_bar_idx(user_bar_idx),
            bypass_bar_idx(bypass_bar_idx),
            socket_id(socket_id)
        {};
};

class OnicPort { // this is really a qdma port...
    public:
        // Attributes
        PortInfo pinfo;
        int port_id;
        uint32_t b, d, f;

    // public:
        // Constructors/etc
        OnicPort(){};
        OnicPort(PortInfo pinfo, int port_id)
            : pinfo(pinfo), port_id(port_id)
            {
                rte_spinlock_init(&pinfo.port_update_lock);
                int ret = port_init();
                if (ret < 0) {
                    std::cerr << "Error initializing port " << port_id << std::endl;
                } else {
                    std::cout << "Port " << port_id << " initialized successfully" << std::endl;
                }
                // int rte_pmd_qdma_get_device_capabilities(int port_id,
                //     struct rte_pmd_qdma_dev_attributes *dev_attr) //check that setup worked
            }
        ~OnicPort(){
            unregister_callbacks();
            if (pinfo.num_queues)
                port_close();

			printf("Removing a device with port id %d\n", port_id);
			if (rte_pmd_qdma_get_device(port_id) == NULL) {
				printf("Port id %d already removed\n", port_id);
			}

			if (rte_pmd_qdma_dev_remove(port_id))
				printf("Failed to detach port '%d'\n", port_id);
        }
        // High level functions
        int port_init();
        // int do_recv_st(int fd, int queueid, int input_size);
        // int do_recv_mm(int fd, int queueid,int ld_size,
        //         int tot_num_desc);
        // int do_xmit(int fd, int queueid,
        //         int ld_size, int tot_num_desc, int zbyte);
        void port_close();
        int port_reset();
        int port_remove();

        // Base functions
        static unsigned int PciRead(unsigned int bar, unsigned int offset, int port_id){
            return rte_pmd_qdma_compat_pci_read_reg(port_id, bar, offset);
        }
        static void PciWrite(unsigned int bar, unsigned int offset, unsigned int reg_val,
            int port_id){
            rte_pmd_qdma_compat_pci_write_reg(port_id, bar, offset, reg_val);
        }

        // Abstraction functions
        void register_callbacks();
        void unregister_callbacks();

        // Callback defs
        static int dev_reset_callback(uint16_t port_id,
            enum rte_eth_event_type type,
            void *param __rte_unused, void *ret_param);
        static int dev_remove_callback(uint16_t port_id,
            enum rte_eth_event_type type,
            void *param __rte_unused, void *ret_param);

        // getters/setters
        int get_port_id() const { return port_id; }
        PortInfo get_pinfo() const { return pinfo; }
        std::array<uint32_t, 3> get_bdf() const { return {b, d, f}; }
    };
