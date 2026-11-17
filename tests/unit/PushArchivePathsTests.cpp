// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "veyra/engine/PushArchivePaths.h"
#include <cstdio>
using namespace veyra::engine::archive;
static int failures = 0;
static void check(bool ok, const char* label) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", label); if (!ok) ++failures; }
int main() {
    check(safeEntry("Apex Neural Rendering FG/dlssg/nvngx_dlssg.dll"), "normal nested path");
    check(safeEntry("dlssg\\nvngx_dlssg.dll"), "backslash path");
    check(safeEntry("./dlssg/model.bin"), "leading ./");
    check(!safeEntry("../evil.dll"), "parent folder");
    check(!safeEntry("dlssg/../../evil.dll"), "parent folder in the middle");
    check(!safeEntry("dlssg\\..\\..\\evil.dll"), "parent folder with backslashes");
    check(!safeEntry("/etc/passwd"), "absolute (slash)");
    check(!safeEntry("\\Windows\\System32\\x.dll"), "absolute (backslash)");
    check(!safeEntry("C:/Windows/x.dll"), "drive letter");
    check(!safeEntry("c:x.dll"), "drive-relative");
    check(!safeEntry("dlssg/CON"), "device name");
    check(!safeEntry("dlssg/nul.txt"), "device name with extension");
    check(!safeEntry("dlssg/name."), "trailing dot");
    check(!safeEntry("dlssg/name "), "trailing space");
    check(!safeEntry(std::string("a\x01" "b")), "control character");
    check(!safeEntry(""), "empty");
    check(safeEntry("dlssg/console.dll"), "a name that only starts like a device is fine");
    check(copiedType("nvngx_dlssg.dll") && copiedType("model.onnx") && copiedType("LICENSE") && copiedType("readme.TXT"), "libraries and data copied");
    check(!copiedType("setup.exe") && !copiedType("run.BAT") && !copiedType("x.ps1") && !copiedType("a.lnk") && !copiedType("b.msi"), "programs and scripts left out");
    std::printf("%s\n", failures ? "PushArchivePaths tests FAILED" : "PushArchivePaths tests passed");
    return failures ? 1 : 0;
}
