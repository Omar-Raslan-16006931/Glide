// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// "Optimise for my PC" measurement statistics (engine/PushBenchmarkProbe.h), on synthetic present streams.
#include "veyra/engine/PushBenchmarkProbe.h"
#include <cstdio>
#include <string>
#include <thread>

using namespace veyra::engine;
static int failures = 0;
static void check(bool ok, const std::string& label) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label.c_str());
    if (!ok) ++failures;
}
static bool near(double a, double b, double tol) { return std::abs(a - b) <= tol; }

int main() {
    // A perfect 120 FPS stream for 5 s.
    std::vector<int64_t> t;
    for (int i = 0; i <= 600; ++i) t.push_back(int64_t(i) * 83333);
    auto r = pushBenchmarkStatistics(t, 300, {});
    check(r.valid && near(r.fps, 120.0, 0.05), "steady 120 FPS measures 120");
    check(near(r.low1Fps, 120.0, 0.1), "steady stream: 1% low equals the average");
    check(r.generatedPresents == 300 && r.realPresents == 301, "generated and real presents counted");
    check(r.intervalStdMs < 0.01, "steady stream has no jitter");

    // Same stream, every 50th interval is a 25 ms hitch (2 % of frames).
    std::vector<int64_t> h; int64_t now = 0;
    for (int i = 0; i < 600; ++i) { h.push_back(now); now += (i % 50 == 49) ? 250000 : 83333; }
    r = pushBenchmarkStatistics(h, 0, {});
    check(r.valid && r.fps < 120 && r.fps > 100, "hitches lower the average");
    check(near(r.low1Fps, 40.0, 0.5), "2 % hitches of 25 ms give a 40 FPS 1% low");

    // A pause (seek, settings change) is not a slow frame.
    std::vector<int64_t> p;
    for (int i = 0; i < 300; ++i) p.push_back(int64_t(i) * 166667);
    for (int i = 0; i < 300; ++i) p.push_back(int64_t(300) * 166667 + 20000000 + int64_t(i) * 166667);
    r = pushBenchmarkStatistics(p, 0, {});
    check(r.gaps == 1 && near(r.fps, 60.0, 0.05) && near(r.low1Fps, 60.0, 0.1), "a 2 s pause is excluded, 60 FPS stays 60");

    // Too few frames is not a measurement.
    r = pushBenchmarkStatistics({0, 166667, 333333}, 0, {});
    check(!r.valid, "three presents are not enough");

    // Latency statistics.
    std::vector<double> lat;
    for (int i = 1; i <= 100; ++i) lat.push_back(double(i));
    r = pushBenchmarkStatistics(t, 0, lat);
    check(near(r.latencyMeanMs, 50.5, 1e-9) && near(r.latencyP50Ms, 50, 1e-9) && near(r.latencyP95Ms, 95, 1e-9),
          "latency mean, median and 95th percentile");

    // The probe itself: off records nothing; begin/end; concurrent writers.
    auto& probe = pushBenchmarkProbe();
    probe.present(false, 1); probe.latency(5);
    probe.begin();
    auto r0 = probe.end();
    check(r0.presents == 0 && r0.latencySamples == 0, "nothing is recorded while the probe is off");
    probe.begin();
    std::thread a([&] { for (int i = 0; i < 400; ++i) probe.present(i % 2 == 1, int64_t(i) * 83333 * 2); });
    std::thread b([&] { for (int i = 0; i < 400; ++i) probe.present(false, int64_t(i) * 83333 * 2 + 83333); });
    std::thread c([&] { for (int i = 0; i < 200; ++i) probe.latency(10.0 + (i % 5)); });
    a.join(); b.join(); c.join();
    probe.latency(-1); probe.latency(std::nan(""));
    auto r1 = probe.end();
    check(r1.presents == 800 && r1.generatedPresents == 200 && r1.latencySamples == 200, "concurrent presents and latency samples all counted");
    check(r1.valid && near(r1.fps, 120.0, 0.1), "out-of-order presents from two threads are sorted before measuring");
    check(!probe.active(), "end() stops the probe");
    std::printf("%s: %d failure(s)\n", failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
