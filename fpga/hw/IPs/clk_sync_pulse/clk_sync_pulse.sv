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

`timescale 1ns / 1ns
module clk_sync_pulse (
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

    // Synchronized to axis_aclk
    input         sync_pulse_i,
    output        sync_pulse_o,
    output [63:0] nb_sync_o,
    output [63:0] curr_tick_o,

    input axil_aclk,
    input axis_aclk,
    input axil_aresetn
);

  wire incr_nb_sync_w;
  wire incr_curr_tick_w;
  wire sync_period_detect_w;
  wire master_mode_w;

  clk_sync_pulse_regs sync_regs_inst (
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

      .axil_aclk   (axil_aclk),
      .axis_aclk   (axis_aclk),
      .axil_aresetn(axil_aresetn),

      // synchronised with axis_aclk
      .master_mode_o       (master_mode_w),
      .sync_period_detect_o(sync_period_detect_w),

      // synchronised with axis_aclk
      .incr_nb_sync_i  (incr_nb_sync_w),
      .incr_curr_tick_i(incr_curr_tick_w),
      .nb_sync_o       (nb_sync_o),
      .curr_tick_o     (curr_tick_o)
  );


  // If master mode
  // - incr_nb_sync high every clock tick -> incr_nb_sync_i = master_mode
  // - sync_pulse_o goes high every time (curr_tick == sync_period): overflow, etc is taken care of by regs

  // If slave mode
  // - everytime sync_pulse_i goes high
  //    - increment nb_sync
  //    - reset curr_tick
  //    - ignore sync_period?

  // ----------- MASTER MODE -----------
  assign incr_curr_tick_w = master_mode_w || incr_nb_sync_w; // Increment every tick, extra logic may be added
  assign sync_pulse_o = master_mode_w && sync_period_detect_w; // master_mode: send out pulse every period

  // ----------- SLAVE MODE -----------
  assign incr_nb_sync_w = (~master_mode_w) && sync_pulse_i; // slave_mode: only increment when master pulses

endmodule : clk_sync_pulse
