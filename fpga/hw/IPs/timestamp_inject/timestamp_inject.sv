`timescale 1ns / 1ps

module timestamp_inject #(
    parameter int NUM_AXI_STREAM = 1
) (
    // AXI-lite interface
    input         s_axil_awvalid,
    input  [31:0] s_axil_awaddr,
    output        s_axil_awready,
    input         s_axil_wvalid,
    input  [31:0] s_axil_wdata,
    output        s_axil_wready,
    output        s_axil_bvalid,
    output [ 1:0] s_axil_bresp,
    input         s_axil_bready,
    input         s_axil_arvalid,
    input  [31:0] s_axil_araddr,
    output        s_axil_arready,
    output        s_axil_rvalid,
    output [31:0] s_axil_rdata,
    output [ 1:0] s_axil_rresp,
    input         s_axil_rready,

    // AXI-stream input
    input logic [    NUM_AXI_STREAM-1:0] i_axis_tvalid,
    input logic [512*NUM_AXI_STREAM-1:0] i_axis_tdata,
    input logic [ 64*NUM_AXI_STREAM-1:0] i_axis_tkeep,
    input logic [    NUM_AXI_STREAM-1:0] i_axis_tlast,
    input       [ 16*NUM_AXI_STREAM-1:0] i_axis_tuser,
    input logic [    NUM_AXI_STREAM-1:0] i_axis_tready,

    // AXI-stream output
    output logic [512*NUM_AXI_STREAM-1:0] o_axis_tdata,

    input axil_aclk,
    input axis_aclk,
    input axil_aresetn
);
  localparam BYTE = 8;

  localparam PAYLOAD_OFFSET = (6+6+2+2+2)*BYTE; // dest mac + src mac + packet type + VLAN + Ethertype
  localparam TIMESTAMP_SIZE = 32 + 64;  // nb_sync + curr_tick
  localparam EMPTY_TIMESTAMP = 32'hDEADBEEF;

  // ------------------ Inject timestamp at next available offset if vlan detected ------------------ //
  genvar i;
  generate
    for (i = 0; i < NUM_AXI_STREAM; i++) begin

      logic sync_detected;
      packet_detect #(
          .NUM_AXI_STREAM(NUM_AXI_STREAM)
      ) packet_detect_inst (
          .s_axis_tvalid(i_axis_tvalid[i]),
          .s_axis_tdata (i_axis_tdata[`getvec(512, i)]),
          .s_axis_tkeep (i_axis_tkeep[`getvec(64, i)]),
          .s_axis_tlast (i_axis_tlast[i]),
          .s_axis_tuser (i_axis_tuser),
          .s_axis_tready(i_axis_tready[i]),

          .aclk   (axis_aclk),
          .aresetn(axil_aresetn),

          .sync_detected_o(sync_detected)
      );

      logic [63:0] nb_sync;
      logic [63:0] curr_tick;
      clk_sync_pulse clk_sync_pulse_inst (
          .s_axil_awvalid(s_axil_awvalid),
          .s_axil_awaddr (s_axil_awaddr),
          .s_axil_awready(s_axil_awready),
          .s_axil_wvalid (s_axil_wvalid),
          .s_axil_wdata  (s_axil_wdata),
          .s_axil_wready (s_axil_wready),
          .s_axil_bvalid (s_axil_bvalid),
          .s_axil_bresp  (s_axil_bresp),
          .s_axil_bready (s_axil_bready),
          .s_axil_arvalid(s_axil_arvalid),
          .s_axil_araddr (s_axil_araddr),
          .s_axil_arready(s_axil_arready),
          .s_axil_rvalid (s_axil_rvalid),
          .s_axil_rdata  (s_axil_rdata),
          .s_axil_rresp  (s_axil_rresp),
          .s_axil_rready (s_axil_rready),

          .sync_pulse_i(0),
          .sync_pulse_o(),
          .nb_sync_o(nb_sync),
          .curr_tick_o(curr_tick),

          .axil_aclk(axil_aclk),
          .axis_aclk(axis_aclk),
          .axil_aresetn(axil_aresetn)
      );

      logic found_timestamp_idx;
      logic [8:0] timestamp_insert_idx;
      always_comb begin : get_timestamp_idx
        int j;
        found_timestamp_idx  = 0;
        timestamp_insert_idx = '0;

        for (
            j = PAYLOAD_OFFSET; j < 512 - TIMESTAMP_SIZE; j += TIMESTAMP_SIZE
        ) begin  //check where to insert timestamp
          if (!found_timestamp_idx) begin
            if (i_axis_tdata[512*(i+1)-1-j-:32] != EMPTY_TIMESTAMP) begin
              found_timestamp_idx  = 1;
              timestamp_insert_idx = 512 * i - j;
            end
          end
        end
      end : get_timestamp_idx

      always_comb begin : insert_timestamp
        o_axis_tdata = i_axis_tdata;

        if (sync_detected) begin
          // o_axis_tdata[timestamp_insert_idx-:TIMESTAMP_SIZE] = {nb_sync[31:0], curr_tick};
          o_axis_tdata[timestamp_insert_idx-:TIMESTAMP_SIZE] = {31'hABCD, curr_tick};
        end
      end : insert_timestamp

    end
  endgenerate


endmodule
