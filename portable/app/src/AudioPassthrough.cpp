// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "AudioPassthrough.h"

#include <QAudioSink>
#include <QAudioSource>
#include <QMediaDevices>

#include <algorithm>
#include <cmath>

namespace pushfps {

AudioPassthrough::AudioPassthrough(QObject* parent) : QObject(parent) {
    timer_.setInterval(5);
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, &AudioPassthrough::pump);
}

AudioPassthrough::~AudioPassthrough() { stop(); }

QAudioDevice AudioPassthrough::matchingInput(const QString& videoDeviceName) {
    const auto inputs = QMediaDevices::audioInputs();
    const QString name = videoDeviceName.trimmed().toLower();
    if (!name.isEmpty()) {
        for (const auto& d : inputs)
            if (d.description().toLower().contains(name) || name.contains(d.description().toLower())) return d;
    }
    // Typical names of capture-card audio (Elgato, AVerMedia, generic UVC "USB3 Video").
    static const char* hints[] = {"capture", "hdmi", "elgato", "avermedia", "usb video", "usb3 video", "cam link", "game", "live gamer"};
    for (const auto& d : inputs)
        for (const char* h : hints)
            if (d.description().toLower().contains(QLatin1String(h))) return d;
    return {};
}

bool AudioPassthrough::start(const QAudioDevice& input, const QAudioDevice& output) {
    stop();
    if (input.isNull() || output.isNull()) return false;
    QAudioFormat f;
    f.setSampleRate(48000);
    f.setChannelCount(2);
    f.setSampleFormat(QAudioFormat::Int16);
    if (!input.isFormatSupported(f)) f = input.preferredFormat();
    if (!output.isFormatSupported(f)) {
        emit errorOccurred(tr("音频格式不受支持"));
        return false;
    }
    format_ = f;
    source_ = new QAudioSource(input, f, this);
    sink_ = new QAudioSink(output, f, this);
    // Small device buffers: the delay we want is the picture's, not the sound system's.
    source_->setBufferSize(f.bytesForDuration(20000));
    sink_->setBufferSize(f.bytesForDuration(40000));
    sink_->setVolume(volume_);
    in_ = source_->start();
    out_ = sink_->start();
    if (!in_ || !out_) {
        stop();
        emit errorOccurred(tr("无法打开音频设备"));
        return false;
    }
    fifo_.clear();
    primed_ = false;
    timer_.start();
    return true;
}

void AudioPassthrough::stop() {
    timer_.stop();
    if (source_) source_->stop();
    if (sink_) sink_->stop();
    delete source_;
    delete sink_;
    source_ = nullptr;
    sink_ = nullptr;
    in_ = out_ = nullptr;
    fifo_.clear();
}

void AudioPassthrough::setDelayMs(double ms) { delayMs_ = std::clamp(ms, 0.0, 250.0); }

void AudioPassthrough::setVolume(float volume) {
    volume_ = std::clamp(volume, 0.0f, 1.0f);
    if (sink_) sink_->setVolume(volume_);
}

void AudioPassthrough::pump() {
    if (!in_ || !out_) return;
    fifo_.append(in_->readAll());
    const int frame = std::max(1, format_.bytesPerFrame());
    const qsizetype want = format_.bytesForDuration(qint64(delayMs_ * 1000.0)) / frame * frame;
    // Too much waiting (the sound device stalled): drop the oldest so the delay comes back.
    const qsizetype tooMuch = want + format_.bytesForDuration(80000) / frame * frame;
    if (fifo_.size() > tooMuch) fifo_.remove(0, (fifo_.size() - want) / frame * frame);
    if (!primed_) {
        if (fifo_.size() < want) return;
        primed_ = true;
    }
    const qsizetype free = sink_->bytesFree() / frame * frame;
    const qsizetype n = std::min(free, (fifo_.size() / frame) * frame);
    if (n <= 0) return;
    const qint64 written = out_->write(fifo_.constData(), n);
    if (written > 0) fifo_.remove(0, written);
    if (fifo_.isEmpty()) primed_ = false;  // ran dry: wait for the delay again instead of crackling
}

}  // namespace pushfps
