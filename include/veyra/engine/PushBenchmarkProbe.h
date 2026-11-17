#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Glide "Optimise for my PC": a measurement window that sees every Present
// and every capture-to-screen latency sample the engine already records
// (FrameFlowWindow.h calls present() and latency()). Off until begin(); the
// hot-path cost while off is one relaxed atomic load.
//
// Numbers match what frame-time overlays report: the frame rate is presents per
// second between the first and last present of the window, the 1% low is the
// 99th-percentile Present-to-Present interval as a rate (gaps over half a second
// are pauses, as in FramePaceWindow), and latency is capture callback to Present
// return for real (not generated) frames - PC processing only, without the
// capture card, the display or the game.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>

namespace veyra::engine {

struct PushBenchmarkResult {
    bool valid = false;
    double seconds = 0, fps = 0, low1Fps = 0, low01Fps = 0, intervalStdMs = 0;
    double latencyMeanMs = 0, latencyP50Ms = 0, latencyP95Ms = 0;
    uint64_t presents = 0, generatedPresents = 0, realPresents = 0, latencySamples = 0, gaps = 0;
};

// Pure statistics, unit-tested on the CPU (tests/unit/PushBenchmarkTests.cpp).
inline double pushPercentile(std::vector<double> v, double p) {
    if (v.empty()) return 0;
    const size_t at = std::min(v.size() - 1, size_t(std::ceil(p * double(v.size()))) - (p > 0 ? 1 : 0));
    std::nth_element(v.begin(), v.begin() + std::ptrdiff_t(at), v.end());
    return v[at];
}
inline PushBenchmarkResult pushBenchmarkStatistics(const std::vector<int64_t>& presentTimes100ns, uint64_t generated,
                                                   const std::vector<double>& latencyMs) {
    constexpr int64_t kGap = 5000000;  // 0.5 s: a pause, not a slow frame
    PushBenchmarkResult r;
    r.presents = presentTimes100ns.size();
    r.generatedPresents = generated;
    r.realPresents = r.presents >= generated ? r.presents - generated : 0;
    std::vector<double> intervals;
    intervals.reserve(presentTimes100ns.size());
    double active = 0;
    for (size_t i = 1; i < presentTimes100ns.size(); ++i) {
        const int64_t d = presentTimes100ns[i] - presentTimes100ns[i - 1];
        if (d <= 0) continue;
        if (d > kGap) { ++r.gaps; continue; }
        intervals.push_back(double(d) / 10000.0);
        active += double(d) / 10000.0;
    }
    if (intervals.size() >= 30 && active > 0) {
        r.seconds = active / 1000.0;
        r.fps = double(intervals.size()) * 1000.0 / active;
        const double p99 = pushPercentile(intervals, 0.99), p999 = pushPercentile(intervals, 0.999);
        r.low1Fps = p99 > 0 ? 1000.0 / p99 : 0;
        r.low01Fps = p999 > 0 ? 1000.0 / p999 : 0;
        const double mean = active / double(intervals.size());
        double var = 0;
        for (double x : intervals) var += (x - mean) * (x - mean);
        r.intervalStdMs = std::sqrt(var / double(intervals.size()));
        r.valid = true;
    }
    r.latencySamples = latencyMs.size();
    if (!latencyMs.empty()) {
        double sum = 0;
        for (double x : latencyMs) sum += x;
        r.latencyMeanMs = sum / double(latencyMs.size());
        r.latencyP50Ms = pushPercentile(latencyMs, 0.5);
        r.latencyP95Ms = pushPercentile(latencyMs, 0.95);
    }
    return r;
}

class PushBenchmarkProbe {
public:
    void begin() {
        std::lock_guard lock(mutex_);
        presents_.clear(); latency_.clear(); generated_ = 0;
        presents_.reserve(16384); latency_.reserve(8192);
        active_.store(true, std::memory_order_release);
    }
    PushBenchmarkResult end() {
        active_.store(false, std::memory_order_release);
        std::lock_guard lock(mutex_);
        std::sort(presents_.begin(), presents_.end());
        return pushBenchmarkStatistics(presents_, generated_, latency_);
    }
    bool active() const { return active_.load(std::memory_order_relaxed); }
    // paceAt: when the Present call began (the same event the 1% low uses).
    void present(bool generated, int64_t paceAt100ns) {
        if (!active_.load(std::memory_order_relaxed)) return;
        std::lock_guard lock(mutex_);
        if (presents_.size() >= kMaxSamples) return;
        presents_.push_back(paceAt100ns);
        if (generated) ++generated_;
    }
    void latency(double ms) {
        if (!active_.load(std::memory_order_relaxed) || !std::isfinite(ms) || ms < 0) return;
        std::lock_guard lock(mutex_);
        if (latency_.size() < kMaxSamples) latency_.push_back(ms);
    }

private:
    static constexpr size_t kMaxSamples = 200000;
    std::atomic<bool> active_{false};
    std::mutex mutex_;
    std::vector<int64_t> presents_;
    std::vector<double> latency_;
    uint64_t generated_ = 0;
};

inline PushBenchmarkProbe& pushBenchmarkProbe() {
    static PushBenchmarkProbe probe;
    return probe;
}
}
