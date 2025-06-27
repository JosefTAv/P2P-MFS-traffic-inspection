// Implementation of a packet filter in the top-level 250 MHz user logic box
// Task -- compare the incoming IP packet to the filter rules, and indicate whether it should be passed or dropped
`timescale 1ns/1ps

module packet_detect #(
    parameter int NUM_CMAC_PORT = 1,
    parameter int NUM_PHYS_FUNC = 1,
    parameter int NUM_QDMA = 1,
    parameter int PORT_ID = 1
) (
    //clock and reset
    input logic clk,
    input logic rst,

    // Packet filter Configuration
    input logic [1:0] rule_config_reg, // Rule configuration register
    input logic [31:0] default_ip_addr, // Default IP address
    input logic [31:0] default_port_num, // Default Port number

    // AXI-stream input from CMAC RX
    input  logic [NUM_CMAC_PORT-1:0]     s_axis_adap_rx_250mhz_tvalid,
    input  logic [512*NUM_CMAC_PORT-1:0] s_axis_adap_rx_250mhz_tdata,
    input  logic [64*NUM_CMAC_PORT-1:0]  s_axis_adap_rx_250mhz_tkeep,
    input  logic [NUM_CMAC_PORT-1:0]     s_axis_adap_rx_250mhz_tlast,
    input   [16*NUM_CMAC_PORT-1:0]       s_axis_adap_rx_250mhz_tuser_size,
    input   [16*NUM_CMAC_PORT-1:0]       s_axis_adap_rx_250mhz_tuser_src,
    input   [16*NUM_CMAC_PORT-1:0]       s_axis_adap_rx_250mhz_tuser_dst,
    output logic [NUM_CMAC_PORT-1:0]     s_axis_adap_rx_250mhz_tready,

    // AXI-stream output to QDMA C2H
    output logic [NUM_PHYS_FUNC*NUM_QDMA-1:0]     m_axis_qdma_c2h_tvalid,
    output logic [512*NUM_PHYS_FUNC*NUM_QDMA-1:0] m_axis_qdma_c2h_tdata,
    output logic [64*NUM_PHYS_FUNC*NUM_QDMA-1:0]  m_axis_qdma_c2h_tkeep,
    output logic [NUM_PHYS_FUNC*NUM_QDMA-1:0]     m_axis_qdma_c2h_tlast,
    output  [16*NUM_PHYS_FUNC*NUM_QDMA-1:0]       m_axis_qdma_c2h_tuser_size,
    output  [16*NUM_PHYS_FUNC*NUM_QDMA-1:0]       m_axis_qdma_c2h_tuser_src,
    output  [16*NUM_PHYS_FUNC*NUM_QDMA-1:0]       m_axis_qdma_c2h_tuser_dst,
    input  logic [NUM_PHYS_FUNC*NUM_QDMA-1:0]     m_axis_qdma_c2h_tready
); 
    // Status register to indicate if a packet is filtered or not (PER PORT)
    // 01 -> IP address match
    // 10 -> Port number match
    // 00 -> No match
    // 11 -> Invalid
    logic [1:0] match_status [NUM_CMAC_PORT-1:0]; 
    logic match [NUM_CMAC_PORT-1:0];

    localparam BYTE = 8;
    localparam ETH_TYPE_OFFSET = 12*BYTE; 
    localparam ETH_TYPE_SIZE = 2*BYTE;
    localparam VLAN_TAG = 0x8100
    
    localparam VLAN_ID_OFFSET = (6+6+2)*BYTE+4; // dest mac + src mac + packet type + VLAN(priority+CFI)[4 bits]
    localparam VLAN_ID_SIZE = 12; 
    localparam VLAN_ID = 12'hABC; // the VLAN we are looking for

    // ------------------ Stream data from CMAC RX to QDMA C2H if rules match ------------------ //
    genvar i;
    generate
        for (i = 0; i < NUM_CMAC_PORT; i++) begin

            // detect start of each packet

            // Extract packet data for each port
            logic [511:0] pkt_data = s_axis_adap_rx_250mhz_tdata[512*i + 511 : 512*i];

            // detect if VLAN packet
            logic [15:0] eth_type  = pkt_data[ETH_TYPE_OFFSET +: ETH_TYPE_SIZE]; 
            
            assign is_VLAN = (eth_type == VLAN_TAG);

            assign sync_detected = is_VLAN && (pkt_data[VLAN_ID_OFFSET +: VLAN_ID_SIZE] == VLAN_ID);






            // IP offset (bytes 30–33)
            logic [31:0]  dst_ip   = pkt_data[143:112];   

            // Port offset (bytes 36–37)
            logic [15:0]  dst_port = pkt_data[175:160];   

            assign match[i] = (rule_config_reg == 2'b01) ? (dst_ip == default_ip_addr) :
                           (rule_config_reg == 2'b10) ? (dst_port == default_port_num) :
                           1'b0;

            assign match_status[i] = (rule_config_reg == 2'b01 && match[i]) ? 2'b01 :
                                (rule_config_reg == 2'b10 && match[i]) ? 2'b10 : 
                                2'b00;

            assign m_axis_qdma_c2h_tvalid[i] = match[i] ? s_axis_adap_rx_250mhz_tvalid[i] : 0;
            assign m_axis_qdma_c2h_tdata[512*i +: 512] = match[i] ? s_axis_adap_rx_250mhz_tdata[512*i +: 512] : 0;
            assign m_axis_qdma_c2h_tkeep[64*i +: 64] = match[i] ? s_axis_adap_rx_250mhz_tkeep[64*i +: 64] : 0;
            assign m_axis_qdma_c2h_tlast[i] = match[i] ? s_axis_adap_rx_250mhz_tlast[i] : 0;
            assign m_axis_qdma_c2h_tuser_size[16*i +: 16] = match[i] ? s_axis_adap_rx_250mhz_tuser_size[16*i +: 16] : 0;
            assign m_axis_qdma_c2h_tuser_src[16*i +: 16] = match[i] ? s_axis_adap_rx_250mhz_tuser_src[16*i +: 16] : 0;
            assign m_axis_qdma_c2h_tuser_dst[16*i +: 16] = match[i] ? s_axis_adap_rx_250mhz_tuser_dst[16*i +: 16] : 0;
            assign s_axis_adap_rx_250mhz_tready[i] = match[i] ? m_axis_qdma_c2h_tready[i] : 0;
        end
    endgenerate


endmodule