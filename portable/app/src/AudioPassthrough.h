// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// Capture-card sound to the speakers. The capture card's audio shows up as a microphone; its sound is
// delayed by the same amount as the picture (frame generation holds the picture one real frame), so
// lips and footsteps stay in sync.
#include <QAudioDevice>
#include <QAudioFormat>
#include <QByteArray>
#include <QObject>
#include <QTimer>

class QAudioSource;
class QAudioSink;
class QIODevice;

namespace pushfps {

class AudioPassthrough : public QObject {
    Q_OBJECT
public:
    explicit AudioPassthrough(QObject* parent = nullptr);
    ~AudioPassthrough() override;
    bool start(const QAudioDevice& input, const QAudioDevice& output);
    void stop();
    bool running() const { return source_ != nullptr; }
    void setDelayMs(double ms);
    void setVolume(float volume);
    // The input device that belongs to a video capture device (same name, or the USB capture card's
    // usual audio name), or a null device.
    static QAudioDevice matchingInput(const QString& videoDeviceName);

signals:
    void errorOccurred(const QString& message);

private:
    void pump();

    QAudioSource* source_ = nullptr;
    QAudioSink* sink_ = nullptr;
    QIODevice* in_ = nullptr;
    QIODevice* out_ = nullptr;
    QAudioFormat format_;
    QByteArray fifo_;
    QTimer timer_;
    double delayMs_ = 0;
    float volume_ = 1.0f;
    bool primed_ = false;
};

}  // namespace pushfps
