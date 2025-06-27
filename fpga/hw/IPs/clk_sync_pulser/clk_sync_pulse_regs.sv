// *************************************************************************
//
// Copyright 2020 Xilinx, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// *************************************************************************
// Address range:
//   - 0x3000 - 0x3FFF (CMAC0)
//   - 0x7000 - 0x7FFF (CMAC1)
// Address width: 12-bit
//
// Register description
// -----------------------------------------------------------------------------
//  Address | Mode |  Description
// -----------------------------------------------------------------------------
//   0x000  |  RW  |  Master mode: when '1' it will generate a pulse and not expect one
// -----------------------------------------------------------------------------
//   0x004  |  RW  |  Reset period: how many ticks the block will consider a reset period
//   0x008  |      |
// -----------------------------------------------------------------------------
//   0x00C  |  RO  |  Number of resets that have occurred
//   0x010  |      |
// -----------------------------------------------------------------------------
//   0x014  |  RO  |  Current amount of clk ticks since last reset
//   0x018  |      |
// -----------------------------------------------------------------------------

`timescale 1ns/1ps

typedef enum logic { SLAVE, MASTER } mode_t;

module clk_sync_pulse_regs #(
    parameter mode_t DEFAULT_MODE = MASTER, // in master mode by default
    parameter int DEFAULT_PERIOD = 64'd322 //Nb ticks per period according to axis_clk
) (
  input         s_axil_awvalid,
  input  [31:0] s_axil_awaddr,
  output        s_axil_awready,
  input         s_axil_wvalid,
  input  [31:0] s_axil_wdata,
  output        s_axil_wready,
  output        s_axil_bvalid,
  output  [1:0] s_axil_bresp,
  input         s_axil_bready,
  input         s_axil_arvalid,
  input  [31:0] s_axil_araddr,
  output        s_axil_arready,
  output        s_axil_rvalid,
  output [31:0] s_axil_rdata,
  output  [1:0] s_axil_rresp,
  input         s_axil_rready,

  input         axil_aclk,
  input         axis_aclk,
  input         axil_aresetn,

  // synchronised with axis_aclk
  output        master_mode_o,
  output        sync_period_detect_o,

  // synchronised with axis_aclk
  input         incr_nb_sync_i,
  input         incr_curr_tick_i,
  output [63:0] nb_sync_o,
  output [63:0] curr_tick_o
);

  localparam C_ADDR_W = 12;

  // Register address
  localparam MASTER_MODE  = 12'h000;

  localparam SYNC_PERIOD_ADDR_UPPER  = 12'h004;
  localparam SYNC_PERIOD_ADDR_LOWER = 12'h008;

  localparam NB_SYNC_UPPER = 12'h00C;
  localparam NB_SYNC_LOWER  = 12'h010;

  localparam CURR_TICK_UPPER  = 12'h014;
  localparam CURR_TICK_LOWER = 12'h018;


  mode_t              reg_master_mode;
  reg          [63:0] reg_sync_period;
  reg          [63:0] reg_nb_sync;
  reg          [63:0] reg_curr_tick;

  wire                sync_period_detect_w;
  wire                master_sync_period_detect_w;

  wire                reg_en;
  wire                reg_we;
  wire [C_ADDR_W-1:0] reg_addr;
  wire         [31:0] reg_din;
  reg          [31:0] reg_dout;

  axi_lite_register #(
    .CLOCKING_MODE ("common_clock"),
    .ADDR_W        (C_ADDR_W),
    .DATA_W        (32)
  ) axil_reg_inst (
    .s_axil_awvalid (s_axil_awvalid),
    .s_axil_awaddr  (s_axil_awaddr),
    .s_axil_awready (s_axil_awready),
    .s_axil_wvalid  (s_axil_wvalid),
    .s_axil_wdata   (s_axil_wdata),
    .s_axil_wready  (s_axil_wready),
    .s_axil_bvalid  (s_axil_bvalid),
    .s_axil_bresp   (s_axil_bresp),
    .s_axil_bready  (s_axil_bready),
    .s_axil_arvalid (s_axil_arvalid),
    .s_axil_araddr  (s_axil_araddr),
    .s_axil_arready (s_axil_arready),
    .s_axil_rvalid  (s_axil_rvalid),
    .s_axil_rdata   (s_axil_rdata),
    .s_axil_rresp   (s_axil_rresp),
    .s_axil_rready  (s_axil_rready),

    .reg_en         (reg_en),
    .reg_we         (reg_we),
    .reg_addr       (reg_addr),
    .reg_din        (reg_din),
    .reg_dout       (reg_dout),

    .axil_aclk      (axil_aclk),
    .axil_aresetn   (axil_aresetn),
    .reg_clk        (axil_aclk),
    .reg_rstn       (axil_aresetn)
  );

// ---------------------------------------------------
// Axil Read logic
// ---------------------------------------------------
  always_ff @(posedge axil_aclk) begin
    if (~axil_aresetn) begin
      reg_dout <= 0;
    end
    else if (reg_en && ~reg_we) begin
      case (reg_addr)
        MASTER_MODE: begin
          reg_dout <= {31'b0, reg_master_mode};
        end
        SYNC_PERIOD_ADDR_UPPER: begin
          reg_dout <= reg_sync_period[63:32];
        end
        SYNC_PERIOD_ADDR_LOWER: begin
          reg_dout <= reg_sync_period[31:0];
        end
        NB_SYNC_UPPER: begin
          reg_dout <= reg_nb_sync[63:32];
        end
        NB_SYNC_LOWER: begin
          reg_dout <= reg_nb_sync[31:0];
        end
        CURR_TICK_UPPER: begin
          reg_dout <= reg_curr_tick[63:32];
        end
        CURR_TICK_LOWER: begin
          reg_dout <= reg_curr_tick[31:0];
        end
        default: begin
          reg_dout <= 32'hDEADBEEF;
        end
      endcase
    end
  end

// ---------------------------------------------------
// Axil Write logic
// ---------------------------------------------------
  always_ff @(posedge axil_aclk) begin
    if (~axil_aresetn) begin
      reg_master_mode <= DEFAULT_MODE;
      reg_sync_period <= DEFAULT_PERIOD - 1;
    end else if (reg_en && reg_we) begin
      case (reg_addr)
        MASTER_MODE: begin
          reg_master_mode <= mode_t'(reg_din[0]);
        end
        SYNC_PERIOD_ADDR_UPPER: begin
          reg_sync_period[63:32] <= reg_din;
        end
        SYNC_PERIOD_ADDR_LOWER: begin
          reg_sync_period[31:0] <= reg_din;
        end
        default: begin
        end
      endcase
    end
  end

  // nb_sync
  always @(posedge axis_aclk) begin
    if (~axil_aresetn) begin
      reg_nb_sync  <= '0;
    end
    else if (incr_nb_sync_i || sync_period_detect_w) begin
      reg_nb_sync  <= reg_nb_sync + 1;
    end
  end

  // curr_tick
  always @(posedge axis_aclk) begin
    if (~axil_aresetn) begin
      reg_curr_tick <= '0;
    end
    else if (incr_nb_sync_i) begin // slave: reset curr_tick manually
      reg_curr_tick <= '0;
    end
    else if (incr_curr_tick_i) begin
      reg_curr_tick <= reg_curr_tick + 1;
      if (master_sync_period_detect_w) begin // master: period has been reached
        reg_curr_tick <= '0;
      end
    end
  end

  // detecting reaching period as master
  assign master_sync_period_detect_w = reg_master_mode && (reg_curr_tick == reg_sync_period); 
  assign sync_period_detect_w = master_sync_period_detect_w && incr_curr_tick_i;

  // connect regs to outputs
  assign master_mode_o = reg_master_mode;
  assign sync_period_detect_o = sync_period_detect_w;
  assign nb_sync_o = reg_nb_sync;
  assign curr_tick_o = reg_curr_tick;

endmodule: clk_sync_pulse_regs
