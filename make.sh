#!/bin/bash

cp -n config.h build/config.h
set -e

bash src/wayland/xdg-shell.cgen | gcc -c -xc - -o build/xdg-shell.o

bash src/wayland/wlr-layer-shell-unstable-v1.cgen | gcc -c -xc - -o build/zwlr-shell.o
bash src/wayland/wlr-layer-shell-unstable-v1.hgen build/zwlr-shell.h

gcc -c -iquote build src/subview.c -o build/subview.o
gcc -lwayland-client -lcairo build/subview.o build/zwlr-shell.o build/xdg-shell.o -o subview

#./a.out
