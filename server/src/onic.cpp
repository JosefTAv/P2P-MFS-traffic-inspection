/*
 * Copyright (c) 2020 Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#include "onic.h"

int Onic::ONIC_LOG_TYPE = rte_log_register("Onic");

int Onic::init_hardware(){
    // Init Queue config regs
    write_reg(0x1000, 0x1);
    write_reg(0x2000, 0x00010001);

    /* get the number of CMAC instances */
    while ((read_reg(SYSCFG_OFFSET_SHELL_STATUS) & 0x10) != 0x10);
    int i;
    for (i = 0; i < NB_CMAC; ++i) {
        int shell_idx = (0x10 << (4*i));

        write_reg(SYSCFG_OFFSET_SHELL_RESET, shell_idx);
		while ((read_reg(SYSCFG_OFFSET_SHELL_STATUS) & shell_idx) != shell_idx) {
            rte_delay_ms(CMAC_RESET_WAIT_MS);
        }

        uint32_t val = read_reg(CMAC_OFFSET_CORE_VERSION(i));
        if (val != ONIC_CMAC_CORE_VERSION)
            break;
        enable_cmac(i);
    }
    onic_log(RTE_LOG_INFO, "Number of CMAC instances enabled = %d\n", i);
    // rte_pmd_qdma_dbg_regdump()
    return 0;

}

int Onic::enable_cmac(int cmac_id){

    if (cmac_id == 0) {
        write_reg(SYSCFG_OFFSET_SHELL_RESET, 0x10);
        while ((read_reg(SYSCFG_OFFSET_SHELL_STATUS) & 0x10) != 0x10)
            rte_delay_ms(CMAC_RESET_WAIT_MS);
    } else {
        write_reg(SYSCFG_OFFSET_SHELL_RESET, 0x100);
        while ((read_reg(SYSCFG_OFFSET_SHELL_STATUS) & 0x100) != 0x100)
            rte_delay_ms(CMAC_RESET_WAIT_MS);
    }
    if (RS_FEC) {
        /* Enable RS-FEC for CMACs with RS-FEC implemented */
        write_reg(CMAC_OFFSET_RSFEC_CONF_ENABLE(cmac_id), 0x3);
        write_reg(CMAC_OFFSET_RSFEC_CONF_IND_CORRECTION(cmac_id), 0x7);
    }

    write_reg(CMAC_OFFSET_CONF_RX_1(cmac_id), 0x1);
    write_reg(CMAC_OFFSET_CONF_TX_1(cmac_id), 0x10);

    write_reg(CMAC_OFFSET_CONF_TX_1(cmac_id), 0x1);

    // /* RX flow control */
    // write_reg(CMAC_OFFSET_CONF_RX_FC_CTRL_1(cmac_id), 0x00003DFF);
    // write_reg(CMAC_OFFSET_CONF_RX_FC_CTRL_2(cmac_id), 0x0001C631);

    // /* TX flow control */
    // write_reg(CMAC_OFFSET_CONF_TX_FC_QNTA_1(cmac_id), 0xFFFFFFFF);
    // write_reg(CMAC_OFFSET_CONF_TX_FC_QNTA_2(cmac_id), 0xFFFFFFFF);
    // write_reg(CMAC_OFFSET_CONF_TX_FC_QNTA_3(cmac_id), 0xFFFFFFFF);
    // write_reg(CMAC_OFFSET_CONF_TX_FC_QNTA_4(cmac_id), 0xFFFFFFFF);
    // write_reg(CMAC_OFFSET_CONF_TX_FC_QNTA_5(cmac_id), 0x0000FFFF);
    // write_reg(CMAC_OFFSET_CONF_TX_FC_RFRH_1(cmac_id), 0xFFFFFFFF);
    // write_reg(CMAC_OFFSET_CONF_TX_FC_RFRH_2(cmac_id), 0xFFFFFFFF);
    // write_reg(CMAC_OFFSET_CONF_TX_FC_RFRH_3(cmac_id), 0xFFFFFFFF);
    // write_reg(CMAC_OFFSET_CONF_TX_FC_RFRH_4(cmac_id), 0xFFFFFFFF);
    // write_reg(CMAC_OFFSET_CONF_TX_FC_RFRH_5(cmac_id), 0x0000FFFF);
    // write_reg(CMAC_OFFSET_CONF_TX_FC_CTRL_1(cmac_id), 0x000001FF);

    printf("Successfully setup cmac id: %d\n", cmac_id);
    return 0;
}

CmacStats Onic::get_cmac_stats(int cmac_id){
    CmacStats stats;
    write_reg(CMAC_OFFSET_TICK(cmac_id), 0b1); // push accunmulated stats to regs

    stats.tx_total_pkts = read_reg(CMAC_OFFSET_STAT_TX_TOTAL_PKTS(cmac_id));
    stats.tx_total_good_pkts= read_reg(CMAC_OFFSET_STAT_TX_TOTAL_GOOD_PKTS(cmac_id));
    stats.tx_total_bytes = read_reg(CMAC_OFFSET_STAT_TX_TOTAL_BYTES(cmac_id));
    stats.tx_total_good_bytes = read_reg(CMAC_OFFSET_STAT_TX_TOTAL_GOOD_BYTES(cmac_id));

    stats.rx_total_pkts = read_reg(CMAC_OFFSET_STAT_RX_TOTAL_PKTS(cmac_id));
    stats.rx_total_good_pkts= read_reg(CMAC_OFFSET_STAT_RX_TOTAL_GOOD_PKTS(cmac_id));
    stats.rx_total_bytes = read_reg(CMAC_OFFSET_STAT_RX_TOTAL_BYTES(cmac_id));
    stats.rx_total_good_bytes = read_reg(CMAC_OFFSET_STAT_RX_TOTAL_GOOD_BYTES(cmac_id));
    // onic_log(RTE_LOG_INFO, "Read cmac id %d stats \n", cmac_id);

    return stats;
}

const std::array<OnicPort, NB_PORTS>& Onic::get_ports() const {
    return ports;
}

std::array<int, NB_PORTS> Onic::get_port_ids() const {
    std::array<int, NB_PORTS> port_ids = {-1};
    for(int i=0; i<NB_PORTS; i++)
        port_ids[i] = ports[i].get_port_id();
    return port_ids;
}

void Onic::onic_log(uint32_t level, const char *format, ...)
{
    char buffer[256];
    va_list args;

    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len > 0) {
        rte_log(level, ONIC_LOG_TYPE, "ONIC: %s", buffer);
    }
}
