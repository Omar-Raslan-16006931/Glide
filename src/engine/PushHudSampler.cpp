// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "veyra/engine/PushHudSampler.h"
#include "veyra/Log.h"

#include <algorithm>
#include <format>

namespace veyra::engine {
namespace {
bool layoutOf(DXGI_FORMAT format, hud::PixelLayout& layout) {
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        layout = hud::PixelLayout::Rgba8; return true;
    case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
        layout = hud::PixelLayout::Bgra8; return true;
    case DXGI_FORMAT_R10G10B10A2_UNORM: case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        layout = hud::PixelLayout::Rgb10a2; return true;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        layout = hud::PixelLayout::Rgba16f; return true;
    default: return false;
    }
}
} // namespace

void PushHudSampler::start(uint32_t samples, uint32_t intervalMs) {
    std::lock_guard lock(m_);
    running_ = true; done_ = false; failed_ = false; error_.clear();
    target_ = std::clamp<uint32_t>(samples, uint32_t(hud::kMinFrames), 32);
    intervalMs_ = std::clamp<uint32_t>(intervalMs, 50, 5000);
    nextDue_ = {};
    frames_.clear(); preview_.clear(); detection_ = {};
    ++revision_;
    // A copy still in flight from an earlier run is read and dropped by status().
    veyra::log::info("push-hud", std::format("detection started samples={} intervalMs={}", target_, intervalMs_));
}

void PushHudSampler::cancel() {
    std::lock_guard lock(m_);
    if (running_) veyra::log::info("push-hud", std::format("detection cancelled after {} frames", frames_.size()));
    running_ = false;
    frames_.clear();
}

bool PushHudSampler::record(ID3D12GraphicsCommandList* list, const Frame& frame) {
    std::lock_guard lock(m_);
    if (!running_ || pending_ || inFlight_ || reading_ || !list || !frame.texture || frame.generated) return false;
    const auto now = std::chrono::steady_clock::now();
    if (now < nextDue_) return false;
    hud::PixelLayout layout;
    if (!layoutOf(frame.format, layout)) {
        running_ = false; failed_ = true; error_ = L"当前画面格式不支持自动识别 HUD";
        veyra::log::warn("push-hud", std::format("unsupported output format {}", int(frame.format)));
        return false;
    }
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    if (FAILED(frame.texture->GetDevice(IID_PPV_ARGS(&device)))) return false;
    const D3D12_RESOURCE_DESC desc = frame.texture->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rows = 0; UINT64 rowBytes = 0, total = 0;
    device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &rows, &rowBytes, &total);
    if (!total) return false;
    if (!readback_ || device_.Get() != device.Get() || readbackBytes_ < total) {
        readback_.Reset();
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = total; bd.Height = 1;
        bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback_)))) {
            running_ = false; failed_ = true; error_ = L"显存不足，无法识别 HUD";
            return false;
        }
        device_ = device; readbackBytes_ = total;
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = frame.texture;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    list->ResourceBarrier(1, &barrier);
    D3D12_TEXTURE_COPY_LOCATION from{}, to{};
    from.pResource = frame.texture; from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; from.SubresourceIndex = 0;
    to.pResource = readback_.Get(); to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; to.PlacedFootprint = footprint;
    list->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    list->ResourceBarrier(1, &barrier);
    footprint_ = footprint;
    width_ = std::min<uint32_t>(frame.width ? frame.width : uint32_t(desc.Width), uint32_t(desc.Width));
    height_ = std::min<uint32_t>(frame.height ? frame.height : desc.Height, desc.Height);
    layout_ = layout;
    pending_ = true;
    return true;
}

void PushHudSampler::submitted(ID3D12Fence* fence, uint64_t value) {
    std::lock_guard lock(m_);
    if (!pending_) return;
    pending_ = false;
    fence_ = fence; fenceValue_ = value; inFlight_ = fence != nullptr;
}

void PushHudSampler::discarded() {
    std::lock_guard lock(m_);
    pending_ = false;
}

void PushHudSampler::finishLocked() {
    detection_ = hud::detect(frames_);
    running_ = false; done_ = true;
    frames_.clear();
    // Nothing is in flight now: give the readback memory (up to 66 MB at 4K FP16) and the device back.
    readback_.Reset(); device_.Reset(); readbackBytes_ = 0;
    veyra::log::info("push-hud", std::format("detection done regions={} motion={:.2f} candidates={} tooStill={}",
        detection_.regions.size(), detection_.motion, detection_.candidateCells, detection_.tooStill));
}

PushHudSampler::Status PushHudSampler::status() {
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    uint32_t width = 0, height = 0;
    hud::PixelLayout layout{};
    uint64_t revision = 0;
    bool last = false;
    {
        std::lock_guard lock(m_);
        if (inFlight_ && fence_ && fence_->GetCompletedValue() >= fenceValue_) {
            inFlight_ = false; fence_.Reset();
            if (running_) {
                buffer = readback_; footprint = footprint_; width = width_; height = height_; layout = layout_;
                revision = revision_; reading_ = true;
                last = frames_.size() + 1 >= target_;
            }
        }
    }
    if (buffer) {
        hud::LumaFrame frame;
        std::vector<uint8_t> preview;
        bool ok = false;
        void* mapped = nullptr;
        const D3D12_RANGE range{0, SIZE_T(footprint.Offset + uint64_t(footprint.Footprint.RowPitch) * height)};
        if (SUCCEEDED(buffer->Map(0, &range, &mapped)) && mapped) {
            ok = hud::reduce(static_cast<const uint8_t*>(mapped) + footprint.Offset, footprint.Footprint.RowPitch, width, height, layout,
                             frame, last ? &preview : nullptr);
            const D3D12_RANGE none{0, 0};
            buffer->Unmap(0, &none);
        }
        std::lock_guard lock(m_);
        reading_ = false;
        if (running_ && revision == revision_) {
            if (!ok) { running_ = false; failed_ = true; error_ = L"无法读取画面，HUD 识别已停止"; }
            else {
                frames_.push_back(std::move(frame));
                if (!preview.empty()) preview_ = std::move(preview);
                nextDue_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(intervalMs_);
                if (frames_.size() >= target_) finishLocked();
            }
        }
    }
    std::lock_guard lock(m_);
    Status out;
    out.running = running_; out.done = done_; out.failed = failed_; out.error = error_;
    out.taken = uint32_t(done_ ? target_ : frames_.size()); out.target = target_;
    out.revision = revision_;
    if (done_) { out.detection = detection_; out.preview = preview_; }
    return out;
}

} // namespace veyra::engine
