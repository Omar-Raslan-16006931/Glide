Glide for macOS (preview)
===========================

Your console through a capture card, boosted to 60 or 120 FPS by your Mac. Frame generation, Frame
Pacing Repair and Live HUD Lock are Glide's own technology on this system and run on the graphics
card through Metal. One app for Apple Silicon and Intel Macs, macOS 12 or newer.

First start: Glide is not notarised by Apple yet, so macOS stops it the first time. Right-click (or
Control-click) Glide.app -> Open -> Open. If macOS still refuses: System Settings -> Privacy &
Security -> "Open Anyway". macOS then asks once for the camera and the microphone: that is your
capture card's picture and sound (macOS lists capture cards as cameras).

Try it without a capture card: Home -> Test picture.

Portable mode: put an empty file named portable.txt next to Glide.app. Settings and screenshots are
then kept in that folder.

This is a preview: tested automatically (self-test, packaging check), not yet with every capture card
or Mac. Reports are welcome: https://github.com/DosukaSOL/PushFPS/issues

Glide is free software under the GPL-3.0 (LICENSE.txt), built on Veyra by Likely7
(https://github.com/Likely7/Veyra-NRVideo). It uses Qt 6 (LGPL-3.0, https://www.qt.io/) and the FFmpeg
libraries that come with Qt (LGPL). Source code: https://github.com/DosukaSOL/PushFPS
