#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Glide streaming mode scenes (qml/Veyra/StudioPage.qml): what recordings, the instant replay and
// live streams carry on top of the picture.
//  - a game scene with overlays (an image such as your logo, a text tag) blended onto the picture;
//  - a card scene ("Starting soon", "Be right back", "Thanks for watching"): an opaque full picture
//    that replaces the game in the output while it is on.
// The interface draws the scene as one straight-alpha sRGB RGBA picture of the output size
// (bridge PushStudio.cpp) and publishes it here; every recorder segment picks the newest one up on
// its next frame and blends it onto its NV12/P010 planes (FrameOverlay, shaders/PushCompareOverlay.hlsl,
// the comparison clip badge blend, run over the whole frame). What you see in Glide itself is
// unchanged, as in OBS's program output. tests/integration/PushGpuTests.cpp runs it on WARP.
#include "veyra/engine/RecordingCompare.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace veyra::pipeline { class StateTracker; }

namespace veyra::engine {

// The scene's picture: straight-alpha sRGB RGBA8, `width` x `height`. Null: no overlay.
using PushOverlayImage = RecordingLabel;
void setPushStreamOverlay(std::shared_ptr<const PushOverlayImage> image);
// The newest picture and its version (+1 per change; 0 before the first).
std::shared_ptr<const PushOverlayImage> pushStreamOverlay(uint64_t& version);
uint64_t pushStreamOverlayVersion();

// One recorder's copy of the scene picture, at its frame size, blended per frame.
class FrameOverlay {
public:
    FrameOverlay();
    ~FrameOverlay();
    FrameOverlay(const FrameOverlay&) = delete;
    FrameOverlay& operator=(const FrameOverlay&) = delete;
    // `image` is scaled to width x height when its size differs. False: no GPU resources (no overlay).
    bool create(ID3D12Device* device, uint32_t width, uint32_t height, bool hdr, const PushOverlayImage& image);
    // After the conversion wrote y and uv (UAV-capable R8/R16 and R8G8/R16G16 planes of the frame size).
    void apply(ID3D12GraphicsCommandList* list, pipeline::StateTracker& states, ID3D12Resource* y, ID3D12Resource* uv);
    bool ready() const { return ready_; }
private:
    struct Impl;
    std::unique_ptr<Impl> p_;
    bool ready_ = false;
};

} // namespace veyra::engine
