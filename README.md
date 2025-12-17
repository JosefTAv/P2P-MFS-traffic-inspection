# P2P-MFS Traffic Inspection

High-performance framework for internet traffic analysis and inspection using a hybrid FPGA-CPU architecture. This project leverages the Xilinx Alveo U55C for accelerated packet processing and a DPDK-based software stack.

## Architecture Guidelines

The system consists of two tightly coupled components:
1.  **FPGA Data Plane**: Xilinx Alveo U55C cards running the ONIC shell.
2.  **Server Control Plane**: A DPDK-based C++ application for control and analytics.

## Developer Environment Setup (Recommended)

To streamline development, a custom environment configuration file is provided: `.bash_custom`. It exports necessary environment variables, updates your `PATH`, and sets up useful aliases.

**It is strongly recommended to source this file in your `~/.bashrc`:**

```bash
# Add to your ~/.bashrc
if [ -f /path/to/repo/.bash_custom ]; then
    . /path/to/repo/.bash_custom
fi
```

**Benefits:**
*   **Automatic Path Configuration**: Adds `server/scripts`, `fpga/scripts`, and tools to your `PATH`. You can run scripts like `run_onicapp.sh` or `build_onic.sh` from anywhere.
*   **Environment Variables**: Sets `P2P_DIR`, `LD_LIBRARY_PATH`, and `PKG_CONFIG_PATH` automatically.
*   **Vivado/Vitis Setup**: Automatically sources Xilinx toolchains.
*   **Convenience Aliases**:
    *   `src`, `fpga`, `scripts`: Quickly navigate between directories.
    *   `make_onic`: Compilation shortcut.

## Repository Structure

*   **/fpga**: **Hardware Design** [Read detailed docs](fpga/README.md)
*   **/server**: **Software Application** [Read detailed docs](server/README.md)

## Quick Start Guide

*Assumes `.bash_custom` is sourced.*

### 1. Hardware Setup (FPGA)
```bash
setup_environment.sh # Patch open-nic + build initial bitstream
```

### 2. Software Setup (Server)
```bash
setup_enviroment.sh  # Run once to patch/build DPDK + tools
modprobe_uio.sh      # Load kernel modules
reinit_onic.sh       # Bind to DPDK and reinitialize ONIC
make_onicapp.sh      # Build application
```

### 3. Running the System
```bash
run_onicapp.sh # Runs app with default arguments
```

*For specific configuration details, see the component READMEs.*
