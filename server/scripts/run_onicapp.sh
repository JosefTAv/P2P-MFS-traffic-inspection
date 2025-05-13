sudo LD_LIBRARY_PATH=$P2P_DIR/server/tools/dpdk-stable/lib/x86_64-linux-gnu \
$P2P_DIR/server/src/build/onic_app --file-prefix josef1 \
-l 176,177,183 -n 1 -a 81:00.0 -a 81:00.1
#
# sudo LD_LIBRARY_PATH=../tools/dpdk-stable/lib/x86_64-linux-gnu \
# ./../src/build/onic_app --file-prefix josef1 \
# -c 0xf -n 4 -a c1:00.0 -a c1:00.1 -a 81:00.0 -a 81:00.1
