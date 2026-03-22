# wvisbell

Port of [xvisbell](https://git.gir.st/xvisbell.git) to work with Wayland.

## Usage

```
wvisbell [color] [count]
```

- `color` - single character color name (default: `w`)
  - `r` red, `g` green, `b` blue, `c` cyan, `m` magenta, `y` yellow, `k` black, `w` white
- `count` - number of flashes (default: `1`)

Examples:

```sh
wvisbell        # single white flash
wvisbell r      # single red flash
wvisbell b 3    # three blue flashes
```

## Dependencies

- `libwayland-client` (headers and library)
- `wayland-scanner` (generates C bindings from protocol XML)
- `wayland-protocols` (xdg-shell protocol XML)
- `wlr-protocols` (wlr-layer-shell-unstable-v1 protocol XML)
