#!/bin/bash
set -e

mkdir -p build

PKGS="wayland-client pangocairo"
CFLAGS="$CFLAGS -iquote build -iquote src"

LDFLAGS="$LDFLAGS $(pkg-config --libs $PKGS)"
CFLAGS="$CFLAGS $(pkg-config --cflags $PKGS)"

bash src/wayland/xdg-shell.cgen | gcc -c -xc - $CFLAGS -o build/xdg-shell.o

bash src/wayland/wlr-layer-shell-unstable-v1.cgen | gcc -c -xc - $CFLAGS -o build/zwlr-shell.o
bash src/wayland/wlr-layer-shell-unstable-v1.hgen build/zwlr-shell.h

gcc -c src/options.c $CFLAGS -o build/options.o
gcc -c src/connection.c $CFLAGS -o build/connection.o
gcc -c src/pangocairo/renderer.c $CFLAGS -o build/pangocairo-rend.o
gcc -c src/wayland/wayland.c $CFLAGS -o build/wayland.o
gcc -c src/subview.c $CFLAGS -o build/subview.o
gcc build/pangocairo-rend.o build/connection.o build/options.o build/subview.o build/zwlr-shell.o build/xdg-shell.o build/wayland.o $LDFLAGS -o subview
