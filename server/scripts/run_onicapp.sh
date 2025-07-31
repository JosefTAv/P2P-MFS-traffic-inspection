#!/bin/bash

# sudo LD_LIBRARY_PATH=$P2P_DIR/server/tools/dpdk-stable/lib/x86_64-linux-gnu \
# $P2P_DIR/server/src/build/onic_app --file-prefix josef \
# -l 176,177,183 -n 2 -a 81:00.0 -a 81:00.1
#
sudo LD_LIBRARY_PATH=$P2P_DIR/server/tools/dpdk-stable/lib/x86_64-linux-gnu \
$P2P_DIR/server/src/build/onic_app \
    --file-prefix josef \
    -l 168,169,170,171,172,173,174,176,177,178,179,180,181,182,183 \
    -n 2 \
    -a 81:00.0 -a 81:00.1 -a c1:00.0 -a c1:00.1 
#81 is onic0, c1 is onic1
