`timescale 1ns / 1ps

module timestamp_inject #(
    parameter int NUM_AXI_STREAM = 1
) (
    // AXI-stream input
    input logic [    NUM_AXI_STREAM-1:0] i_axis_tvalid,
    input logic [512*NUM_AXI_STREAM-1:0] i_axis_tdata,
    input logic [ 64*NUM_AXI_STREAM-1:0] i_axis_tkeep,
    input logic [    NUM_AXI_STREAM-1:0] i_axis_tlast,
    input       [ 16*NUM_AXI_STREAM-1:0] i_axis_tuser,
    input logic [    NUM_AXI_STREAM-1:0] i_axis_tready,

    // AXI-stream output
    output logic [512*NUM_AXI_STREAM-1:0] o_axis_tdata,

    input logic [31:0] i_nb_sync,
    input logic [31:0] i_curr_tick,
    output logic o_sync_detected,

    input axil_aclk,
    input axis_aclk,
    input axil_aresetn
);
  localparam BYTE = 8;

  localparam PAYLOAD_OFFSET = (6+6+2+2+2)*BYTE; // dest mac + src mac + packet type + VLAN + Ethertype
  localparam TIMESTAMP_SIZE = 32 + 32;  // nb_sync + curr_tick
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
      assign o_sync_detected = sync_detected;

      logic found_timestamp_idx;
      logic [$clog2(512)-1:0] timestamp_insert_idx;
      always_comb begin : get_timestamp_idx
        int j;  // offset where we insert our timestamp
        found_timestamp_idx  = 0;
        timestamp_insert_idx = '0;

        for (
            j = PAYLOAD_OFFSET; j < 512 - TIMESTAMP_SIZE; j += TIMESTAMP_SIZE
        ) begin  //check where to insert timestamp: increment
          if (!found_timestamp_idx) begin
            if (i_axis_tdata[`getbit(512, i, j)+:$bits(EMPTY_TIMESTAMP)] != EMPTY_TIMESTAMP) begin
              found_timestamp_idx  = 1;
              timestamp_insert_idx = `getbit(512, i, j);
            end
          end
        end
      end : get_timestamp_idx

      always_comb begin : insert_timestamp
        o_axis_tdata = i_axis_tdata;

        if (sync_detected) begin
          // o_axis_tdata[timestamp_insert_idx-:TIMESTAMP_SIZE] = {i_nb_sync[31:0], i_curr_tick};
          o_axis_tdata[timestamp_insert_idx+:TIMESTAMP_SIZE] = {31'hABCD, i_curr_tick};
        end
      end : insert_timestamp

    end
  endgenerate


endmodule
