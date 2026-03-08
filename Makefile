CC ?= gcc
CFLAGS = -Wall -Wextra -O2 -I./include -D_GNU_SOURCE
LDFLAGS = ./rust/target/release/libblackmap_rust.a -ldl

# Enable sanitizers when DEBUG target is used
ifdef DEBUG
    CFLAGS += -g -O0 -fsanitize=address,undefined
    LDFLAGS += -fsanitize=address,undefined
endif

# Optional: liburing
ifeq ($(shell pkg-config --exists liburing && echo 1),1)
    CFLAGS += -DHAVE_LIBURING
    LDFLAGS += $(shell pkg-config --libs liburing)
endif

SRCDIR = src
OBJDIR = obj
BINDIR = .
RUSTDIR = rust
RUST_TARGET = $(RUSTDIR)/target/release/libblackmap_rust.a

# CLI project location and binary paths
CLI_DIR = cli
# workspace uses a shared target directory at root
CLI_DEBUG_BIN = target/debug/cli
CLI_RELEASE_BIN = target/release/cli

SOURCES = $(shell find $(SRCDIR) -name "*.c")
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
HEADERS = $(shell find include -name "*.h")

TARGET = blackmap

.PHONY: all clean install uninstall help debug rust

# Default build: compile CLI debug binary and then the C core (library) for completeness.
all: cli-debug rust $(TARGET)

# Build CLI in debug mode and copy resulting executable to workspace root
cli-debug:
	cd $(CLI_DIR) && cargo build
	@cp -f $(CLI_DEBUG_BIN) $(BINDIR)/blackmap
	@echo "[+] Updated root binary with CLI (debug)"

# Build CLI release binary and copy
cli-release:
	cd $(CLI_DIR) && cargo build --release
	@cp -f $(CLI_RELEASE_BIN) $(BINDIR)/blackmap
	@echo "[+] Updated root binary with CLI (release)"

rust: $(RUST_TARGET)

$(RUST_TARGET): $(RUSTDIR)/src/lib.rs $(RUSTDIR)/Cargo.toml
	cd $(RUSTDIR) && cargo build --release

debug: CFLAGS += -g -O0 -DDEBUG
debug: rust cli-debug $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "[+] Build successful: $(TARGET)"
	@ls -lh $(TARGET)
	# After building core, ensure CLI binary is still in place
	@if [ -f $(CLI_DEBUG_BIN) ]; then cp -f $(CLI_DEBUG_BIN) $(BINDIR)/blackmap; fi

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)
	cd $(RUSTDIR) && cargo clean
	@echo "[+] Clean completed"

install: $(TARGET)
	mkdir -p /usr/local/bin
	install -m 755 $(TARGET) /usr/local/bin/$(TARGET)
	@echo "[+] Installed to /usr/local/bin/$(TARGET)"

uninstall:
	rm -f /usr/local/bin/$(TARGET)
	@echo "[+] Uninstalled"

help:
	@echo "BlackMap Build System"
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build blackmap (default)"
	@echo "  debug    - Build with debug symbols"
	@echo "  clean    - Remove build artifacts"
	@echo "  install  - Install to /usr/local/bin (requires root)"
	@echo "  uninstall - Remove installation (requires root)"
	@echo "  help     - Show this help"


