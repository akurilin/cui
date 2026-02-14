.PHONY: configure build run clean check-tools format format-check lint analyze precommit install-hooks

C_SOURCES := $(shell find . -type f -name '*.c' -not -path './build/*' -not -path './vendored/*')
C_HEADERS := $(shell find . -type f -name '*.h' -not -path './build/*' -not -path './vendored/*')
C_FILES := $(C_SOURCES) $(C_HEADERS)

UNAME := $(shell uname)
ifeq ($(UNAME),Darwin)
  LLVM_BIN ?= /opt/homebrew/opt/llvm/bin
  SDK_PATH := $(shell xcrun --show-sdk-path)
  TIDY_EXTRA_ARGS := --extra-arg=-isysroot --extra-arg=$(SDK_PATH)
else
  LLVM_BIN ?= /usr/bin
  TIDY_EXTRA_ARGS :=
endif

CLANG_FORMAT := $(LLVM_BIN)/clang-format
CLANG_TIDY := $(LLVM_BIN)/clang-tidy
SCAN_BUILD := $(LLVM_BIN)/scan-build

check-tools:
	@for tool in "$(CLANG_FORMAT)" "$(CLANG_TIDY)" "$(SCAN_BUILD)"; do \
		[ -x "$$tool" ] || { \
			echo "Missing required tool: $$tool"; \
			echo "Install Homebrew LLVM (brew install llvm), then retry."; \
			exit 1; \
		}; \
	done

configure:
	cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

build: configure
	cmake --build build

run: build
	./build/hello

clean:
	rm -rf build

format: check-tools
	$(CLANG_FORMAT) -i $(C_FILES)

format-check: check-tools
	$(CLANG_FORMAT) --dry-run --Werror $(C_FILES)

lint: check-tools configure
	$(CLANG_TIDY) $(C_SOURCES) -p build $(TIDY_EXTRA_ARGS)

analyze: check-tools configure
	$(SCAN_BUILD) --status-bugs --exclude vendored/SDL cmake --build build --clean-first

precommit: format-check lint analyze

install-hooks:
	git config core.hooksPath .githooks
