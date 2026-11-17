// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Glide comparison clip, side-by-side layout (engine/RecordingCompare.h): the recorder's
// NV12/P010 picture is rebuilt as two half-size copies of the whole frame, the held real frame
// on the left and the current frame on the right, centred vertically on black. Each half-size
// pixel is the average of the 2x2 source pixels it covers (luma and chroma alike).
Texture2D<float> currentY : register(t0);
Texture2D<float2> currentUv : register(t1);
Texture2D<float> heldY : register(t2);
Texture2D<float2> heldUv : register(t3);
RWTexture2D<float> outY : register(u0);
RWTexture2D<float2> outUv : register(u1);
cbuffer Constants : register(b0) {
    uint width, height;    // picture (luma) size
    uint pictureW, pictureH; // each half-size picture, even, = width/2 x height/2
    uint top, rightX;      // first picture row; first column of the right picture
    uint hdr, pad;         // 1: P010 (10-bit codes in the top bits)
};
float codes(float v) { return hdr ? round(v * 65535.0 / 64.0) * 64.0 / 65535.0 : v; }

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= width || id.y >= height) return;
    const float blackY = hdr ? 4096.0 / 65535.0 : 16.0 / 255.0;
    const float neutral = hdr ? 32768.0 / 65535.0 : 128.0 / 255.0;
    const bool rows = id.y >= top && id.y < top + pictureH;
    const bool left = rows && id.x < pictureW;
    const bool right = rows && id.x >= rightX && id.x < rightX + pictureW;
    const uint2 maxY = uint2(width - 1, height - 1);
    if (!left && !right) {
        outY[id.xy] = blackY;
    } else {
        const uint2 s = uint2((id.x - (left ? 0 : rightX)) * 2, (id.y - top) * 2);
        float sum = 0;
        [unroll] for (uint j = 0; j < 2; ++j)
            [unroll] for (uint i = 0; i < 2; ++i) {
                const uint2 p = min(s + uint2(i, j), maxY);
                sum += left ? heldY[p] : currentY[p];
            }
        outY[id.xy] = codes(sum * 0.25);
    }
    if ((id.x & 1) != 0 || (id.y & 1) != 0) return;
    const uint2 c = id.xy / 2;
    if (!left && !right) { outUv[c] = float2(neutral, neutral); return; }
    // Chroma: the picture's chroma texel covers 2x2 source chroma texels.
    const uint2 s = uint2(id.x - (left ? 0 : rightX), id.y - top);
    const uint2 maxUv = uint2(width / 2 - 1, height / 2 - 1);
    float2 sum = 0;
    [unroll] for (uint j = 0; j < 2; ++j)
        [unroll] for (uint i = 0; i < 2; ++i) {
            const uint2 p = min(s + uint2(i, j), maxUv);
            sum += left ? heldUv[p] : currentUv[p];
        }
    sum *= 0.25;
    outUv[c] = float2(codes(sum.x), codes(sum.y));
}
