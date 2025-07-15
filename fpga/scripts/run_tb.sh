#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

usage(){
    echo -e "${RED}Usage: $script_name${NC} ${YELLOW}(This script assumes there is a file list in the same dir as tb_path called [file_list.f])${NC}"
    echo $'\t\t --tb_path \t <path to testbench.cpp>; the directory of this file should contain a file_list.f'
    echo $'\t\t --top \t\t <name of top-module>; top DUT'
    echo $'This script assumes there is a file list in the same dir as tb_path called file_list.f'
    exit 1
}

parse_args() {
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --top)
                top="$2"
                shift 2
                ;;
            --tb_path)
                tb_path="$2"
                shift 2
                ;;
            *)
                echo "Unknown argument: $1"
                shift
                ;;
        esac
    done

    # Check if args are given
    if [[ -z "$tb_path" || -z "$top" ]]; then
        usage
    fi
    # Check if file exists
    if [[ ! -f "$tb_path" ]]; then
        echo "Error: '$tb_path' is not a directory"
        exit 1
    fi
    tb_dir=$(dirname "$tb_path")
    ip_dir=$tb_dr/..
    
    echo -e ''${GREEN}'Testbench:'${NC}' \t '$(basename $tb_dir)''
    echo -e ''${GREEN}'Top:'${NC}' \t\t '$top''
    echo -e ''${GREEN}'Project dir:'${NC}' \t '$ip_dir''
    echo
}

verilate(){
    cd "$tb_dir" || { echo "Failed to cd to $tb_dir"; return 1; }
    echo -e "${GREEN}VERLIATING $top${NC}"
    verilator -f "$tb_dir/file_list.f"  \
        --exe "$tb_path"                \
        -Wall                           \
        -Wno-fatal                      \
        -j 0                            \
        --assert                        \
        --timing                        \
        --trace-fst                     \
        --trace-structs                 \
        --main-top-name "-"             \
        --x-assign unique               \
        --x-initial unique              \
        --top-module "$top"             \
        --trace --sv                    \
        --build                         

        # -Wall                         # Strict warnings
        # -Wno-fatal                    # Don't exit on warnings
        # -j 0                          # Fully parallelized
        # --assert                      # enable SystemVerilog assertions
        # --timing                      # enable timing constructs
        # --trace-fst                   # dump as FST (compressed version of VCD)
        # --trace-structs               # dump structs as human-readable format
        # --main-top-name "-"           # remove extra TOP module    
        # --x-assign unique             # all explicit Xs are replaced by a constant value determined at runtime
        # --x-initial unique            # all variables are randomly initialized (if +verilator+rand+reset+2)
        # --top-module clk_sync_pulse 
        # --trace --sv                  # enable waveform tracing and systemverilog
        # --build                       # Make the executable directly
    ret=$?
    echo
    return $ret
}

run_sim(){
    found_exec=false
    for file in "$tb_dir"/obj_dir/*; do
        if [[ -x "$file" && -f "$file" ]]; then
            found_exec=true
            echo -e "${GREEN}Running ./$(basename "$file") ${NC}"
            "$file"
        fi
    done
    if [[ ! $found_exec ]]; then 
        echo -e "${RED}No executable found: did verilator compile properly?${NC}"
    fi
}

# ========================
# MAIN
# ========================
script_name=$(basename "$0")
parse_args $@
verilate || exit 1
run_sim