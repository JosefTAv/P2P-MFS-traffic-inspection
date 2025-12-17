# FPGA Hardware Design

This directory contains the hardware design source code and build scripts for the P2P-MFS Traffic Inspection system.

## Developer Note: `.bash_custom`

It is assumed .bash_custom is sourced in .bashrc.

## Directory Structure

*   **`scripts/`**: Automation scripts (exported to PATH).
*   **`hw/`**: Hardware source files.
*   **`tools/`**: Contains the `open-nic-shell` submodule and patches.

## Prerequisites

*   **Xilinx Vivado**: Checked and sourced automatically if you use `.bash_custom`.

## Build Instructions

### 1. Environment Setup

Before the first build, setup the environment and patches:

```bash
setup_environment.sh
```

### 2. Building the Bitstream

To build the project for the Alveo U55C:

```bash
build_onic.sh [options]
```

**Common Options:**

*   `--tag=<string>`: Specify a tag for the build output (default: `build`).
*   `--plugin_dir=<path>`: Path to a custom user plugin.

**Example:**
```bash
build_onic.sh --tag=timestamp_build --plugin_dir=../hw/plugins/inject_timestamp
```

## Output

After a successful build, the generated bitstream (`.bit`) and hardware definition files (`.xsa`) will be located in the specified build directory (typically `hw/onic_au55c_build_<tag>`).
