#!/bin/bash
gcc -o keyview keyview.c `pkg-config --cflags --libs libinput libudev libevdev`
