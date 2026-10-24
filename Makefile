# ============================================================
#  Ordo · 学习任务编排器 — 构建入口
#  用法：make / make test / make clean / make fmt / make help
# ============================================================

# Windows 下 mingw32-make 默认用 cmd.exe 执行命令，mkdir -p / rm -rf 会失败。
# 显式指定 POSIX shell（Git for Windows 自带 sh.exe）。
ifeq ($(OS),Windows_NT)
SHELL    := sh
endif

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -Wshadow -Iinclude -O2 -g
LDFLAGS  ?=
FORMAT   ?= clang-format

BIN_DIR  := bin
APP      := $(BIN_DIR)/ordo
TEST_BIN := $(BIN_DIR)/ordo_test

APP_SRC  := src/main.cpp
LIB_SRC  := $(filter-out $(APP_SRC),$(wildcard src/*.cpp src/*/*.cpp src/*/*/*.cpp))
TEST_SRC := $(wildcard tests/*.cpp)
HEADERS  := $(wildcard include/ordo/*.h include/ordo/*/*.h include/ordo/*/*/*.h)

# ---------- 目标 ----------

.PHONY: all test clean fmt fmt-check help run

all: $(APP)

$(APP): $(LIB_SRC) $(APP_SRC) $(HEADERS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(LIB_SRC) $(APP_SRC) $(LDFLAGS)
	@echo "==> built: $@"

$(TEST_BIN): $(LIB_SRC) $(TEST_SRC) $(HEADERS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(LIB_SRC) $(TEST_SRC) $(LDFLAGS)

test: $(TEST_BIN)
	@echo "==> running tests"
	@./$(TEST_BIN)

run: $(APP)
	@./$(APP)

clean:
	@rm -rf $(BIN_DIR) build
	@echo "==> cleaned"

fmt:
	@$(FORMAT) -i $(wildcard src/*.cpp src/*/*.cpp src/*/*/*.cpp) \
	                $(wildcard tests/*.cpp) $(HEADERS)
	@echo "==> formatted"

fmt-check:
	@$(FORMAT) --dry-run --Werror $(wildcard src/*.cpp src/*/*.cpp src/*/*/*.cpp) \
	                                $(wildcard tests/*.cpp) $(HEADERS)

help:
	@echo "Ordo build targets (Windows: use mingw32-make, POSIX: make):"
	@echo "  all        compile -> $(APP)"
	@echo "  test       compile and run unit tests"
	@echo "  run        compile and launch"
	@echo "  clean      remove build artifacts"
	@echo "  fmt        format in place with .clang-format"
	@echo "  fmt-check  report formatting differences only"
