# subview plugin for mpv

## Introduction
[mpv](https://mpv.io/) is an open source video player. this is an implementation of the subview protocol for mpv subtitle playback.

The project is still active in development. Contributions are welcome!

## Installation
to install the subview plugin on your system
```
cp examples/mpv/subview.lua ~/.config/mpv/scripts/subview.lua
```
or alternatively to try it out first
```
mpv --script=examples/mpv/subview.lua <mpv options>
```

## usage
### keyboard shourtcuts
|  Keys                                                                     |  Description                                                   |
|---                                                                        |---                                                             |
| <kbd>ctrl</kbd> + <kbd>v</kbd>                                          | toggle subview rendering for subtitles |
| <kbd>v</kbd>                                          | toggle internal rendering for subtitles |
### environment variables
if `SUBVIEW_SOCK` is set then it is used as the path to the subview control socket

## Dependencies
### luaposix
this is used in establishing a connection to the subview server
### mpv
used for running the plugin
### subview
used as a server for rendering subtitles

