WLR_PROTO_DIR := $(shell pkg-config --variable=pkgdatadir wlr-protocols)
WL_PROTO_DIR := $(shell pkg-config --variable=pkgdatadir wayland-protocols)

LAYER_SHELL_XML := $(WLR_PROTO_DIR)/unstable/wlr-layer-shell-unstable-v1.xml
XDG_SHELL_XML := $(WL_PROTO_DIR)/stable/xdg-shell/xdg-shell.xml

WAYLAND_CFLAGS := $(shell pkg-config --cflags wayland-client)
WAYLAND_LIBS := $(shell pkg-config --libs wayland-client)

CFLAGS ?= -Wall -Wextra -Wpedantic -Werror -O2
GENERATED_H := wlr-layer-shell-client-protocol.h xdg-shell-client-protocol.h
GENERATED_C := wlr-layer-shell-protocol.c xdg-shell-protocol.c

wvisbell: protocols wvisbell.c
	$(CC) $(CFLAGS) $(WAYLAND_CFLAGS) -o wvisbell wvisbell.c $(GENERATED_C) $(WAYLAND_LIBS)

protocols: $(GENERATED_H) $(GENERATED_C)

wlr-layer-shell-client-protocol.h: $(LAYER_SHELL_XML)
	wayland-scanner client-header $< $@

wlr-layer-shell-protocol.c: $(LAYER_SHELL_XML)
	wayland-scanner private-code $< $@

xdg-shell-client-protocol.h: $(XDG_SHELL_XML)
	wayland-scanner client-header $< $@

xdg-shell-protocol.c: $(XDG_SHELL_XML)
	wayland-scanner private-code $< $@

fmt:
	clang-format -i wvisbell.c

clean:
	rm -f wvisbell $(GENERATED_H) $(GENERATED_C)

.PHONY: clean fmt
