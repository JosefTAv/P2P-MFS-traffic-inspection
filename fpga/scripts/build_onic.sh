#!/bin/bash
# ------------------------------------------------------------------
# Summary
# 1. Get user arguments
# 2. Build onic with recommended settings
# ------------------------------------------------------------------
SCRIPT_DIR=$(dirname "$(realpath "${BASH_SOURCE[0]}")")
ONIC_SCRIPT_DIR="$(realpath $SCRIPT_DIR/../tools/open-nic-shell/script)"

# ========================
# 1. Get user arguments
# ========================
TAG=""
BUILD_DIR="../../hw"

for arg in "$@"; do
  case $arg in
    --tag=*)
      TAG="${arg#*=}"
      shift
      ;;
    --build_dir=*)
      BUILD_DIR="${arg#*=}"
      shift
      ;;
    *)
      echo "Unknown argument: $arg"
      exit 1
      ;;
  esac
done

# ========================
# 2. Build onic
# ========================
cd "$ONIC_SCRIPT_DIR" || exit 1
vivado -mode batch -source build.tcl -tclargs -board au55c -tag "$TAG" -build_dir "$BUILD_DIR" -num_phys_func 3 -num_cmac_port 2 -max_pkt_len 9600 -jobs 32
