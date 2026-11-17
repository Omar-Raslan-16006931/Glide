// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Glide comparison clip (engine/RecordingCompare.h): the raw console picture (the graph's
// untouched linear BT.709 copy of the input, scRGB units: 1 = 80 nits) scaled to the recorded
// size and coded exactly as RgbToNv12.hlsl codes the enhanced picture: SDR as BT.709
// limited-range 8-bit from sRGB, HDR as BT.2020 PQ 10-bit codes. Encoding follows the
// presenter's comparison view (PresentBlit flags 1, 4 and 8).
#include "HdrColor.hlsli"
Texture2D<float4> raw : register(t0);
RWTexture2D<float> outY : register(u0);
RWTexture2D<float2> outUv : register(u1);
SamplerState linearClamp : register(s1);
cbuffer Constants : register(b0) {
    uint width, height;  // recorded (luma) size
    uint hdr, sdrInHdr;  // 1: P010 PQ output; 1: an SDR source shown in HDR (203-nit white)
    uint pad0, pad1, pad2, pad3;
};
float3 encoded(float3 c) {
    if (hdr) return HdrEncodePq(sdrInHdr ? c * (203.0 / 80.0) : c);
    c = max(c, 0);
    return saturate(lerp(1.055 * pow(c, 1.0 / 2.4) - 0.055, c * 12.92, step(c, 0.0031308)));
}
float3 coded(float3 e) {
    if (hdr) {
        float y = dot(e, float3(.2627, .6780, .0593));
        return round(float3(64 + 876 * y, 512 + 896 * (e.b - y) / 1.8814, 512 + 896 * (e.r - y) / 1.4746)) * 64.0 / 65535.0;
    }
    float y = dot(e, float3(0.2126, 0.7152, 0.0722));
    return float3((16 + 219 * y) / 255.0, (128 + 224 * (e.b - y) / 1.8556) / 255.0, (128 + 224 * (e.r - y) / 1.5748) / 255.0);
}
float3 at(uint2 p) { return encoded(raw.SampleLevel(linearClamp, (float2(p) + 0.5) / float2(width, height), 0).rgb); }

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= width || id.y >= height) return;
    outY[id.xy] = coded(at(id.xy)).x;
    if ((id.x & 1) != 0 || (id.y & 1) != 0) return;
    const uint2 last = uint2(width - 1, height - 1);
    const float3 c = (at(id.xy) + at(min(id.xy + uint2(1, 0), last)) + at(min(id.xy + uint2(0, 1), last)) + at(min(id.xy + uint2(1, 1), last))) * 0.25;
    outUv[id.xy / 2] = coded(saturate(c)).yz;
}
