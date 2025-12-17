# Host Server Software

This directory contains the host-side software application that interfaces with the FPGA hardware.

## Developer Note: `.bash_custom`

It is assumed .bash_custom is sourced in the .bashrc.

## Architecture Overview

The application is a high-performance user-space packet inspector built on **DPDK**. It interfaces with the FPGA via the `QDMA` driver.

*   **`src/main.cpp`**: Entry point. initializes EAL, configures the FPGA ports (`Onic`), and launches the forwarding threads. It also runs the main stats loop which prints throughput to the console.
*   **`src/onic.cpp` / `src/onic_port.cpp`**: Abstraction layer for the FPGA hardware, managing port configurations and stats retrieval.
*   **`src/pipeline.cpp`**: Defines the packet processing pipeline logic used by the software forwarder threads.

## Prerequisites & Setup

The `scripts/setup_enviroment.sh` script handles the complex dependency setup. It performs the following:

1.  **Patches Drivers**: Applies custom patches to the `dma_ip_drivers` and `dpdk-stable` source trees.
2.  **Builds DPDK**: Compiles a local version of DPDK with the patched QDMA driver.
3.  **Kernel Modules**: Compiles and installs the `igb_uio` driver and `pcimem` tool.

**Initial Setup Command:**
```bash
setup_enviroment.sh
```

## Usage

### Building

You can use the provided script or the convenient alias:

```bash
make_onicapp.sh
# or alias
make_onic
```

### Running the Application

The `run_onicapp.sh` script launches the application with the recommended DPDK Environment Abstraction Layer (EAL) arguments.

```bash
run_onicapp.sh 
```

**What it does:**
*   Sets `LD_LIBRARY_PATH` to the local DPDK build.
*   Configures CPU core affinity (`-l`) to isolate the forwarding threads.
*   Whitelists the FPGA PCIe addresses (`-a 81:00.0`, etc).

### Runtime Output

Once running, the application will print periodic throughput statistics to the console:

```text
[Onic0, port1 -> Onic0, port1] (RX) Throughput: 14.88 Mpps, 9523.20 Mbps, Drop: 0.00 % (0 pkts)
[Onic0, port1 -> Onic0, port1] (TX) Throughput: 14.88 Mpps, 9523.20 Mbps, Drop: 0.00 % (0 pkts)
```

*   **RX/TX Throughput**: Current packet and bit rates.
*   **Drop**: Packet loss percentage and count (useful for debugging PCIe backpressure or software bottlenecks).

### Kafka Integration
The application has `librdkafka` linked for reporting statistics or events to a Kafka broker. This is configured in `main.cpp` (disabled by default in some configurations).

## Traffic Simulator (`src_clk_sync`)

To run the clock synchronization traffic generator:

```bash
run_clk_sync_app.sh
```
