BUILD_DIR := build
BIN_DIR := $(BUILD_DIR)/bin
TEST_DIR := $(BUILD_DIR)/tests
TARGET := $(BIN_DIR)/order_matching_system
TEST_TARGET := $(TEST_DIR)/order_book_tests

all: configure build

configure:
	@echo "Configuring CMake project (Release)..."
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

configure-debug:
	@echo "Configuring CMake project (Debug)..."
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug

build:
	@echo "Building (Release)..."
	@cmake --build $(BUILD_DIR) -j

build-debug:
	@echo "Configuring and building target $(TARGET) (Debug)..."
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	@cmake --build $(BUILD_DIR) --target $(TARGET) -j

run: build
	@echo "Running $(TARGET) (Release)..."
	@$(TARGET)

run-debug: build-debug
	@echo "Running $(TARGET) (Debug)..."
	@if command -v gdb >/dev/null 2>&1; then \
		gdb -q $(TARGET); \
	else \
		$(TARGET); \
	fi

test: build
	@echo "Building tests..."
	@cmake --build $(BUILD_DIR) --target order_book_tests -j

run-test: test
	@echo "Running tests..."
	@$(TEST_TARGET)

clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)

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
	@echo "  make run-debug    - Run Debug binary (via gdb if installed)"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make rebuild      - Clean, configure, and build (Release)"
	@echo "  make rebuild-debug - Clean, configure, and build (Debug)"
	@echo "  make help         - Show this help message"

.PHONY: all configure configure-debug build build-debug run run-debug clean rebuild rebuild-debug help
