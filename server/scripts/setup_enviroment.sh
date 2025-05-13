#!/bin/bash
# ------------------------------------------------------------------
# Summary
# 1. Apply patches to dma_ip_drivers and DPDK
# 2. Add DPDK-kmods
# 3. Add QDMA to DPDK
# 4. Build DPDK
# 5. Make igb_uio driver
# 6. Make pcimem
# ------------------------------------------------------------------

# Get the directory where this script lives
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PATCHES_PATH="$SCRIPT_DIR/../tools/patches"

print_log() {
    echo ""
    echo "@1"
    echo ""
}

# ========================
# 1. Apply patches
# ========================
### 1. Apply first patch to DPDK
DPDK_PATH="$SCRIPT_DIR/../tools/dpdk-stable"
DPDK_PATCH="0001-DPDK-add-QDMA-to-drivers.patch"

# Copy and apply patch
cp "$PATCHES_PATH/$DPDK_PATCH" "$DPDK_PATH/"
cd "$DPDK_PATH" || exit 1
git apply --verbose "$DPDK_PATCH"

### 2. Apply second patch to QDMA DPDK
QDMA_PATH="$SCRIPT_DIR/../tools/dma_ip_drivers/QDMA/DPDK"
QDMA_PATCH="0002-QDMA-make-Onic-configs.patch"

# Copy and apply patch
cp "$PATCHES_PATH/$QDMA_PATCH" "$QDMA_PATH/"
cd "$QDMA_PATH" || exit 1
git apply --verbose "$QDMA_PATCH"
print_log "Finished applying patches to tools"

# ========================
# 2. Add DPDK-kmods
# ========================
cd "$DPDK_PATH" || exit 1
git clone git://dpdk.org/dpdk-kmods
print_log "Added DPDK-kmods"

# ========================
# 3. Add QDMA to DPDK
# ========================
cp -r "$QDMA_PATH"/drivers/net/qdma "$DPDK_PATH"/drivers/net/
cp -r "$QDMA_PATH"/examples/qdma_testapp "$DPDK_PATH"/examples/
sleep 5
# ========================
# 4. Build DPDK
# ========================
cd "$DPDK_PATH" || exit 1
meson setup --prefix="$DPDK_PATH" build
cd build
ninja
ninja install
#SKIP ldconfig (not needed for local installs)
print_log "Finished building DPDK"

# ========================
# 5. Make igb_uio driver
# ========================
cd "$DPDK_PATH"/dpdk-kmods/linux/igb_uio || exit 1
make
print_log "Made igb_uio driver"


# ========================
# 6. make pciemem
# ========================
PCIMEM_PATH="$SCRIPT_DIR/../tools/pcimem"
cd "$PCIMEM_PATH"/ || exit 1
make
print_log "Made pcimem"
