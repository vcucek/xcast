# XCast
is a simple server/client command line program for sharing screen under X.

With XCast you have the ability to either pull the display from a remote server or push a local display to a remote server, simply by specifying the IP address.
The application is ment to be used in a local network or VPN environment (like office) for quickly sharing displays between coleges.

The initial implementation was made for streaming a Linux PC display to a RaspberryPI, connected to a TV.
That was more then five years ago, before I bought the HDMI cable. With a bit of tinkering this functionality should work or is still working (make file includes rPI build target and ARM dependencies), but has not been tested since then.

The aplication is using FFmpeg for ecoding and decoding video.
<a href=http://ffmpeg.org>FFmpeg</a> is licensed under the <a href=http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html>LGPLv2.1</a> and its source can be downloaded <a href=link_to_your_sources>here</a>.

OpenGL on EGL is used for rendering the remote video stream which facilitates implementation of image/display compositing or any other post-processing transformations.

## Usage

Start XCast in the listening mode on machine A:
```bash
xcast --listen
```

On machine B, show machine A desktop:
```bash
xcast --pull <machine A IP address>
```

On machine B, show machine B desktop on machine A:
```bash
xcast --push <machine A IP address>
```

## Installation

Make sure that following dependencies are installed:
- ffmpeg
- xorg

On a tipical Linux desktop machine the xorg dependency is probably installed and used by the desktop environment.
You can check this by executing:
```bash
if [ $DISPLAY ]; then echo "X is running"; else echo "X is not running"; fi
```
If you get `X is not running`, your desktop environment is using the Wayland display protocol which is currently not supported. 

When all dependecies are met you can install the program:
```bash
git clone git@github.com:vcucek/xcast.git
cd xcast
make
make install
```
This builds the application into a single executable and copyes it to the `/usr/local/bin` folder.

## TODO:
- implement screen zooming, panning..
- implement keyboard/mouse capture and sharing
- add Wayland support
- test RaspberryPI support

