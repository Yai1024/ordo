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
# 静态链接 C++ 运行库：产物 exe 不依赖 PATH 里的 libstdc++/libgcc 版本，
# 拷到任何 Windows 机器都能直接运行（本机 MSYS2 与 .vscode 两套 MinGW 曾因此冲突）。
# shell32 用于 main.cpp 从 GetCommandLineW 读取 UTF-8 参数。
ifeq ($(OS),Windows_NT)
PLATFORM_LDFLAGS := -static-libgcc -static-libstdc++ -lshell32
else
PLATFORM_LDFLAGS :=
endif
LDFLAGS  ?= $(PLATFORM_LDFLAGS)
FORMAT   ?= clang-format

BIN_DIR  := bin

ifeq ($(OS),Windows_NT)
EXEEXT   := .exe
else
EXEEXT   :=
endif

APP      := $(BIN_DIR)/ordo$(EXEEXT)
GUI      := $(BIN_DIR)/ordo-gui$(EXEEXT)
TEST_BIN := $(BIN_DIR)/ordo_test$(EXEEXT)

CLI_SRC  := src/main.cpp
GUI_SRC  := $(wildcard src/gui/*.cpp)
LIB_SRC  := $(filter-out $(CLI_SRC) $(GUI_SRC),$(wildcard src/*.cpp src/*/*.cpp src/*/*/*.cpp))
TEST_SRC := $(wildcard tests/*.cpp)
HEADERS  := $(wildcard include/ordo/*.h include/ordo/*/*.h include/ordo/*/*/*.h)

# GUI 只在 Windows 上构建：Win32 控件 + 公共控件。
ifeq ($(OS),Windows_NT)
ALL_BINS       := $(APP) $(GUI)
GUI_CXXFLAGS   := $(CXXFLAGS) -DUNICODE -D_UNICODE -municode -mwindows
GUI_LDLIBS     := -lcomctl32
else
ALL_BINS       := $(APP)
GUI_CXXFLAGS   := $(CXXFLAGS)
GUI_LDLIBS     :=
endif

# ---------- 目标 ----------

.PHONY: all test clean fmt fmt-check help run gui run-gui

all: $(ALL_BINS)

$(APP): $(LIB_SRC) $(CLI_SRC) $(HEADERS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(LIB_SRC) $(CLI_SRC) $(LDFLAGS)
	@echo "==> built: $@"

$(GUI): $(LIB_SRC) $(GUI_SRC) $(HEADERS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(GUI_CXXFLAGS) -o $@ $(LIB_SRC) $(GUI_SRC) $(LDFLAGS) $(GUI_LDLIBS)
	@echo "==> built: $@"

$(TEST_BIN): $(LIB_SRC) $(TEST_SRC) $(HEADERS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(LIB_SRC) $(TEST_SRC) $(LDFLAGS)

test: $(TEST_BIN)
	@echo "==> running tests"
	@./$(TEST_BIN)

run: $(APP)
	@./$(APP)

gui: $(GUI)

run-gui: $(GUI)
	@./$(GUI)

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
	@echo "  all        compile -> $(ALL_BINS)"
	@echo "  test       compile and run unit tests"
	@echo "  run        compile and launch the CLI"
	@echo "  gui        compile the Win32 GUI (Windows only)"
	@echo "  run-gui    compile and launch the GUI (Windows only)"
	@echo "  clean      remove build artifacts"
	@echo "  fmt        format in place with .clang-format"
	@echo "  fmt-check  report formatting differences only"
