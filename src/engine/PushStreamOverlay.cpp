// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "veyra/engine/PushStreamOverlay.h"
#include "veyra/pipeline/GpuPassUtils.h"

#include <atomic>
#include <cstring>
#include <mutex>

namespace veyra::engine {
namespace {
std::mutex overlayMutex;
std::shared_ptr<const PushOverlayImage> overlayImage;
std::atomic<uint64_t> overlayVersion{0};
constexpr uint32_t kPitchAlign = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
} // namespace

void setPushStreamOverlay(std::shared_ptr<const PushOverlayImage> image) {
    {
        std::lock_guard lock(overlayMutex);
        overlayImage = std::move(image);
    }
    overlayVersion.fetch_add(1);
}
std::shared_ptr<const PushOverlayImage> pushStreamOverlay(uint64_t& version) {
    std::lock_guard lock(overlayMutex);
    version = overlayVersion.load();
    return overlayImage;
}
uint64_t pushStreamOverlayVersion() { return overlayVersion.load(); }

// Heap slots 0-2: the overlay, the picture's y and uv copies; 3-4: the recorder's y and uv.
struct FrameOverlay::Impl {
    ID3D12Device* device = nullptr;
    uint32_t width = 0, height = 0;
    bool hdr = false, uploaded = false;
    pipeline::ComputePass pass;
    pipeline::DescriptorStager stager;
    Microsoft::WRL::ComPtr<ID3D12Resource> overlay, bgY, bgUv, upload;
    uint32_t pitch = 0;
    ID3D12Resource* boundY = nullptr;
    ID3D12Resource* boundUv = nullptr;
};

FrameOverlay::FrameOverlay() = default;
FrameOverlay::~FrameOverlay() = default;

bool FrameOverlay::create(ID3D12Device* device, uint32_t width, uint32_t height, bool hdr, const PushOverlayImage& image) {
    ready_ = false;
    p_ = std::make_unique<Impl>();
    auto& p = *p_;
    p.device = device; p.width = width; p.height = height; p.hdr = hdr;
    if (!device || width < 2 || height < 2 || image.width < 2 || image.height < 2 ||
        image.rgba.size() < size_t(image.width) * image.height * 4) return false;
    RecordingLabel scaled;
    const RecordingLabel* src = &image;
    if (image.width != width || image.height != height) {
        scaled = scaleRecordingLabel(image, width, height);
        if (scaled.rgba.size() < size_t(width) * height * 4) return false;
        src = &scaled;
    }
    const DXGI_FORMAT yFormat = hdr ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
    const DXGI_FORMAT uvFormat = hdr ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R8G8_UNORM;
    p.overlay = pipeline::makeTexture(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, false);
    p.bgY = pipeline::makeTexture(device, width, height, yFormat, false);
    p.bgUv = pipeline::makeTexture(device, width / 2, height / 2, uvFormat, false);
    if (!p.overlay || !p.bgY || !p.bgUv) return false;
    std::vector<uint8_t> shader;
    if (!p.pass.loadShader("PushCompareOverlay.dxil", shader) || !p.pass.create(device, shader, 5, 3, 2) || !p.stager.initialize(device, 3))
        return false;
    p.pitch = (width * 4 + kPitchAlign - 1) / kPitchAlign * kPitchAlign;
    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = uint64_t(p.pitch) * height; bd.Height = 1;
    bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&p.upload))))
        return false;
    uint8_t* mapped = nullptr;
    const D3D12_RANGE none{0, 0};
    if (FAILED(p.upload->Map(0, &none, reinterpret_cast<void**>(&mapped))) || !mapped) return false;
    for (uint32_t row = 0; row < height; ++row)
        std::memcpy(mapped + uint64_t(p.pitch) * row, &src->rgba[size_t(row) * width * 4], size_t(width) * 4);
    p.upload->Unmap(0, nullptr);
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};
    view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    view.Texture2D.MipLevels = 1;
    view.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    p.stager.stageSrv(p.overlay.Get(), &view, p.pass.heap.Get(), 0);
    view.Format = yFormat;
    p.stager.stageSrv(p.bgY.Get(), &view, p.pass.heap.Get(), 1);
    view.Format = uvFormat;
    p.stager.stageSrv(p.bgUv.Get(), &view, p.pass.heap.Get(), 2);
    ready_ = true;
    return true;
}

void FrameOverlay::apply(ID3D12GraphicsCommandList* list, pipeline::StateTracker& states, ID3D12Resource* y, ID3D12Resource* uv) {
    if (!ready_ || !list || !y || !uv) return;
    auto& p = *p_;
    if (!p.uploaded) {
        states.transition(list, p.overlay.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION from{}, to{};
        from.pResource = p.upload.Get(); from.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        from.PlacedFootprint.Footprint = {DXGI_FORMAT_R8G8B8A8_UNORM, p.width, p.height, 1, p.pitch};
        to.pResource = p.overlay.Get(); to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        list->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
        states.transition(list, p.overlay.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        p.uploaded = true;
    }
    if (p.boundY != y || p.boundUv != uv) {
        pipeline::makeUav(p.device, y, p.hdr ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM, pipeline::cpuHandleOf(p.pass, 3));
        pipeline::makeUav(p.device, uv, p.hdr ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R8G8_UNORM, pipeline::cpuHandleOf(p.pass, 4));
        p.boundY = y; p.boundUv = uv;
    }
    // The picture under the overlay, read by the blend (a UAV is never read here).
    states.transition(list, y, D3D12_RESOURCE_STATE_COPY_SOURCE);
    states.transition(list, uv, D3D12_RESOURCE_STATE_COPY_SOURCE);
    states.transition(list, p.bgY.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
    states.transition(list, p.bgUv.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION from{}, to{};
    from.Type = to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    from.pResource = y; to.pResource = p.bgY.Get();
    const D3D12_BOX yBox{0, 0, 0, p.width, p.height, 1};
    list->CopyTextureRegion(&to, 0, 0, 0, &from, &yBox);
    from.pResource = uv; to.pResource = p.bgUv.Get();
    const D3D12_BOX uvBox{0, 0, 0, p.width / 2, p.height / 2, 1};
    list->CopyTextureRegion(&to, 0, 0, 0, &from, &uvBox);
    states.transition(list, p.bgY.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    states.transition(list, p.bgUv.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    states.transition(list, y, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    states.transition(list, uv, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    const uint32_t constants[8] = {0, 0, p.width & ~1u, p.height & ~1u, p.hdr ? 1u : 0u, 0, 0, 0};
    float bits[8];
    std::memcpy(bits, constants, sizeof(bits));
    p.pass.bind(list, bits, pipeline::gpuHandleOf(p.pass, 0).ptr, pipeline::gpuHandleOf(p.pass, 3).ptr);
    list->Dispatch((p.width + 15) / 16, (p.height + 15) / 16, 1);
    states.uavBarrier(list, y);
    states.uavBarrier(list, uv);
}

} // namespace veyra::engine
