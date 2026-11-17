// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Glide streaming mode mixer (engine/PushAudioMix.h): faders, mute, the microphone filters
// (rumble filter, EQ, noise gate, compressor, limiter) and the level meters, without a device.
#include "veyra/engine/PushAudioMix.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace veyra::engine;

namespace {
int failures = 0;
void check(bool ok, const std::string& label) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label.c_str());
    if (!ok) ++failures;
}
constexpr double kPi = 3.14159265358979323846;
std::vector<float> tone(double hz, double amplitude, size_t frames) {
    std::vector<float> out(frames * 2);
    for (size_t i = 0; i < frames; ++i) {
        const float v = float(amplitude * std::sin(2 * kPi * hz * double(i) / 48000.0));
        out[i * 2] = out[i * 2 + 1] = v;
    }
    return out;
}
double peakOf(const std::vector<float>& pcm, size_t from = 0) {
    double p = 0;
    for (size_t i = from * 2; i < pcm.size(); ++i) p = std::max(p, double(std::abs(pcm[i])));
    return p;
}
double db(double v) { return 20 * std::log10(std::max(v, 1e-9)); }
} // namespace

int main() {
    // Faders.
    check(mixGain(0, false) == 1.0f, "0 dB is unity");
    check(mixGain(-60, false) == 0.0f && mixGain(0, true) == 0.0f, "bottom of the fader and mute are silence");
    check(std::abs(db(mixGain(-6, false)) + 6) < 0.01, "-6 dB");
    PushMixSettings wild; wild.gameDb = 99; wild.micDb = -999; wild.micGateDb = 5; wild.micEqDb[2] = 50; wild.micEqDb[3] = NAN;
    const auto clean = wild.sanitized();
    check(clean.gameDb == 12 && clean.micDb == -60 && clean.micGateDb == -10 && clean.micEqDb[2] == 12 && clean.micEqDb[3] == 0, "out-of-range settings are clamped");

    // Flat chain: a quiet tone passes unchanged.
    {
        PushMicProcessor p; p.configure({});
        auto pcm = tone(1000, 0.25, 4800);
        const auto in = pcm;
        p.process(pcm.data(), 4800);
        double err = 0; for (size_t i = 0; i < pcm.size(); ++i) err = std::max(err, double(std::abs(pcm[i] - in[i])));
        check(err < 1e-6, "flat chain leaves the voice untouched");
    }
    // Fader and mute glide (no click), then settle.
    {
        PushMicProcessor p; p.configure({});
        PushMixSettings s; s.micDb = -6;
        p.configure(s);
        auto pcm = tone(1000, 0.5, 9600);
        p.process(pcm.data(), 9600);
        check(std::abs(db(peakOf(pcm, 4800)) - db(0.25)) < 0.2, "-6 dB fader halves the level");
        check(std::abs(pcm[2]) > 0.0f || std::abs(pcm[4]) > 0.0f, "fader change glides instead of jumping");
        s.micMuted = true; p.configure(s);
        pcm = tone(1000, 0.5, 9600);
        p.process(pcm.data(), 9600);
        check(peakOf(pcm, 4800) < 1e-4, "mute is silence after the glide");
    }
    // Rumble filter cuts 30 Hz, keeps 1 kHz; EQ response matches the bands.
    {
        PushMicProcessor p; PushMixSettings s; s.micHighPass = true; p.configure(s);
        check(p.responseDb(30) < -10 && std::abs(p.responseDb(1000)) < 0.5, "high-pass: 30 Hz down, 1 kHz kept");
        s.micHighPass = false; s.micEq = true; s.micEqDb = {0, -4, 0, 5, 0}; p.configure(s);
        check(std::abs(p.responseDb(3500) - 5) < 0.6 && std::abs(p.responseDb(300) + 4) < 0.6, "EQ bands boost and cut where asked");
        s.micEq = false; p.configure(s);
        check(std::abs(p.responseDb(3500)) < 1e-6, "EQ off is flat");
    }
    // Noise gate: closes on room noise, opens on speech, holds through short pauses.
    {
        PushMicProcessor p; PushMixSettings s; s.micGate = true; s.micGateDb = -40; p.configure(s);
        auto noise = tone(500, 0.003, 24000);      // about -50 dBFS
        p.process(noise.data(), 24000);
        check(peakOf(noise, 19200) < 0.0005 && !p.gateOpen(), "gate closes on noise below the threshold");
        auto voice = tone(500, 0.2, 4800);          // about -14 dBFS
        p.process(voice.data(), 4800);
        check(p.gateOpen() && std::abs(db(peakOf(voice, 2400)) - db(0.2)) < 0.5, "gate opens on speech");
        auto pause = tone(500, 0.003, 2400);        // 50 ms pause: inside the 150 ms hold
        p.process(pause.data(), 2400);
        check(p.gateOpen(), "gate holds through a short pause");
    }
    // Compressor evens out loud speech; the limiter keeps the peak under -1 dBFS.
    {
        PushMicProcessor p; PushMixSettings s; s.micCompressor = true; p.configure(s);
        auto loud = tone(1000, 0.7, 24000);         // about -3 dBFS
        p.process(loud.data(), 24000);
        const double out = db(peakOf(loud, 12000));
        // 15 dB over -18 at 3:1 -> 5 dB over, +6 make-up: about -7 dBFS.
        check(out < -5 && out > -9, "compressor: -3 dBFS in, about -7 out (" + std::to_string(out) + ")");
        PushMicProcessor q; PushMixSettings boost; boost.micDb = 20; q.configure(boost);
        auto hot = tone(1000, 0.5, 24000);
        q.process(hot.data(), 24000);
        check(peakOf(hot) <= 0.8915, "limiter: never above -1 dBFS");
    }
    // Meters.
    {
        PushLevelMeter m;
        auto pcm = tone(1000, 0.5, 4800);
        m.feed(pcm.data(), 4800, 2);
        auto l = m.take();
        check(std::abs(l.peakDb - db(0.5)) < 0.1 && std::abs(l.rmsDb - db(0.5 / std::sqrt(2.0))) < 0.2, "meter: peak and RMS");
        check(m.take().peakDb == -100, "meter starts over after a read");
        m.feed(pcm.data(), 4800, 2, 0.5f);
        check(std::abs(m.take().peakDb - db(0.25)) < 0.1, "meter after the fader");
    }
    // Process-wide mix.
    {
        PushMixSettings s; s.gameDb = -6;
        const auto before = pushMixVersion();
        setPushMix(s);
        check(pushMixVersion() != before && std::abs(db(pushGameGain()) + 6) < 0.01, "game fader reaches the recorder's gain");
        s.gameMuted = true; setPushMix(s);
        check(pushGameGain() == 0.0f, "game mute");
        s.micMuted = true; setPushMix(s);
        auto pcm = tone(1000, 0.5, 9600);
        pushProcessMicrophone(pcm.data(), 9600);
        check(peakOf(pcm, 4800) < 1e-4 && pushMicMeter().take().peakDb < -60, "muted microphone is silent and meters silent");
        setPushMix({});
    }
    std::printf("%s (%d failures)\n", failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
