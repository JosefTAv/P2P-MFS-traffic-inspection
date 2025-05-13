#pragma once

#include "onic_port.h"
#include "onic_regs.h"
#include <rte_cycles.h>

#include <array>
#include <new>

#define NB_CMAC (2)
#define NB_FUNC (2)

#define NB_PORTS (NB_FUNC)

#define ONIC_CMAC_CORE_VERSION		(0x00000301)
#define RX_ALIGN_TIMEOUT_MS			(1000)
#define CMAC_RESET_WAIT_MS			(1)

struct CmacStats{
    uint32_t tx_total_pkts = 0;
    uint32_t tx_total_good_pkts = 0;
    uint32_t tx_total_bytes = 0;
    uint32_t tx_total_good_bytes = 0;

    uint32_t rx_total_pkts = 0;
    uint32_t rx_total_good_pkts = 0;
    uint32_t rx_total_bytes = 0;
    uint32_t rx_total_good_bytes = 0;

    void print() const{
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
        printf("\n");
        #undef PRINT_FIELD  // Clean up macro after use
    };

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

    void onic_log(uint32_t level, const char *format, ...);

    public:
        // High level functions
        int init_hardware();
        int enable_cmac(int cmac_id);
        CmacStats get_cmac_stats(int cmac_id);


        // Constructors/etc
        Onic(){
            init_hardware();
        };
        Onic(PortInfo portInfos[], int port_ids[], int nb_ports, int config_port_id=-1, int axil_bar_id=-1,
            int RS_FEC=0){
            assert(nb_ports <= NB_PORTS);
            for(int i=0; i<nb_ports; i++) {
                new (&ports[i]) OnicPort(portInfos[i], port_ids[i]);
            }

            this->config_port_id = (config_port_id == -1) ? port_ids[0] : config_port_id;
            this->axil_bar_id = (axil_bar_id == -1) ? ports[0].get_pinfo().user_bar_idx : axil_bar_id;
            this->RS_FEC = RS_FEC;
            init_hardware();
        };
        ~Onic(){
            onic_log(RTE_LOG_INFO, "Reset Onic\n");
            write_reg(SYSCFG_OFFSET_SYSTEM_RESET, 0x1);// Global reset
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
