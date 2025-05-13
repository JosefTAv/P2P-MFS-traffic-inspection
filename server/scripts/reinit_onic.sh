#!/bin/bash
# ------------------------------------------------------------------
# Summary
# 1. Unbind OpenNic (each PCIe function must be input manually)
# 2. Setup OpenNic registers for usage as nic
# 3. Rebind OpeNic for usage
# ------------------------------------------------------------------
# Get the directory where this script lives
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ "$#" -lt 1 ]; then
    	echo "Usage: $0 [list of]<bb:dd:ff>"
    	echo "E.g. $0 c1:00.0 c1:00.1"
	echo "Only do one FPGA at a time"
	exit 1
fi

# ========================
# 1. Unbind OpenNic
# ========================
unbind_onic() {
	DPDK_PATH="$SCRIPT_DIR/../tools/dpdk-stable"
	for pci_adr in "$@"; do # remove all devices
		sudo "$DPDK_PATH"/usertools/dpdk-devbind.py -u $pci_adr
	done
}

# ========================
# 2. Write to registers
# ========================
setup_onic() {
	pcimem="$SCRIPT_DIR/../tools/pcimem/pcimem"

	pci_dev="/sys/bus/pci/devices/0000"

	# Reset entire system
	sudo "$pcimem" "$pci_dev:"$1/resource2 0x0004 w 0x1;

	## Reinit Onic regs

	sudo "$pcimem" "$pci_dev:"$1/resource2 0x1000 w 0x1;
	sudo "$pcimem" "$pci_dev:"$1/resource2 0x2000 w 0x10001;

	sudo "$pcimem" "$pci_dev:"$1/resource2 0x8014 w 0x1;
	sudo "$pcimem" "$pci_dev:"$1/resource2 0x800c w 0x1;

	sudo "$pcimem" "$pci_dev:"$1/resource2 0xc014 w 0x1;
	sudo "$pcimem" "$pci_dev:"$1/resource2 0xc00c w 0x1;
}

# ========================
# 3. Rebind driver
# ========================
rebind_driver() {
	for pci_adr in "$@"; do # rebind all devices
		sudo "$DPDK_PATH"/usertools/dpdk-devbind.py -b vfio-pci $pci_adr
	done

	sudo "$DPDK_PATH"/usertools/dpdk-devbind.py -s
}

unbind_onic $@
setup_onic $1 #: not needed anymore, done in app on init
rebind_driver $@
