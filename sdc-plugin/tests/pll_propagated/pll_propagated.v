// Copyright (C) 2020-2021  The SymbiFlow Authors.
//
// Use of this source code is governed by a ISC-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/ISC
//
// SPDX-License-Identifier:ISC

module top (
    input clk,
    input cpu_reset,
    input data_in,
    output [5:0] data_out
);

  wire [5:0] data_out;
  wire builder_pll_fb;
  wire fdce_0_out, fdce_1_out;
  wire main_locked;

  wire clk_ibuf;
  IBUF ibuf_clk(.I(clk), .O(clk_ibuf));

  wire clk_bufg;
  BUFG bufg_clk(.I(clk_ibuf), .O(clk_bufg));

  FDCE FDCE_0 (
      .D  (data_in),
      .C  (clk_bufg),
      .CE (1'b1),
      .CLR(1'b0),
      .Q  (fdce_0_out)
  );

  FDCE FDCE_1 (
      .D  (fdce_0_out),
      .C  (clk_bufg),
      .CE (1'b1),
      .CLR(1'b0),
      .Q  (data_out[0])
  );

  PLLE2_ADV #(
      .CLKFBOUT_MULT(4'd12),
      .CLKIN1_PERIOD(10.0),
      .CLKOUT0_DIVIDE(4'd12),
      .CLKOUT0_PHASE(90.0),
      .CLKOUT1_DIVIDE(2'd3),
      .CLKOUT1_PHASE(0.0),
      .CLKOUT2_DIVIDE(3'd6),
      .CLKOUT2_PHASE(90.0),
      .DIVCLK_DIVIDE(1'd1),
      .REF_JITTER1(0.01),
      .STARTUP_WAIT("FALSE")
  ) PLLE2_ADV (
      .CLKFBIN(builder_pll_fb),
      .CLKIN1(clk),
      .RST(cpu_reset),
      .CLKFBOUT(builder_pll_fb),
      .CLKOUT0(main_clkout0),
      .CLKOUT1(main_clkout1),
      .CLKOUT2(main_clkout2),
      .LOCKED(main_locked)
  );

  wire main_clkout0_bufg;
  wire main_clkout1_bufg;
  wire main_clkout2_bufg;

  BUFG bufg_clkout0 (.I(main_clkout0), .O(main_clkout0_bufg));
  BUFG bufg_clkout1 (.I(main_clkout1), .O(main_clkout1_bufg));
  BUFG bufg_clkout2 (.I(main_clkout2), .O(main_clkout2_bufg));

  FDCE FDCE_PLLx1_PH90 (
      .D  (data_in),
      .C  (main_clkout0_bufg),
      .CE (1'b1),
      .CLR(1'b0),
      .Q  (data_out[1])
  );

  FDCE FDCE_PLLx4_PH0_0 (
      .D  (data_in),
      .C  (main_clkout1_bufg),
      .CE (1'b1),
      .CLR(1'b0),
      .Q  (data_out[2])
  );

  FDCE FDCE_PLLx4_PH0_1 (
      .D  (data_in),
      .C  (main_clkout1_bufg),
      .CE (1'b1),
      .CLR(1'b0),
      .Q  (data_out[3])
  );

  FDCE FDCE_PLLx4_PH0_2 (
      .D  (data_in),
      .C  (main_clkout1_bufg),
      .CE (1'b1),
      .CLR(1'b0),
      .Q  (data_out[4])
  );

  FDCE FDCE_PLLx2_PH90_0 (
      .D  (data_in),
      .C  (main_clkout2_bufg),
      .CE (1'b1),
      .CLR(1'b0),
      .Q  (data_out[5])
  );
endmodule
