// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Glide comparison clip, vertical layout for TikTok, YouTube Shorts and Reels
// (engine/RecordingCompare.h): a 9:16 picture of its own size, the held real frame (the console)
// in the top half and the current frame (Glide) in the bottom half. Each half shows the middle
// of the frame (crop, 9:8) scaled to fill it. Every output texel averages an N x N grid of
// bilinear taps spread over the source area it covers (N = the scale, 1..4), so a 4K frame
// shrinks without shimmering; luma and chroma alike, on the recorder's NV12/P010 planes.
Texture2D<float> currentY : register(t0);
Texture2D<float2> currentUv : register(t1);
Texture2D<float> heldY : register(t2);
Texture2D<float2> heldUv : register(t3);
RWTexture2D<float> outY : register(u0);
RWTexture2D<float2> outUv : register(u1);
SamplerState linearClamp : register(s1);
cbuffer Constants : register(b0) {
    uint outW, outH;        // output (luma) size, 9:16
    uint srcW, srcH;        // the frame's (luma) size
    uint cropX, cropY;      // the part of the frame each half shows
    uint cropW, cropH;
    uint pictureH;          // each half's height (= outH / 2)
    uint hdr;               // 1: P010 (10-bit codes in the top bits)
    uint pad0, pad1;
};
float codes(float v) { return hdr ? round(v * 65535.0 / 64.0) * 64.0 / 65535.0 : v; }

// The source area under an output position (cx, cy in output luma pixels, picture-local rows),
// as normalised texture coordinates of its centre and the footprint in source luma pixels.
void footprint(float cx, float cy, out float2 centre, out float2 size) {
    size = float2(float(cropW) / float(outW), float(cropH) / float(pictureH));
    centre = float2(float(cropX) + cx * size.x, float(cropY) + cy * size.y) / float2(srcW, srcH);
}
uint taps(float2 size) { return clamp((uint)ceil(max(size.x, size.y)), 1u, 4u); }

float lumaAt(bool top, float2 centre, float2 size) {
    const uint n = taps(size);
    const float2 step = size / float2(srcW, srcH) / float(n);
    float sum = 0;
    for (uint j = 0; j < n; ++j)
        for (uint i = 0; i < n; ++i) {
            const float2 uv = centre + (float2(i, j) + 0.5 - 0.5 * float(n)) * step;
            sum += top ? heldY.SampleLevel(linearClamp, uv, 0) : currentY.SampleLevel(linearClamp, uv, 0);
        }
    return sum / float(n * n);
}
float2 chromaAt(bool top, float2 centre, float2 size) {
    // A chroma texel of the output covers 2 x 2 output pixels, i.e. `size` source chroma texels.
    const uint n = taps(size);
    const float2 step = 2.0 * size / float2(srcW, srcH) / float(n);
    float2 sum = 0;
    for (uint j = 0; j < n; ++j)
        for (uint i = 0; i < n; ++i) {
            const float2 uv = centre + (float2(i, j) + 0.5 - 0.5 * float(n)) * step;
            sum += top ? heldUv.SampleLevel(linearClamp, uv, 0) : currentUv.SampleLevel(linearClamp, uv, 0);
        }
    return sum / float(n * n);
}

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= outW || id.y >= outH) return;
    const uint bottomTop = outH - pictureH;
    const bool top = id.y < pictureH;
    const float row = top ? float(id.y) : float(id.y) - float(bottomTop);
    float2 centre, size;
    footprint(float(id.x) + 0.5, row + 0.5, centre, size);
    outY[id.xy] = codes(lumaAt(top, centre, size));
    if ((id.x & 1) != 0 || (id.y & 1) != 0) return;
    // The chroma texel's centre is the middle of its 2 x 2 output pixels.
    footprint(float(id.x) + 1.0, row + 1.0, centre, size);
    const float2 c = chromaAt(top, centre, size);
    outUv[id.xy / 2] = float2(codes(c.x), codes(c.y));
}
