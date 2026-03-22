{
  pkgs,
  lib,
  config,
  inputs,
  ...
}: {
  languages.c.enable = true;

  packages = [
    pkgs.wayland            # libwayland-client headers + lib
    pkgs.wayland-scanner    # generates C bindings from protocol XML
    pkgs.wayland-protocols   # xdg-shell protocol XML (dependency of layer-shell)
    pkgs.wlr-protocols      # wlr-layer-shell-unstable-v1 protocol XML
    pkgs.pkg-config         # finding wayland cflags/libs
    pkgs.clang-tools        # clang-format
  ];
}
