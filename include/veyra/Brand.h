#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Glide product identity: the one place that names the product and its GitHub home.
//
// Glide is a fork of Veyra (https://github.com/Likely7/Veyra-NRVideo, GPL-3.0). Internal
// identifiers (namespaces, target names, the qml/Veyra module, window classes, data folders)
// keep Veyra's names on purpose so upstream fixes keep merging cleanly; only what a user sees
// uses these constants.

namespace veyra::brand {
inline constexpr const char* kProductName = "Glide";
inline constexpr const wchar_t* kProductNameW = L"Glide";

// GitHub repository that publishes Glide releases. The update checker only accepts a release
// page under kReleaseTagPathPrefix, so these must match the repository's exact owner/name case.
inline constexpr const char* kRepositoryUrl = "https://github.com/Omar-Raslan-16006931/Glide";
inline constexpr const char* kIssuesUrl = "https://github.com/Omar-Raslan-16006931/Glide/issues";
inline constexpr const char* kReleasesUrl = "https://github.com/Omar-Raslan-16006931/Glide/releases";
inline constexpr const char* kLatestReleaseApi = "https://api.github.com/repos/Omar-Raslan-16006931/Glide/releases/latest";
inline constexpr const char* kReleaseTagPathPrefix = "/Omar-Raslan-16006931/Glide/releases/tag/";

// The project Glide is built on; shown in About and the README credits.
inline constexpr const char* kUpstreamName = "Veyra";
inline constexpr const char* kUpstreamUrl = "https://github.com/Likely7/Veyra-NRVideo";

// Veyra's ANR (Apex Neural Rendering) frame generation, offered exactly as Veyra 2.1.6 does: the
// host is in the program, but ANR's own runtime may not be redistributed yet, so no ANR file ships.
// The choice is usable once the user places ANR's files in runtime\experimental\anr. False would
// hide it from every menu and report it unavailable.
inline constexpr bool kAnrFrameGeneration = true;
// ANR is made by Apex Compute (credited in About, the README and the components page). The
// app links to their releases and installs the package a user downloaded from there into
// runtime\experimental\anr on that user's PC; Glide itself never ships or mirrors it.
inline constexpr const char* kAnrName = "Apex Neural Rendering (ANR)";
inline constexpr const char* kAnrMaker = "Apex Compute";
inline constexpr const char* kAnrUrl = "https://github.com/apex-compute/ANR";
inline constexpr const char* kAnrReleasesUrl = "https://github.com/apex-compute/ANR/releases";
inline constexpr const char* kApexUrl = "https://apexcompute.com";
}
