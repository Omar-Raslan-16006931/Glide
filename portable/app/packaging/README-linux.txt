Glide for Linux (preview)
===========================

Your console through a capture card, boosted to 60 or 120 FPS by your PC. Frame generation, Frame
Pacing Repair and Live HUD Lock are Glide's own technology on this system: no NVIDIA, AMD or Intel
components are needed. It runs on the graphics card through Vulkan or OpenGL.

Start:        ./Glide            (or double-click the AppImage from the release page)
Try it:       Home -> Test picture, no capture card needed
Check:        ./Glide --selftest 6
Devices:      ./Glide --list-devices

Capture cards: any card that Linux shows as a video device (UVC, /dev/video*), which is almost every
USB capture card (Elgato HD60 X / 4K X / 4K S, AVerMedia Live Gamer, generic USB 3 cards). Its sound
shows up as a microphone and plays in sync with the picture. Your user needs access to /dev/video*
(usually the "video" group).

Portable mode: create an empty file named portable.txt in this folder. Settings and screenshots are
then kept here, and deleting the folder removes everything.

Needs: x86-64 Linux with glibc 2.31 or newer (Ubuntu 20.04+, Debian 11+, Fedora 33+...), OpenGL 3.3 or
Vulkan, X11 or Wayland (through XWayland). Desktop systems have the OpenGL libraries already; on a minimal
install, "error while loading shared libraries: libOpenGL.so.0" means: sudo apt install libopengl0 libegl1
(Fedora: sudo dnf install libglvnd-opengl libglvnd-egl).

This is a preview: tested automatically (self-test, GPU check against the CPU reference, a clean
Ubuntu container), not yet with every capture card. Reports are welcome:
https://github.com/DosukaSOL/PushFPS/issues

Glide is free software under the GPL-3.0 (LICENSE.txt), built on Veyra by Likely7
(https://github.com/Likely7/Veyra-NRVideo). It uses Qt 6 (LGPL-3.0, https://www.qt.io/) and the FFmpeg
libraries that come with Qt (LGPL). Source code: https://github.com/DosukaSOL/PushFPS
