# keyview

## Introduction
it is ocasionally useful to display the currently pressed keys on the screen for things like video confrences and screen recordings.
this program is a subview client that allows for displaying the pressed keys on the screen as an overlay.

The project is still active in development. Contributions are welcome!

## Building
to build this example
```
./make.sh
```

## usage
### running
to run this example
```
./keyview
```
### environment variables
if `SUBVIEW_SOCK` is set then it is used as the path to the subview control socket

## Dependencies
### libinput
this is used in getting the currently active keys from the keyboard
### udev
used for monitoring event device changes
### libevdev
partially used for converting the keyboard keycodes to readable text
### subview
used as a server for rendering subtitles

