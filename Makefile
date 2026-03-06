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

SOURCES = $(shell find $(SRCDIR) -name "*.c")
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
HEADERS = $(shell find include -name "*.h")

TARGET = blackmap

.PHONY: all clean install uninstall help debug rust

all: rust $(TARGET)

rust: $(RUST_TARGET)

$(RUST_TARGET): $(RUSTDIR)/src/lib.rs $(RUSTDIR)/Cargo.toml
	cd $(RUSTDIR) && cargo build --release

debug: CFLAGS += -g -O0 -DDEBUG
debug: rust $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "[+] Build successful: $(TARGET)"
	@ls -lh $(TARGET)

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


