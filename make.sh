#!/bin/bash

mkdir -p build
cp -n config.h build/config.h
set -e

PKGS="wayland-client cairo"
LDFLAGS="$LDFLAGS $(pkg-config --libs $PKGS)"
CFLAGS="$CFLAGS $(pkg-config --cflags $PKGS)"

bash src/wayland/xdg-shell.cgen | gcc -c -xc - $CFLAGS -o build/xdg-shell.o

bash src/wayland/wlr-layer-shell-unstable-v1.cgen | gcc -c -xc - $CFLAGS -o build/zwlr-shell.o
bash src/wayland/wlr-layer-shell-unstable-v1.hgen build/zwlr-shell.h

gcc -c -iquote build src/wayland/wayland.c $CFLAGS -o build/wayland.o
gcc -c -iquote build src/subview.c $CFLAGS -o build/subview.o
gcc build/subview.o build/zwlr-shell.o build/xdg-shell.o build/wayland.o $LDFLAGS -o subview

#./a.out
