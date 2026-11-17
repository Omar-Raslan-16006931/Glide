// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// Glide streaming mode audio mixer (qml/Veyra/StudioPage.qml), in the spirit of OBS Studio's
// audio mixer: what goes into recordings, the instant replay and live streams.
//
//  - Game sound: a fader (-60..+12 dB) and mute, applied to the sound the recorder takes
//    (LiveRecorder renderedPcm). What you hear yourself keeps the player's own volume, the way
//    OBS separates monitoring from the output mix.
//  - Microphone: a filter chain run once on the microphone capture thread (MicrophoneCapture.cpp)
//    before every recorder and stream gets the packet: rumble filter (80 Hz high-pass), 5-band EQ,
//    noise gate, compressor, fader with mute, then a peak limiter at -1 dBFS. Fader and mute
//    changes glide over ~10 ms so they never click.
//  - Level meters: peak and RMS of what each source sends (after its fader), read by the
//    interface ~20 times a second.
//
// The DSP is free of platform headers and unit-tested (tests/unit/PushAudioMixTests.cpp).
// 48 kHz interleaved float stereo, the format of the microphone capture and the sound tap.
#include "veyra/sink/SoundProfile.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace veyra::engine {

inline constexpr int kMicEqBands = 5;
inline constexpr std::array<float, kMicEqBands> kMicEqBandHz{100.0f, 300.0f, 1000.0f, 3500.0f, 10000.0f};
inline constexpr float kMicEqMaxDb = 12.0f;
inline constexpr float kMixFloorDb = -60.0f;     // the bottom of a fader: silence

struct PushMixSettings {
    float gameDb = 0; bool gameMuted = false;    // game sound in recordings and streams, -60..+12 dB
    float micDb = 0; bool micMuted = false;      // microphone, -60..+20 dB
    bool micHighPass = false;                    // 80 Hz rumble filter
    bool micEq = false;
    std::array<float, kMicEqBands> micEqDb{};    // -12..+12 dB per band
    bool micGate = false; float micGateDb = -45; // noise gate: opens above the threshold, -80..-10 dB
    bool micCompressor = false;                  // 3:1 above -18 dBFS, +6 dB make-up
    bool operator==(const PushMixSettings&) const = default;
    PushMixSettings sanitized() const {
        PushMixSettings s = *this;
        const auto clean = [](float v, float lo, float hi, float fallback) { return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback; };
        s.gameDb = clean(s.gameDb, kMixFloorDb, 12.0f, 0.0f);
        s.micDb = clean(s.micDb, kMixFloorDb, 20.0f, 0.0f);
        s.micGateDb = clean(s.micGateDb, -80.0f, -10.0f, -45.0f);
        for (float& g : s.micEqDb) g = clean(g, -kMicEqMaxDb, kMicEqMaxDb, 0.0f);
        return s;
    }
};

// Fader position in dB to a linear gain; the bottom of the fader is silence.
inline float mixGain(float db, bool muted) {
    if (muted || !std::isfinite(db) || db <= kMixFloorDb) return 0.0f;
    return float(std::pow(10.0, double(db) / 20.0));
}

// Microphone filter chain (capture thread). 48 kHz interleaved stereo.
class PushMicProcessor {
public:
    static constexpr double kRate = 48000.0;

    void configure(const PushMixSettings& wanted) {
        const PushMixSettings s = wanted.sanitized();
        if (configured_ && s == settings_) return;
        const bool filtersChanged = !configured_ || s.micHighPass != settings_.micHighPass || s.micEq != settings_.micEq || s.micEqDb != settings_.micEqDb;
        settings_ = s;
        if (filtersChanged) {
            using K = sink::Biquad::Kind;
            count_ = 0;
            if (s.micHighPass) filters_[size_t(count_++)] = sink::Biquad::make(K::HighPass, kRate, 80.0, 0.707, 0.0);
            if (s.micEq)
                for (int b = 0; b < kMicEqBands; ++b) {
                    const double db = s.micEqDb[size_t(b)];
                    if (db == 0.0) continue;
                    const K kind = b == 0 ? K::LowShelf : b == kMicEqBands - 1 ? K::HighShelf : K::Peaking;
                    filters_[size_t(count_++)] = sink::Biquad::make(kind, kRate, kMicEqBandHz[size_t(b)], b == 0 || b == kMicEqBands - 1 ? 0.9 : 1.0, db);
                }
            state_ = {};
        }
        target_ = mixGain(s.micDb, s.micMuted);
        if (!configured_) gain_ = target_;
        configured_ = true;
    }
    const PushMixSettings& settings() const { return settings_; }
    // The combined response of the rumble filter and EQ at `hz` (the interface's EQ curve, tests).
    double responseDb(double hz) const {
        double db = 0;
        for (int f = 0; f < count_; ++f) db += filters_[size_t(f)].responseDb(kRate, hz);
        return db;
    }
    // Whether the gate is open now (tests, the interface).
    bool gateOpen() const { return gateGain_ > 0.5; }

    void process(float* stereo, size_t frames) {
        if (!stereo || !frames) return;
        const double gateOpenDb = settings_.micGateDb, gateCloseDb = settings_.micGateDb - 6.0;
        const double gateAttack = std::exp(-1.0 / (kRate * 0.002)), gateRelease = std::exp(-1.0 / (kRate * 0.120));
        const double levelAttack = std::exp(-1.0 / (kRate * 0.001)), levelRelease = std::exp(-1.0 / (kRate * 0.050));
        const double compAttack = std::exp(-1.0 / (kRate * 0.005)), compRelease = std::exp(-1.0 / (kRate * 0.120));
        const double glide = std::exp(-1.0 / (kRate * 0.010));
        const double limitRelease = std::exp(-1.0 / (kRate * 0.080));
        constexpr double ceiling = 0.891;            // -1 dBFS
        const int holdFrames = int(kRate * 0.150);
        for (size_t i = 0; i < frames; ++i) {
            double x[2];
            double peak = 0;
            for (int c = 0; c < 2; ++c) {
                double v = std::isfinite(stereo[i * 2 + c]) ? stereo[i * 2 + c] : 0.0;
                auto& st = state_[size_t(c)];
                for (int f = 0; f < count_; ++f) {
                    const auto& q = filters_[size_t(f)];
                    double& s1 = st[size_t(2 * f)];
                    double& s2 = st[size_t(2 * f + 1)];
                    const double y = q.b0 * v + s1;
                    s1 = q.b1 * v - q.a1 * y + s2;
                    s2 = q.b2 * v - q.a2 * y;
                    v = y;
                }
                x[c] = v;
                peak = (std::max)(peak, std::abs(v));
            }
            // Noise gate on a fast peak follower, with hysteresis and a 150 ms hold.
            double gain = 1.0;
            if (settings_.micGate) {
                level_ = peak > level_ ? peak + levelAttack * (level_ - peak) : peak + levelRelease * (level_ - peak);
                const double levelDb = 20.0 * std::log10((std::max)(level_, 1e-6));
                if (levelDb >= gateOpenDb) { open_ = true; hold_ = holdFrames; }
                else if (levelDb < gateCloseDb) { if (hold_ > 0) --hold_; else open_ = false; }
                const double want = open_ ? 1.0 : 0.0;
                gateGain_ = want + (want > gateGain_ ? gateAttack : gateRelease) * (gateGain_ - want);
                gain *= gateGain_;
            } else {
                gateGain_ = 1.0;
            }
            // Compressor: 3:1 above -18 dBFS, +6 dB make-up.
            if (settings_.micCompressor) {
                const double levelDb = 20.0 * std::log10((std::max)(peak * gain, 1e-6));
                envDb_ = levelDb + (levelDb > envDb_ ? compAttack : compRelease) * (envDb_ - levelDb);
                const double over = envDb_ - (-18.0);
                gain *= std::pow(10.0, ((over > 0 ? over / 3.0 - over : 0.0) + 6.0) / 20.0);
            }
            // Fader with mute, gliding to the new position.
            gain_ = target_ + glide * (gain_ - target_);
            gain *= gain_;
            // Peak limiter at -1 dBFS: instant attack, 80 ms release.
            const double out = peak * gain;
            const double needed = out > ceiling ? ceiling / out : 1.0;
            limit_ = needed < limit_ ? needed : 1.0 + limitRelease * (limit_ - 1.0);
            limit_ = (std::min)(limit_, needed);
            gain *= limit_;
            stereo[i * 2] = float(x[0] * gain);
            stereo[i * 2 + 1] = float(x[1] * gain);
        }
    }

private:
    PushMixSettings settings_{};
    std::array<sink::Biquad, 1 + kMicEqBands> filters_{};
    int count_ = 0;
    std::array<std::array<double, 2 * (1 + kMicEqBands)>, 2> state_{};
    double level_ = 0, gateGain_ = 1, envDb_ = -120, gain_ = 1, target_ = 1, limit_ = 1;
    int hold_ = 0;
    bool open_ = false, configured_ = false;
};

// A source's level, in dBFS (-100 = silence).
struct PushLevel { float peakDb = -100, rmsDb = -100; };

// Fed by one audio thread, read by the interface: the loudest sample and the mean square since
// the last read. take() returns them and starts over.
class PushLevelMeter {
public:
    void feed(const float* pcm, size_t frames, unsigned channels, float gain = 1.0f) {
        if (!pcm || !frames || !channels) return;
        float peak = 0; double sum = 0;
        const size_t n = frames * channels;
        for (size_t i = 0; i < n; ++i) {
            const float v = std::isfinite(pcm[i]) ? std::abs(pcm[i] * gain) : 0.0f;
            peak = (std::max)(peak, v);
            sum += double(v) * v;
        }
        float seen = peak_.load(std::memory_order_relaxed);
        while (peak > seen && !peak_.compare_exchange_weak(seen, peak, std::memory_order_relaxed)) {}
        double total = sum_.load(std::memory_order_relaxed);
        while (!sum_.compare_exchange_weak(total, total + sum, std::memory_order_relaxed)) {}
        samples_.fetch_add(n, std::memory_order_relaxed);
    }
    PushLevel take() {
        const float peak = peak_.exchange(0.0f, std::memory_order_relaxed);
        const double sum = sum_.exchange(0.0, std::memory_order_relaxed);
        const uint64_t n = samples_.exchange(0, std::memory_order_relaxed);
        const auto db = [](double v) { return v > 1e-5 ? float(20.0 * std::log10(v)) : -100.0f; };
        return {db(peak), n ? db(std::sqrt(sum / double(n))) : -100.0f};
    }
private:
    std::atomic<float> peak_{0.0f};
    std::atomic<double> sum_{0.0};
    std::atomic<uint64_t> samples_{0};
};

// Process-wide mix, set by the interface (preferences mix*, mic*).
void setPushMix(const PushMixSettings& settings);
PushMixSettings pushMix();
uint64_t pushMixVersion();
// Gain for the game sound in recordings and streams (0 when muted).
float pushGameGain();
PushLevelMeter& pushGameMeter();
PushLevelMeter& pushMicMeter();
// Microphone capture thread: runs the filter chain on a packet in place and meters it.
void pushProcessMicrophone(float* stereo, size_t frames);

} // namespace veyra::engine
