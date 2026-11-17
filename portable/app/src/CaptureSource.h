// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// The picture source: a capture card (UVC camera on Linux through V4L2, on macOS through
// AVFoundation; both via Qt Multimedia), a video file, or the built-in test picture. Frames are read
// on a worker thread with a short queue, so the capture never waits for the engine.
#include "TestPattern.h"

#include "veyra/portable/Engine.h"

#include <QCameraDevice>
#include <QCameraFormat>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVideoFrame>

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

class QCamera;
class QMediaCaptureSession;
class QMediaPlayer;
class QVideoSink;
class QAudioOutput;

namespace pushfps {

class CaptureSource : public QObject {
    Q_OBJECT
public:
    enum class Kind { None, Camera, File, TestPattern };
    explicit CaptureSource(std::shared_ptr<veyra::portable::Engine> engine, QObject* parent = nullptr);
    ~CaptureSource() override;

    bool startCamera(const QCameraDevice& device, const QCameraFormat& format);
    bool startFile(const QUrl& url, float volume, bool withSound = true);
    bool startTestPattern(const TestPattern::Options& options);
    void stop();
    Kind kind() const { return kind_; }
    qint64 framesRead() const;
    void setFileVolume(float volume);

    // The best format for a capture card: 1080p (or the largest below 1440p) at 59+ Hz, uncompressed
    // (NV12, YUYV, UYVY) before MJPEG; then the largest uncompressed format at 50+ Hz; then anything.
    static int preferredFormat(const QList<QCameraFormat>& formats);

signals:
    void errorOccurred(const QString& message);
    void started();
    void stopped();

private:
    void enqueue(const QVideoFrame& frame);
    void workerLoop();

    std::shared_ptr<veyra::portable::Engine> engine_;
    Kind kind_ = Kind::None;
    QCamera* camera_ = nullptr;
    QMediaCaptureSession* session_ = nullptr;
    QMediaPlayer* player_ = nullptr;
    QAudioOutput* fileAudio_ = nullptr;
    QVideoSink* sink_ = nullptr;
    std::unique_ptr<TestPattern> pattern_;

    mutable std::mutex m_;
    std::condition_variable cv_;
    std::deque<std::pair<QVideoFrame, qint64>> queue_;
    bool stopWorker_ = false;
    qint64 read_ = 0;
    std::thread worker_;
};

}  // namespace pushfps
