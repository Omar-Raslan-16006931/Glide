// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// A synthetic "console game" for trying Glide without a capture card and for the self-tests:
// a 30 FPS picture on a 60 Hz signal (every picture sent twice, or 1/3 times with uneven pacing),
// a panning landscape, moving objects and a still HUD.
#include <QImage>
#include <QThread>

#include <atomic>
#include <functional>

namespace pushfps {

class TestPattern : public QThread {
    Q_OBJECT
public:
    struct Options {
        int width = 1280, height = 720;
        double signalHz = 60;     // frames sent per second
        double contentFps = 30;   // new pictures per second
        bool uneven = false;      // 1/3 repeats instead of 2/2 (the 17/50 ms pattern)
    };
    using Sink = std::function<void(const QImage& frame, qint64 arrivalNs)>;
    TestPattern(Options options, Sink sink, QObject* parent = nullptr);
    ~TestPattern() override;
    void stop();
    // One picture of the scene at content time t (seconds); used by tests too.
    static QImage render(int width, int height, double t);

protected:
    void run() override;

private:
    Options o_;
    Sink sink_;
    std::atomic<bool> stop_{false};
};

}  // namespace pushfps
