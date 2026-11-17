// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "veyra/engine/PushAudioMix.h"

#include <mutex>

namespace veyra::engine {
namespace {
std::mutex mixMutex;
PushMixSettings mixSettings;
std::atomic<uint64_t> mixVersion{1};
std::atomic<float> gameGain{1.0f};
PushLevelMeter gameMeter, micMeter;
} // namespace

void setPushMix(const PushMixSettings& settings) {
    const PushMixSettings s = settings.sanitized();
    {
        std::lock_guard lock(mixMutex);
        if (s == mixSettings) return;
        mixSettings = s;
    }
    gameGain.store(mixGain(s.gameDb, s.gameMuted));
    mixVersion.fetch_add(1);
}
PushMixSettings pushMix() { std::lock_guard lock(mixMutex); return mixSettings; }
uint64_t pushMixVersion() { return mixVersion.load(); }
float pushGameGain() { return gameGain.load(); }
PushLevelMeter& pushGameMeter() { return gameMeter; }
PushLevelMeter& pushMicMeter() { return micMeter; }

void pushProcessMicrophone(float* stereo, size_t frames) {
    // One microphone capture thread at a time (MicrophoneCapture's hub); the processor keeps the
    // filter state between packets.
    static PushMicProcessor processor;
    static uint64_t seen = 0;
    const uint64_t version = mixVersion.load();
    if (version != seen) { processor.configure(pushMix()); seen = version; }
    processor.process(stereo, frames);
    micMeter.feed(stereo, frames, 2);
}

} // namespace veyra::engine
