// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// Checks for the optional ANR add-on installer (Settings -> Components and licenses -> Install from
// download): every entry of the archive the user picked is looked at before anything is extracted,
// and only safe entries of safe types are copied into the runtime folder.
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace veyra::engine::archive {

// An archive entry path that stays inside the folder it is extracted to: relative, no drive letter,
// no "..", no device names, no control characters. Both slash kinds count as separators.
inline bool safeEntry(std::string_view path) {
    if (path.empty() || path.size() > 1024) return false;
    if (path[0] == '/' || path[0] == '\\') return false;
    if (path.size() >= 2 && path[1] == ':') return false;                 // C:...
    for (unsigned char c : path) if (c < 32 || c == 127) return false;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find_first_of("/\\", start);
        if (end == std::string_view::npos) end = path.size();
        const std::string_view part = path.substr(start, end - start);
        if (part == "..") return false;
        // Windows device names (CON, NUL, COM1 ...) and names ending in a dot or space.
        std::string upper;
        for (char c : part.substr(0, part.find('.'))) upper += char(std::toupper((unsigned char)c));
        static const char* devices[] = {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                                        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
        for (const char* d : devices) if (upper == d) return false;
        if (!part.empty() && part != "." && (part.back() == '.' || part.back() == ' ')) return false;
        if (end == path.size()) break;
        start = end + 1;
    }
    return true;
}

// File types that are copied: libraries and data. Programs and scripts are left out (nothing in the
// runtime folder needs them, and nothing there should be runnable).
inline bool copiedType(std::string_view name) {
    const size_t dot = name.rfind('.');
    if (dot == std::string_view::npos) return true;  // data without an extension
    std::string ext;
    for (char c : name.substr(dot + 1)) ext += char(std::tolower((unsigned char)c));
    static const char* blocked[] = {"exe", "com", "bat", "cmd", "ps1", "psm1", "vbs", "vbe", "js", "jse", "wsf", "wsh",
                                    "msi", "msp", "scr", "lnk", "url", "reg", "hta", "cpl", "sh", "jar", "pif", "inf"};
    for (const char* b : blocked) if (ext == b) return false;
    return true;
}

// Limits for one add-on: generous for a runtime with models, small enough to refuse a disk bomb.
inline constexpr int kMaxFiles = 2000;
inline constexpr long long kMaxBytes = 8LL * 1024 * 1024 * 1024;

}  // namespace veyra::engine::archive
