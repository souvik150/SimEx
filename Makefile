# ──────────────────────────────────────────────
# User Configuration
# ──────────────────────────────────────────────
BUILD_DIR := build
TARGET := order_matching_system

# ──────────────────────────────────────────────
# Main Targets
# ──────────────────────────────────────────────

# Default target: configure + build (Release)
all: configure build

# Configure Release build
configure:
	@echo "⚙️  Configuring CMake project (Release)..."
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

# Configure Debug build
configure-debug:
	@echo "⚙️  Configuring CMake project (Debug)..."
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug

# Build Release target
build:
	@echo "🔧 Building target $(TARGET) (Release)..."
	@cmake --build $(BUILD_DIR) --target $(TARGET) -j

# Build Debug target
build-debug:
	@echo "🧰 Configuring and building target $(TARGET) (Debug)..."
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	@cmake --build $(BUILD_DIR) --target $(TARGET) -j

# Run the Release binary
run: build
	@echo "🚀 Running $(TARGET) (Release)..."
	@$(BUILD_DIR)/$(TARGET)

# Run the Debug binary (with gdb if available)
run-debug: build-debug
	@echo "🐞 Running $(TARGET) (Debug)..."
	@if command -v gdb >/dev/null 2>&1; then \
		gdb -q $(BUILD_DIR)/$(TARGET); \
	else \
		$(BUILD_DIR)/$(TARGET); \
	fi

# Clean the build directory
clean:
	@echo "🧹 Cleaning build directory..."
	@rm -rf $(BUILD_DIR)

# Full rebuild (clean + reconfigure + build)
rebuild: clean all

# Full debug rebuild
rebuild-debug: clean configure-debug build-debug

# Show available targets
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
