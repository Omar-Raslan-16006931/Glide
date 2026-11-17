// SPDX-License-Identifier: GPL-3.0-or-later
// Glide plug and play: the capture format picked when nobody chose one.
#include "veyra/source/PushAutoCaptureFormat.h"
#include <iostream>
#include <vector>

using namespace veyra::source;

static CaptureFormat mode(unsigned w, unsigned h, double fps, const wchar_t* key, const wchar_t* pixel, int tier, int rank) {
    CaptureFormat f;
    f.width = w; f.height = h; f.fps = fps; f.key = key; f.pixel = pixel; f.tier = tier; f.rank = rank;
    return f;
}

int main() {
    int failures = 0, checks = 0;
    auto check = [&](bool ok, const char* name) { ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << name << '\n'; } };
    auto key = [](const CaptureFormat* f) { return f ? f->key : std::wstring(L"<none>"); };

    // A typical Elgato-style list: many sizes, rates and pixel formats.
    std::vector<CaptureFormat> elgato{
        mode(3840, 2160, 60, L"4k60-nv12", L"NV12", 0, 0),
        mode(1920, 1080, 30, L"1080p30-nv12", L"NV12", 0, 0),
        mode(1920, 1080, 60, L"1080p60-yuy2", L"YUY2", 0, 10),
        mode(1920, 1080, 60, L"1080p60-mjpg", L"MJPG", 3, 100),
        mode(1920, 1080, 60, L"1080p60-nv12", L"NV12", 0, 0),
        mode(1280, 720, 60, L"720p60-nv12", L"NV12", 0, 0),
    };
    check(key(pushAutoCaptureFormat(elgato)) == L"1080p60-nv12", "1080p60 NV12 wins");

    // 59.94 counts as 60 and beats 4K60 and 1080p30.
    std::vector<CaptureFormat> ntsc{mode(1920, 1080, 59.94, L"1080p5994", L"YUY2", 0, 10),
                                    mode(1920, 1080, 30, L"1080p30", L"NV12", 0, 0),
                                    mode(3840, 2160, 60, L"4k60", L"NV12", 0, 0)};
    check(key(pushAutoCaptureFormat(ntsc)) == L"1080p5994", "59.94 is treated as 60");

    // Only 4K offered at 60: take it rather than 1080p30.
    std::vector<CaptureFormat> only4k{mode(1920, 1080, 30, L"1080p30", L"NV12", 0, 0), mode(3840, 2160, 60, L"4k60", L"P010", 0, 1)};
    check(key(pushAutoCaptureFormat(only4k)) == L"4k60", "60 fps beats a smaller size");

    // MJPEG only when nothing else exists.
    std::vector<CaptureFormat> cheapCard{mode(1920, 1080, 60, L"mjpg60", L"MJPG", 3, 100), mode(1920, 1080, 30, L"yuy230", L"YUY2", 0, 10)};
    check(key(pushAutoCaptureFormat(cheapCard)) == L"yuy230", "uncompressed 30 beats MJPEG 60");
    std::vector<CaptureFormat> mjpgOnly{mode(1920, 1080, 60, L"mjpg60", L"MJPG", 3, 100)};
    check(key(pushAutoCaptureFormat(mjpgOnly)) == L"mjpg60", "MJPEG when it is the only mode");

    // Interlaced loses; unsupported is never picked.
    auto interlaced = mode(1920, 1080, 60, L"1080i60", L"NV12", 0, 0); interlaced.interlaced = true;
    auto unsupported = mode(1920, 1080, 60, L"bad", L"NV12", 0, 0); unsupported.supported = false;
    std::vector<CaptureFormat> mixed{interlaced, unsupported, mode(1280, 720, 60, L"720p60", L"NV12", 0, 0)};
    check(key(pushAutoCaptureFormat(mixed)) == L"720p60", "progressive 720p beats 1080i, unsupported skipped");

    // A mode that follows the real signal always wins.
    auto follow = mode(0, 0, 0, L"auto", L"NV12", 0, 0); follow.automatic = true;
    std::vector<CaptureFormat> magewell{mode(1920, 1080, 60, L"1080p60", L"NV12", 0, 0), follow};
    check(key(pushAutoCaptureFormat(magewell)) == L"auto", "signal-following mode wins");

    // Driver default breaks a full tie.
    auto a = mode(1920, 1080, 60, L"a", L"NV12", 0, 0), b = mode(1920, 1080, 60, L"b", L"NV12", 0, 0);
    b.driverCurrent = true;
    std::vector<CaptureFormat> tie{a, b};
    check(key(pushAutoCaptureFormat(tie)) == L"b", "driver default breaks a tie");

    std::vector<CaptureFormat> empty;
    check(pushAutoCaptureFormat(empty) == nullptr, "empty list gives nothing");
    std::vector<CaptureFormat> none{unsupported};
    check(pushAutoCaptureFormat(none) == nullptr, "all unsupported gives nothing");

    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
