/* Wayland visual bell: briefly flashes a fullscreen colored overlay on every
 * monitor. Requires a compositor that supports the wlr-layer-shell protocol
 * (Sway, Hyprland, river, etc.).
 *
 */

// NOLINTNEXTLINE(readability-identifier-naming)
#define _GNU_SOURCE /* for memfd_create */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* Core Wayland client API – lets us talk to the compositor. */
#include <wayland-client.h>

/* Generated from the wlr-layer-shell-unstable-v1 protocol XML.
 * This protocol lets us create overlay surfaces that sit above normal windows,
 * similar to X11's override_redirect. */
#include "wlr-layer-shell-client-protocol.h"

/* --- Global Wayland objects ---
 * These are singletons provided by the compositor via the "registry".
 * Think of the registry as a phonebook: we look up the interfaces we need. */
static struct wl_compositor *compositor = NULL;
static struct wl_shm *shm = NULL;
static struct zwlr_layer_shell_v1 *layer_shell = NULL;

/* We need one overlay surface per monitor. This linked list tracks them. */
struct output_surface {
  struct output_surface *next;
  struct wl_output *output;
  struct wl_surface *surface;
  struct zwlr_layer_surface_v1 *layer_surface;
  int32_t width, height; /* filled in by the configure callback */
  uint32_t fill_color;   /* ARGB8888 color to flash */
};
static struct output_surface *outputs = NULL;

/* --- Create and attach a solid-color buffer to a surface ---
 * Allocates a shared-memory buffer, fills it with the given color, and
 * attaches + commits it to the surface. */
static void attach_color_buffer(struct output_surface *os, uint32_t color) {
  if (os->width <= 0 || os->height <= 0) {
    return;
  }

  int32_t stride = os->width * 4; /* 4 bytes per pixel (ARGB8888) */
  int32_t size = stride * os->height;

  int fd = memfd_create("wvisbell-buffer", 0);
  if (fd < 0) {
    perror("wvisbell: memfd_create");
    return;
  }
  if (ftruncate(fd, size) < 0) {
    perror("wvisbell: ftruncate");
    close(fd);
    return;
  }

  uint32_t *pixels =
      mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (pixels == MAP_FAILED) {
    perror("wvisbell: mmap");
    close(fd);
    return;
  }

  for (int32_t i = 0; i < os->width * os->height; i++) {
    pixels[i] = color;
  }
  munmap(pixels, size);

  struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
  struct wl_buffer *buffer = wl_shm_pool_create_buffer(
      pool, 0, os->width, os->height, stride, WL_SHM_FORMAT_ARGB8888);
  wl_shm_pool_destroy(pool);
  close(fd);

  wl_surface_attach(os->surface, buffer, 0, 0);
  wl_surface_damage_buffer(os->surface, 0, 0, os->width, os->height);
  wl_surface_commit(os->surface);
  wl_buffer_destroy(buffer);
}

/* --- Layer surface configure callback ---
 * The compositor calls this to tell us the actual dimensions we should use.
 * We must ack the configure, then create a pixel buffer and attach it. */
static void layer_surface_configure(void *data,
                                    struct zwlr_layer_surface_v1 *lsurface,
                                    uint32_t serial, uint32_t w, uint32_t h) {
  struct output_surface *os = data;
  os->width = (int32_t)w;
  os->height = (int32_t)h;

  zwlr_layer_surface_v1_ack_configure(lsurface, serial);

  attach_color_buffer(os, os->fill_color);
}

static void layer_surface_closed(void *data,
                                 struct zwlr_layer_surface_v1 *lsurface) {
  (void)data;
  (void)lsurface;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

/* --- Registry listener ---
 * Called once for each global object the compositor advertises. We pick out
 * the three interfaces we need: compositor, shm, and layer_shell. We also
 * record every wl_output (monitor) so we can put an overlay on each one. */
static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version) {
  (void)data;
  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    compositor = wl_registry_bind(registry, name, &wl_compositor_interface,
                                  version < 4 ? version : 4);
  } else if (strcmp(interface, wl_shm_interface.name) == 0) {
    shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
  } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
    layer_shell =
        wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
  } else if (strcmp(interface, wl_output_interface.name) == 0) {
    /* Found a monitor – add it to our linked list. */
    struct output_surface *os = calloc(1, sizeof(*os));
    if (!os) {
      perror("wvisbell: calloc");
      return;
    }
    os->output = wl_registry_bind(registry, name, &wl_output_interface,
                                  version < 4 ? version : 4);
    os->next = outputs;
    outputs = os;
  }
}

static void registry_global_remove(void *data, struct wl_registry *registry,
                                   uint32_t name) {
  (void)data;
  (void)registry;
  (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

int main(int argc, char **argv) {
  /* ARGB8888: fully opaque alpha + the RGB value. */
  uint32_t fill_color = 0xFFFFFFFF;

  /* Parse optional color argument – same scheme as xvisbell. */
  if (argc > 1) {
    switch (argv[1][0]) {
    case 'r':
      fill_color = 0xFFFF0000;
      break;
    case 'g':
      fill_color = 0xFF00FF00;
      break;
    case 'b':
      fill_color = 0xFF0000FF;
      break;
    case 'c':
      fill_color = 0xFF00FFFF;
      break;
    case 'm':
      fill_color = 0xFFFF00FF;
      break;
    case 'y':
      fill_color = 0xFFFFFF00;
      break;
    case 'k':
      fill_color = 0xFF000000;
      break;
    case 'w':
      fill_color = 0xFFFFFFFF;
      break;
    default:
      break;
    }
  }

  /* Connect to the Wayland compositor (usually via the WAYLAND_DISPLAY env
   * var). This is analogous to XOpenDisplay() in X11. */
  struct wl_display *display = wl_display_connect(NULL);
  if (!display) {
    (void)fprintf(stderr, "Failed to connect to Wayland display\n");
    return EXIT_FAILURE;
  }

  /* Ask the compositor for its list of global objects. Our registry_global
   * callback will fire once per object. */
  struct wl_registry *registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);

  /* roundtrip: send the request, wait for all replies. After this call,
   * compositor/shm/layer_shell/outputs are populated. */
  wl_display_roundtrip(display);

  if (!compositor || !shm || !layer_shell) {
    (void)fprintf(stderr,
                  "Compositor missing required interfaces "
                  "(need wl_compositor, wl_shm, zwlr_layer_shell_v1)\n");
    wl_display_disconnect(display);
    return EXIT_FAILURE;
  }

  /* Create a layer surface on each output (monitor). */
  for (struct output_surface *os = outputs; os; os = os->next) {
    os->fill_color = fill_color;

    /* wl_surface is the basic drawable in Wayland – like a Window in X11,
     * but without any built-in decoration or positioning. */
    os->surface = wl_compositor_create_surface(compositor);

    /* Wrap it in a layer surface on the OVERLAY layer (topmost).
     * This is what makes it fullscreen and above everything else. */
    os->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        layer_shell, os->surface, os->output, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        "wvisbell");

    /* Anchor to all four edges so the compositor stretches it to fill the
     * entire output. Size 0,0 means "use whatever the anchors give you". */
    zwlr_layer_surface_v1_set_anchor(os->layer_surface,
                                     ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                                         ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                                         ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                                         ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_size(os->layer_surface, 0, 0);

    /* exclusive_zone = -1 means "don't reserve space, just overlap". */
    zwlr_layer_surface_v1_set_exclusive_zone(os->layer_surface, -1);

    zwlr_layer_surface_v1_add_listener(os->layer_surface,
                                       &layer_surface_listener, os);

    /* Initial commit to signal the compositor that we're ready to receive
     * a configure event with the output dimensions. */
    wl_surface_commit(os->surface);
  }

  /* roundtrip: wait for all configure events to arrive and be handled.
   * After this, every surface has its buffer attached and committed. */
  wl_display_roundtrip(display);

  /* Flush ensures all our buffer-attach + commit messages reach the
   * compositor before we sleep. */
  wl_display_flush(display);

  /* Flash on for 100ms. */
  usleep(100000);

  wl_display_disconnect(display);
  return EXIT_SUCCESS;
}
