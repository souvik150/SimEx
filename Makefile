BUILD_DIR := build
BUILD_DIR_DEBUG := build-debug
BIN_DIR := $(BUILD_DIR)/bin
BIN_DIR_DEBUG := $(BUILD_DIR_DEBUG)/bin
TEST_DIR := $(BUILD_DIR)/tests
TARGET := $(BIN_DIR)/order_matching_system
TARGET_DEBUG := $(BIN_DIR_DEBUG)/order_matching_system
GEN_TARGET := $(BIN_DIR)/order_generator
TEST_TARGET := $(TEST_DIR)/order_book_tests
BOOK_TARGET := $(BIN_DIR)/book
BENCH_TARGET := $(BIN_DIR)/add_order_bench
TOKEN ?= 26000

all: configure build

configure:
	@echo "Configuring CMake project (Release)..."
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

configure-debug:
	@echo "Configuring CMake project (Debug)..."
	@cmake -S . -B $(BUILD_DIR_DEBUG) -DCMAKE_BUILD_TYPE=Debug

build:
	@echo "Building (Release)..."
	@cmake --build $(BUILD_DIR) -j

build-debug:
	@echo "Configuring and building target $(TARGET) (Debug)..."
	@cmake -S . -B $(BUILD_DIR_DEBUG) -DCMAKE_BUILD_TYPE=Debug
	@cmake --build $(BUILD_DIR_DEBUG) --target order_matching_system -j

run: build
	@echo "Running $(TARGET) (Release)..."
	@$(TARGET)

run-generator: build
	@echo "Running $(GEN_TARGET) (Release)..."
	@$(GEN_TARGET)

run-book: build
	@if [ -z "$(TOKEN)" ]; then \
		echo "Please provide TOKEN=<instrument-token> for run-book"; \
		exit 1; \
	fi
	@echo "Running $(BOOK_TARGET) for token $(TOKEN) ..."
	@$(BOOK_TARGET) $(TOKEN)

run-debug: build-debug
	@echo "Running $(TARGET) (Debug)..."
	@if command -v gdb >/dev/null 2>&1; then \
		gdb -q $(TARGET_DEBUG); \
	else \
		$(TARGET_DEBUG); \
	fi

test: build
	@echo "Building tests..."
	@cmake --build $(BUILD_DIR) --target order_book_tests -j

run-test: test
	@echo "Running tests..."
	@$(TEST_TARGET)

clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR) $(BUILD_DIR_DEBUG)

rebuild: clean all

rebuild-debug: clean configure-debug build-debug

help:
	@echo "Available targets:"
	@echo "  make              - Configure and build Release (default)"
	@echo "  make configure    - Configure Release build"
	@echo "  make configure-debug - Configure Debug build"
	@echo "  make build        - Build Release binary"
	@echo "  make build-debug  - Build Debug binary"
	@echo "  make run          - Run Release binary"
	@echo "  make run-generator- Run Release order generator"
	@echo "  make run-book     - Run the FTX-style book UI (TOKEN=<instrument-token>)"
	@echo "  make run-bench    - Run the addOrder micro-benchmark"
	@echo "  make run-debug    - Run Debug binary (via gdb if installed)"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make rebuild      - Clean, configure, and build (Release)"
	@echo "  make rebuild-debug - Clean, configure, and build (Debug)"
	@echo "  make help         - Show this help message"

.PHONY: all configure configure-debug build build-debug run run-debug clean rebuild rebuild-debug help
run-bench: build
	@echo "Running $(BENCH_TARGET) ..."
	@$(BENCH_TARGET)
