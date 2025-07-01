# Makefile to build clk_sync_app into build/ directory

APP = clk_sync_app
SRCS = main.cpp
BUILD_DIR = build
BIN = $(BUILD_DIR)/$(APP)

PKGCONF ?= pkg-config
DPDK_CFLAGS = $(shell $(PKGCONF) --cflags libdpdk)
DPDK_LDLIBS = $(shell $(PKGCONF) --libs libdpdk)

CC = cc
CFLAGS += -O3 -Wall $(DPDK_CFLAGS)
LDFLAGS += $(DPDK_LDLIBS)

# Default target
all: $(BIN)

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile into build/
$(BIN): $(SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Clean
clean:
	rm -rf $(BUILD_DIR)
