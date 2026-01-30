.PHONY: all build test clean run-engine run-api lint format

BUILD_DIR := build
ENGINE_BIN := $(BUILD_DIR)/engine/exchange_engine

all: build

build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$$(nproc)

build-debug:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON .. && make -j$$(nproc)

test: build-debug
	@cd $(BUILD_DIR) && ctest --output-on-failure

test-api: build
	@cd api && python -m pytest tests/ -v

test-all: test test-api

run-engine: build
	@$(ENGINE_BIN)

run-api: build
	@cd api && python -m uvicorn app.main:app --reload

lint:
	@find engine/src engine/include -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run -Werror
	@cd api && python -m ruff check .

format:
	@find engine/src engine/include -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i
	@cd api && python -m ruff format .

clean:
	@rm -rf $(BUILD_DIR)
	@find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true

benchmark: build
	@$(BUILD_DIR)/engine/exchange_benchmark
