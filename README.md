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
