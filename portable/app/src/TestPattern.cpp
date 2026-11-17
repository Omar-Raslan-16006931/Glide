// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "TestPattern.h"

#include "VideoFrames.h"

#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace pushfps {

TestPattern::TestPattern(Options options, Sink sink, QObject* parent) : QThread(parent), o_(options), sink_(std::move(sink)) {}

TestPattern::~TestPattern() {
    stop();
    wait();
}

void TestPattern::stop() { stop_ = true; }

QImage TestPattern::render(int width, int height, double t) {
    QImage img(width, height, QImage::Format_RGB32);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    const double sx = width / 1280.0, sy = height / 720.0;
    p.scale(sx, sy);
    // Sky.
    QLinearGradient sky(0, 0, 0, 430);
    sky.setColorAt(0, QColor(28, 34, 52));
    sky.setColorAt(1, QColor(96, 112, 140));
    p.fillRect(QRectF(0, 0, 1280, 430), sky);
    // Far hills (slow), near buildings (faster): the camera pans right.
    const double pan = t * 180.0;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(52, 66, 80));
    QPainterPath hills;
    hills.moveTo(0, 430);
    for (int x = 0; x <= 1280; x += 16) {
        const double u = (x + pan * 0.3) * 0.01;
        hills.lineTo(x, 330 - 40 * std::sin(u) - 22 * std::sin(u * 2.7 + 1.3));
    }
    hills.lineTo(1280, 430);
    hills.closeSubpath();
    p.drawPath(hills);
    for (int i = -2; i < 16; ++i) {
        const double x = std::fmod(i * 110.0 - pan, 1760.0) + (i * 110.0 - pan < 0 ? 1760.0 : 0.0) - 240.0;
        const int k = (i % 7 + 7) % 7;
        const double h = 120 + k * 23;
        p.setBrush(QColor(40 + k * 9, 44 + k * 6, 58 + k * 5));
        p.drawRect(QRectF(x, 430 - h, 92, h));
        p.setBrush(QColor(230, 200, 120));
        for (int wy = 0; wy < int(h / 26) - 1; ++wy)
            for (int wx = 0; wx < 3; ++wx)
                if ((wy * 3 + wx + k) % 4 != 0) p.drawRect(QRectF(x + 12 + wx * 26, 430 - h + 14 + wy * 26, 12, 12));
    }
    // Road with stripes moving faster.
    p.setBrush(QColor(44, 44, 48));
    p.drawRect(QRectF(0, 430, 1280, 290));
    p.setBrush(QColor(235, 235, 220));
    for (int i = -1; i < 12; ++i) {
        const double x = std::fmod(i * 140.0 - pan * 2.2, 1820.0);
        p.drawRect(QRectF(x < -140 ? x + 1820 : x, 560, 80, 12));
    }
    // A car going left over the road, a ball bouncing.
    const double carX = 1280 - std::fmod(t * 420.0, 1700.0);
    p.setBrush(QColor(200, 40, 50));
    p.drawRoundedRect(QRectF(carX, 600, 220, 60), 14, 14);
    p.setBrush(QColor(20, 20, 24));
    p.drawEllipse(QPointF(carX + 50, 664), 22, 22);
    p.drawEllipse(QPointF(carX + 170, 664), 22, 22);
    const double by = 480 - std::abs(std::sin(t * 3.1)) * 240;
    p.setBrush(QColor(141, 242, 55));
    p.drawEllipse(QPointF(640 + 360 * std::sin(t * 0.9), by), 34, 34);
    // HUD: never moves (what Live HUD Lock keeps sharp).
    p.setBrush(QColor(0, 0, 0, 140));
    p.drawRoundedRect(QRectF(28, 24, 330, 64), 10, 10);
    p.setBrush(QColor(60, 220, 90));
    p.drawRect(QRectF(44, 64, 220, 10));
    p.setPen(QColor(245, 245, 245));
    QFont f;
    f.setPixelSize(26);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRectF(44, 28, 300, 34), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("PUSHFPS TEST"));
    p.setPen(QPen(QColor(255, 255, 255), 3));
    p.drawLine(QPointF(640 - 14, 360), QPointF(640 + 14, 360));
    p.drawLine(QPointF(640, 360 - 14), QPointF(640, 360 + 14));
    p.end();
    return img;
}

void TestPattern::run() {
    using clock = std::chrono::steady_clock;
    const auto period = std::chrono::nanoseconds(qint64(1e9 / std::max(1.0, o_.signalHz)));
    auto next = clock::now();
    QImage current;
    int left = 0, picture = 0;
    while (!stop_) {
        if (left == 0) {
            ++picture;
            current = render(o_.width, o_.height, double(picture) / std::max(1.0, o_.contentFps));
            const int repeats = std::max(1, int(std::lround(o_.signalHz / std::max(1.0, o_.contentFps))));
            left = o_.uneven && repeats == 2 ? ((picture % 2) ? 3 : 1) : repeats;
        }
        --left;
        std::this_thread::sleep_until(next);
        if (stop_) break;
        sink_(current, nowNs());
        next += period;
        // Fell far behind (debugger, suspended machine): don't burst to catch up.
        if (clock::now() - next > period * 4) next = clock::now();
    }
}

}  // namespace pushfps
