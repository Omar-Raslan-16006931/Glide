// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "CaptureSource.h"

#include "VideoFrames.h"

#include <QAudioOutput>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QMediaPlayer>
#include <QVideoFrameFormat>
#include <QVideoSink>

#include <algorithm>

namespace pushfps {

CaptureSource::CaptureSource(std::shared_ptr<veyra::portable::Engine> engine, QObject* parent)
    : QObject(parent), engine_(std::move(engine)) {
    worker_ = std::thread([this] { workerLoop(); });
}

CaptureSource::~CaptureSource() {
    stop();
    {
        std::lock_guard lk(m_);
        stopWorker_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

qint64 CaptureSource::framesRead() const {
    std::lock_guard lk(m_);
    return read_;
}

int CaptureSource::preferredFormat(const QList<QCameraFormat>& formats) {
    auto score = [](const QCameraFormat& f) {
        const QSize r = f.resolution();
        const bool compressed = f.pixelFormat() == QVideoFrameFormat::Format_Jpeg;
        const float fps = f.maxFrameRate();
        long long s = 0;
        if (fps >= 59.0f) s += 4'000'000'000LL;
        else if (fps >= 49.0f) s += 2'000'000'000LL;
        if (!compressed) s += 1'000'000'000LL;
        // 1080p is the sweet spot; above 1440p costs a lot of processing for console pictures.
        const long long pixels = (long long)r.width() * r.height();
        s += pixels <= 2560LL * 1440 ? pixels : 2560LL * 1440 - (pixels - 2560LL * 1440) / 4;
        s += (long long)std::min(fps, 240.0f);
        return s;
    };
    int best = -1;
    long long bestScore = -1;
    for (int i = 0; i < formats.size(); ++i) {
        const long long s = score(formats[i]);
        if (s > bestScore) { bestScore = s; best = i; }
    }
    return best;
}

void CaptureSource::enqueue(const QVideoFrame& frame) {
    if (!frame.isValid()) return;
    const qint64 t = nowNs();
    {
        std::lock_guard lk(m_);
        queue_.emplace_back(frame, t);
        // Never let the capture wait: if the engine falls behind, the oldest waiting frame goes.
        while (queue_.size() > 3) queue_.pop_front();
    }
    cv_.notify_one();
}

void CaptureSource::workerLoop() {
    std::unique_lock lk(m_);
    for (;;) {
        cv_.wait(lk, [&] { return stopWorker_ || !queue_.empty(); });
        if (stopWorker_) return;
        auto item = std::move(queue_.front());
        queue_.pop_front();
        lk.unlock();
        const bool ok = pushVideoFrame(*engine_, item.first, item.second);
        lk.lock();
        if (ok) ++read_;
    }
}

bool CaptureSource::startCamera(const QCameraDevice& device, const QCameraFormat& format) {
    stop();
    if (device.isNull()) {
        emit errorOccurred(tr("未检测到采集设备"));
        return false;
    }
    engine_->reset();
    session_ = new QMediaCaptureSession(this);
    camera_ = new QCamera(device, this);
    sink_ = new QVideoSink(this);
    if (!format.isNull()) camera_->setCameraFormat(format);
    session_->setCamera(camera_);
    session_->setVideoSink(sink_);
    // Delivered on Qt's capture thread: only queue the frame there.
    connect(sink_, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame& f) { enqueue(f); }, Qt::DirectConnection);
    connect(camera_, &QCamera::errorOccurred, this, [this](QCamera::Error error, const QString& text) {
        if (error != QCamera::NoError) emit errorOccurred(text.isEmpty() ? tr("无法打开采集设备") : text);
    });
    kind_ = Kind::Camera;
    camera_->start();
    emit started();
    return true;
}

bool CaptureSource::startFile(const QUrl& url, float volume, bool withSound) {
    stop();
    if (!url.isValid()) return false;
    engine_->reset();
    player_ = new QMediaPlayer(this);
    sink_ = new QVideoSink(this);
    player_->setVideoSink(sink_);
    if (withSound) {
        fileAudio_ = new QAudioOutput(this);
        fileAudio_->setVolume(volume);
        player_->setAudioOutput(fileAudio_);
    }
    player_->setLoops(QMediaPlayer::Infinite);
    connect(sink_, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame& f) { enqueue(f); }, Qt::DirectConnection);
    connect(player_, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString& text) {
        if (error != QMediaPlayer::NoError) emit errorOccurred(text.isEmpty() ? tr("无法播放此文件") : text);
    });
    player_->setSource(url);
    kind_ = Kind::File;
    player_->play();
    emit started();
    return true;
}

void CaptureSource::setFileVolume(float volume) {
    if (fileAudio_) fileAudio_->setVolume(volume);
}

bool CaptureSource::startTestPattern(const TestPattern::Options& options) {
    stop();
    engine_->reset();
    auto engine = engine_;
    // The test picture comes ready as a QImage; it goes straight into the engine on its own thread.
    pattern_ = std::make_unique<TestPattern>(options, [this, engine](const QImage& image, qint64 t) {
        engine->push(bgraFromImage(image), t);
        std::lock_guard lk(m_);
        ++read_;
    });
    pattern_->start();
    kind_ = Kind::TestPattern;
    emit started();
    return true;
}

void CaptureSource::stop() {
    const bool wasRunning = kind_ != Kind::None;
    if (pattern_) {
        pattern_->stop();
        pattern_->wait();
        pattern_.reset();
    }
    if (camera_) camera_->stop();
    if (player_) player_->stop();
    if (sink_) disconnect(sink_, nullptr, this, nullptr);
    delete session_;
    session_ = nullptr;
    delete camera_;
    camera_ = nullptr;
    delete player_;
    player_ = nullptr;
    delete fileAudio_;
    fileAudio_ = nullptr;
    delete sink_;
    sink_ = nullptr;
    {
        std::lock_guard lk(m_);
        queue_.clear();
        read_ = 0;
    }
    kind_ = Kind::None;
    if (wasRunning) emit stopped();
}

}  // namespace pushfps
