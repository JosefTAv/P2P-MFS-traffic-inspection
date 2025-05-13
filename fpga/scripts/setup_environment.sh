#!/bin/bash
# ------------------------------------------------------------------
# Summary
# 1. Apply patch to onic
# 2. Build onic with default tag
# ------------------------------------------------------------------
SCRIPT_DIR=$(dirname "$(realpath "${BASH_SOURCE[0]}")")
TOOLS_DIR="$(realpath $SCRIPT_DIR/../tools/)"

# ========================
# 1. Apply patch to onic
# ========================
PATCH_FILE="build_script_patch.diff"
ONIC_PATH="$TOOLS_DIR/open-nic-shell/"
cp "$TOOLS_DIR/patches/$PATCH_FILE" "$ONIC_PATH"

cd "$ONIC_PATH" || exit 1
git apply --verbose "$PATCH_FILE"

# ========================
# 2. Build onic with default tag
# ========================
cd "$SCRIPT_DIR" || exit 1
./build_onic.sh --tag="build" --build_dir="../../hw" #the arguments are redundant, they just show proper usage
