// Implementation of a packet filter in the top-level 250 MHz user logic box
// Task -- compare the incoming IP packet to the filter rules, and indicate whether it should be passed or dropped
`timescale 1ns / 1ps

module packet_detect #(
    parameter int NUM_AXI_STREAM = 1
) (
    // AXI-stream input
    input logic [    NUM_AXI_STREAM-1:0] s_axis_tvalid,
    input logic [512*NUM_AXI_STREAM-1:0] s_axis_tdata,
    input logic [ 64*NUM_AXI_STREAM-1:0] s_axis_tkeep,
    input logic [    NUM_AXI_STREAM-1:0] s_axis_tlast,
    input       [ 16*NUM_AXI_STREAM-1:0] s_axis_tuser,
    input logic [    NUM_AXI_STREAM-1:0] s_axis_tready,

    //clock and reset
    input logic aclk,
    input logic aresetn,

    output logic sync_detected_o
);
  localparam BYTE = 8;
  localparam ETH_TYPE_OFFSET = 12 * BYTE;
  localparam ETH_TYPE_SIZE = 2 * BYTE;
  localparam VLAN_TAG = 16'h8100;

  localparam VLAN_ID_OFFSET = (6+6+2)*BYTE+4; // dest mac + src mac + packet type + VLAN(priority+CFI)[4 bits]
  localparam VLAN_ID_SIZE = 12;
  localparam VLAN_ID = 12'hABC;  // the VLAN we are looking for

  // ------------------ Signal clock sync if VLAN packet with VLAD ID == VLAN_TAG ------------------ //
  genvar i;
  generate
    for (i = 0; i < NUM_AXI_STREAM; i++) begin

      // detect start of each packet
      logic in_packet;

      always_ff @(posedge aclk or negedge aresetn) begin
        if (!aresetn) in_packet <= 0;
        else if (s_axis_tvalid && s_axis_tready) begin
          if (s_axis_tlast) in_packet <= 0;
          else if (!in_packet) in_packet <= 1;
        end
      end

      // First beat detection
      logic first_beat;
      assign first_beat = (s_axis_tvalid && s_axis_tready && !in_packet);

      // Extract packet data for each port
      logic [511:0] pkt_data;
      assign pkt_data = s_axis_tdata[`getvec(512, i)];

      // detect if VLAN packet
      logic [15:0] eth_type;
      assign eth_type = pkt_data[ETH_TYPE_OFFSET+:ETH_TYPE_SIZE];

      logic is_VLAN;
      logic is_correct_VLAN;
      assign is_VLAN = (eth_type == VLAN_TAG);
      assign is_correct_VLAN = (pkt_data[VLAN_ID_OFFSET+:VLAN_ID_SIZE] == VLAN_ID);
      assign sync_detected_o = first_beat && is_VLAN && is_correct_VLAN;

    end
  endgenerate


endmodule
