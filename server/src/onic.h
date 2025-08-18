#pragma once

#include "onic_port.h"
#include "onic_regs.h"
#include <rte_cycles.h>

#include <string>
#include <array>
#include <new>

#define NB_CMAC (2)
#define NB_FUNC (2)

#define NB_PORTS (NB_FUNC)

#define ONIC_CMAC_CORE_VERSION		(0x00000301)
#define RX_ALIGN_TIMEOUT_MS			(1000)
#define CMAC_RESET_WAIT_MS			(1)

#define FIELD_NAME(field) #field

// Mempool constants
#define RING_SIZE 8192

struct CmacStats{
    uint32_t tx_total_pkts = 0;
    uint32_t tx_total_good_pkts = 0;
    uint32_t tx_total_bytes = 0;
    uint32_t tx_total_good_bytes = 0;

    uint32_t rx_total_pkts = 0;
    uint32_t rx_total_good_pkts = 0;
    uint32_t rx_total_bytes = 0;
    uint32_t rx_total_good_bytes = 0;

    uint32_t rx_pkt_65_127 = 0;
    uint32_t rx_pkt_large = 0;
    uint32_t rx_pkt_small = 0;
    uint32_t rx_undersize = 0;
    uint32_t rx_frag = 0;
    uint32_t rx_oversize = 0;
    uint32_t rx_toolong = 0;
    uint32_t rx_jabber = 0;
    uint32_t rx_bad_fcs = 0;
    uint32_t rx_pkt_bad_fcs = 0;
    uint32_t rx_stomped_fcs = 0;
    uint32_t rx_unicast = 0;
    uint32_t rx_multicast = 0;
    uint32_t rx_broadcast = 0;
    uint32_t rx_vlan = 0;
    uint32_t rx_pause = 0;
    uint32_t rx_user_pause = 0;
    uint32_t rx_in_range_err = 0;
    uint32_t rx_truncated = 0;

    void print(bool debug=false) const{
        #define PRINT_FIELD(field) \
            printf("(CMAC) %-25s %10u\n", #field, field);

        PRINT_FIELD(tx_total_pkts);
        PRINT_FIELD(tx_total_good_pkts);
        PRINT_FIELD(tx_total_bytes);
        PRINT_FIELD(tx_total_good_bytes);

        PRINT_FIELD(rx_total_pkts);
        PRINT_FIELD(rx_total_good_pkts);
        PRINT_FIELD(rx_total_bytes);
        PRINT_FIELD(rx_total_good_bytes);

        if(debug){
            PRINT_FIELD(rx_pkt_65_127);
            PRINT_FIELD(rx_pkt_large);
            PRINT_FIELD(rx_pkt_small);
            PRINT_FIELD(rx_undersize);
            PRINT_FIELD(rx_frag);
            PRINT_FIELD(rx_oversize);
            PRINT_FIELD(rx_toolong);
            PRINT_FIELD(rx_jabber);
            PRINT_FIELD(rx_bad_fcs);
            PRINT_FIELD(rx_pkt_bad_fcs);
            PRINT_FIELD(rx_stomped_fcs);
            PRINT_FIELD(rx_unicast);
            PRINT_FIELD(rx_multicast);
            PRINT_FIELD(rx_broadcast);
            PRINT_FIELD(rx_vlan);
            PRINT_FIELD(rx_pause);
            PRINT_FIELD(rx_user_pause);
            PRINT_FIELD(rx_in_range_err);
            PRINT_FIELD(rx_truncated);
        }
        #undef PRINT_FIELD  // Clean up macro after use
        printf("\n");
    };

    std::string to_string(std::string tag="") const {
        std::string string_out;
        char buf[64];

        auto append_field = [&](std::string tag, std::string field_name, uint32_t field) {
            snprintf(buf, sizeof(buf), "(%s)%s=%d", tag.c_str(), field_name.c_str(), field);
            string_out += std::string(buf) + ",";
        };

        append_field(tag, "tx_total_pkts", tx_total_pkts);
        append_field(tag, "tx_total_good_pkts", tx_total_good_pkts);
        append_field(tag, "tx_total_bytes", tx_total_bytes);
        append_field(tag, "tx_total_good_bytes", tx_total_good_bytes);
        
        append_field(tag, "rx_total_pkts", rx_total_pkts);
        append_field(tag, "rx_total_good_pkts", rx_total_good_pkts);
        append_field(tag, "rx_total_bytes", rx_total_bytes);
        append_field(tag, "rx_total_good_bytes", rx_total_good_bytes);

        // Remove trailing comma
        if (!string_out.empty()) {
            string_out.pop_back();
        }

        return string_out;
    }

    void add(const CmacStats& other) {
        tx_total_pkts += other.tx_total_pkts;
        tx_total_good_pkts += other.tx_total_good_pkts;
        tx_total_bytes += other.tx_total_bytes;
        tx_total_good_bytes += other.tx_total_good_bytes;

        rx_total_pkts += other.rx_total_pkts;
        rx_total_good_pkts += other.rx_total_good_pkts;
        rx_total_bytes += other.rx_total_bytes;
        rx_total_good_bytes += other.rx_total_good_bytes;
    }
};

class Onic {
    private:
        // Attributes
        std::array<OnicPort, NB_PORTS> ports;
        int config_port_id; // also refers to master_pf
        int axil_bar_id;

        int RS_FEC;

        // DPDK logs
        static int ONIC_LOG_TYPE;

        inline static int ring_ids = 0;

    void onic_log(uint32_t level, const char *format, ...);

    public:
        // mempools
        struct rte_ring *mbuf_ring; // NUMA local to onic

        // High level functions
        int init_hardware();
        int enable_cmac(int cmac_id);
        CmacStats get_cmac_stats(int cmac_id, bool debug=false) const;
        void print_packet_adaptor_stats(int cmac_id);
        void get_cmac_debug_stats(int cmac_id, CmacStats &stats) const;

        std::string ring_name;


        // Constructors/etc
        Onic(PortInfo portInfos[], int port_ids[], int nb_ports, int config_port_id=-1, int axil_bar_id=-1,
            int RS_FEC=0) 
        {
            assert(nb_ports <= NB_PORTS);

            // setup ring
            ring_name = std::to_string(ring_ids++);
            mbuf_ring = rte_ring_create(ring_name.c_str(),
                                    RING_SIZE,
                                rte_eth_dev_socket_id(port_ids[0]),
                                    RING_F_SP_ENQ | RING_F_SC_DEQ);
            
            // setup Qs
            for(int i=0; i<nb_ports; i++) {
                new (&ports[i]) OnicPort(portInfos[i], port_ids[i]);
            }

            // setup hardware
            this->config_port_id = (config_port_id == -1) ? port_ids[0] : config_port_id;
            this->axil_bar_id = (axil_bar_id == -1) ? ports[0].get_pinfo().user_bar_idx : axil_bar_id;
            this->RS_FEC = RS_FEC;
            init_hardware();
        };
        ~Onic(){
            onic_log(RTE_LOG_INFO, "Reset Onic\n");
            write_reg(SYSCFG_OFFSET_SYSTEM_RESET, 0x1); // Global reset
         };

        // Base functions
        uint32_t read_reg(uint32_t offset) const{
            return OnicPort::PciRead(axil_bar_id, offset, config_port_id);
        };
        void write_reg(uint32_t offset, uint32_t val) const{
            OnicPort::PciWrite(axil_bar_id, offset, val,config_port_id);
        };

        // getters/setters
        const std::array<OnicPort, NB_PORTS>& get_ports() const;
        std::array<int, NB_PORTS> get_port_ids() const;
    };
