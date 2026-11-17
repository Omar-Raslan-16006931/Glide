// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// The Linux / macOS frame pipeline without Qt: capture frames in, "what to show at this moment" out.
//
//   push(frame, arrival)   capture thread: duplicate removal, Frame Pacing Repair, luma pyramid;
//                          a worker thread then updates Live HUD Lock and estimates motion both ways
//                          between the last two real frames.
//   pick(displayTime)      render thread, once per refresh: the pair of real frames and the phase t
//                          for that moment, with its motion fields. Never blocks on the worker.
//
// Timing: generated frames sit between two real frames, so the picture runs one real-frame interval
// (plus the motion estimation time, plus up to one capture refresh with Frame Pacing Repair) behind
// the capture, like every frame generation. With frame generation off, the newest frame is shown at
// once. Times are nanoseconds on one steady clock.
#include "veyra/portable/HudLock.h"
#include "veyra/portable/Image.h"
#include "veyra/portable/Motion.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace veyra::portable {

struct EngineSettings {
    bool frameGeneration = true;
    double targetFps = 0;        // 0: one new picture every display refresh ("match display")
    bool removeDuplicates = true;
    bool pacingRepair = false;
    bool hudLock = false;
    int quality = 1;             // 0 fast (vectors from the quarter-size picture), 1 balanced (half size)
    double estimateBudgetMs = 0; // headroom for motion estimation in the display delay; 0 = measured
};

struct ContentFrame {
    uint64_t id = 0;
    int64_t time = 0;     // content time after Frame Pacing Repair
    int64_t arrival = 0;  // when the capture delivered it
    std::shared_ptr<const Bgra> image;
    std::shared_ptr<const Pyramid> pyramid;
};

struct PairFields {
    uint64_t aId = 0, bId = 0;
    MotionField forward, backward;
    std::shared_ptr<const Gray> hudMask;  // null when Live HUD Lock is off
    double estimateMs = 0;
};

struct Pick {
    std::shared_ptr<const Bgra> a, b;          // b == a when only one real frame is shown
    std::shared_ptr<const PairFields> fields;  // null: show the nearer real frame (no generation)
    uint64_t aId = 0, bId = 0;
    float t = 0;                               // 0 = a, 1 = b
    bool generated() const { return fields && t > 0.f && t < 1.f && a != b; }
    bool valid() const { return a != nullptr; }
};

struct EngineStats {
    double transportFps = 0;   // frames per second arriving from the capture
    double contentFps = 0;     // real (non-duplicate) pictures per second
    double outputFps = 0;      // different pictures shown per second
    double latencyMs = 0;      // display time minus arrival of the newest real frame it needs
    double estimateMs = 0;     // motion estimation time per pair (both directions)
    double delayMs = 0;        // current display delay behind the content
    uint64_t framesIn = 0, duplicates = 0, kept = 0, sceneCuts = 0, pacingRepaired = 0;
    uint64_t lateFields = 0;   // refreshes that showed a real frame because the motion was not ready yet
    uint64_t picks = 0;        // refreshes asked for
    int width = 0, height = 0;
};

class Engine {
public:
    explicit Engine(EngineSettings settings = {});
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void setSettings(const EngineSettings& settings);
    EngineSettings settings() const;
    // Capture thread (one producer). luma: full-size luma of the frame; image: called only for frames
    // that are not duplicates, returns the BGRA picture (same size as luma). A size change starts over.
    void push(Gray&& luma, const std::function<Bgra()>& image, int64_t arrivalNs);
    // Convenience: luma computed from the image.
    void push(Bgra&& image, int64_t arrivalNs);
    // Render thread, once per refresh. refreshHz: the display's refresh rate (for the output count).
    Pick pick(int64_t displayNs);
    EngineStats stats() const;
    // Drops all frames (source change, resolution change).
    void reset();
    // Tests: waits until the worker has finished every queued pair.
    void waitIdle();

private:
    struct Job { std::shared_ptr<const ContentFrame> a, b; };
    void workerLoop();
    void resetLocked();

    mutable std::mutex m_;
    std::condition_variable cv_, idle_;
    EngineSettings s_;
    std::deque<std::shared_ptr<const ContentFrame>> frames_;
    std::deque<std::shared_ptr<const PairFields>> pairs_;
    std::deque<Job> jobs_;
    bool busy_ = false, stop_ = false, hudReset_ = false;
    uint64_t nextId_ = 1, generation_ = 0;
    std::shared_ptr<const Gray> lastQuarter_;
    int64_t lastArrival_ = 0, lastKeptArrival_ = 0, lastContent_ = 0;
    int lastWidth_ = 0, lastHeight_ = 0;
    std::deque<int64_t> arrivals_, keptTimes_;
    double delayNs_ = 0;
    double lateMarginMs_ = 0;  // extra delay this machine needed (motion not ready in time)
    double estimateMsAvg_ = 0;
    // Output bookkeeping (render thread under the same lock).
    uint64_t lastShownA_ = 0, lastShownB_ = 0, lastNeededId_ = 0;
    float lastShownT_ = -1;
    std::deque<int64_t> shownTimes_, pickTimes_;
    int64_t lastPick_ = 0;
    double displayHz_ = 0;
    double latencyAvgMs_ = 0;
    EngineStats st_;
    struct Pacing;
    std::unique_ptr<Pacing> pacing_;
    HudLock hud_;  // worker thread only
    MotionField lastForward_, lastBackward_;
    std::thread worker_;
};

}  // namespace veyra::portable
