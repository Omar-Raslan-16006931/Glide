// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once

namespace pushfps {
// macOS: tells the system that Glide shows live video, so App Nap and timer coalescing never slow its
// capture and display threads (as OBS Studio does). Elsewhere it does nothing.
void keepTimingPrecise();
}  // namespace pushfps
