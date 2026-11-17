// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// QML bridge: the Glide streaming mode (qml/Veyra/StudioPage.qml) audio mixer.
//
// The mix itself (game fader and mute, microphone filters, fader and mute) is kept in preferences
// (mixGameDb, mixGameMuted, mixMicDb, mixMicMuted, micHighPass, micEqOn, micEq, micGate, micGateDb,
// micCompressor; validated in Preferences.cpp) and applied process-wide by applyPushMix
// (engine/PushAudioMix.h). While the streaming mode is on screen, pushSetStudioMeters keeps the
// game sound tap and (when the microphone is used for recordings or streams) the microphone open
// so their meters move before anything records, as in OBS; the page turns them off when it leaves.
#include "../QmlPlayerBridge_p.h"
#include "veyra/engine/MicrophoneCapture.h"
#include "veyra/engine/PushAudioMix.h"
#include "veyra/engine/PushStreamOverlay.h"
#include "veyra/Brand.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QPainter>
#include <QPainterPath>
#include "veyra/sink/RecordingAudioTap.h"

#include <cmath>
#include <cstring>
#include <format>
#include <memory>

namespace veyra::ui {
namespace {

// The game sound for the meter: what the recorder would take, after the game fader.
struct GameMeterTap final : sink::RecordingAudioSink {
    void renderedPcm(const float* pcm, size_t frames, const sink::AudioFormat& format, int64_t, const void*) override {
        if (format.valid()) engine::pushGameMeter().feed(pcm, frames, format.channels, engine::pushGameGain());
    }
};
// Keeps the microphone open for its meter; the capture thread meters every packet itself.
struct MicMeterSink final : engine::MicrophoneSink {
    void microphonePcm(const float*, size_t, int64_t) override {}
};
std::shared_ptr<GameMeterTap> gameTap;
std::shared_ptr<MicMeterSink> micTap;

double level(float db) { return std::isfinite(db) ? double(db) : -100.0; }

QFont sceneFont(int pixels, int weight, bool italic = false) {
    QFont font(QStringLiteral("Bahnschrift"));
    font.setStyleHint(QFont::SansSerif);
    font.setPixelSize(std::max(8, pixels));
    font.setWeight(QFont::Weight(weight));
    font.setItalic(italic);
    return font;
}
// Straight-alpha RGBA bytes of a painted picture (RecordingLabel layout).
engine::PushOverlayImage imageOf(const QImage& painted) {
    const QImage image = painted.convertToFormat(QImage::Format_RGBA8888);
    engine::PushOverlayImage out;
    out.width = uint32_t(image.width()); out.height = uint32_t(image.height());
    out.rgba.resize(size_t(out.width) * out.height * 4);
    for (int y = 0; y < image.height(); ++y)
        std::memcpy(&out.rgba[size_t(y) * out.width * 4], image.constScanLine(y), size_t(out.width) * 4);
    return out;
}
// The Glide wordmark: Push in white, FPS in logo green.
void drawWordmark(QPainter& painter, const QPointF& rightBaseline, int pixels) {
    const QFont font = sceneFont(pixels, QFont::Bold, true);
    const QFontMetricsF m(font);
    const QString push = QStringLiteral("Push"), fps = QStringLiteral("FPS");
    const qreal width = m.horizontalAdvance(push) + m.horizontalAdvance(fps);
    painter.setFont(font);
    painter.setPen(QColor(255, 255, 255, 230));
    painter.drawText(QPointF(rightBaseline.x() - width, rightBaseline.y()), push);
    painter.setPen(QColor(141, 242, 55));
    painter.drawText(QPointF(rightBaseline.x() - m.horizontalAdvance(fps), rightBaseline.y()), fps);
}
// Draws `image` to cover `area` (cropped to keep its shape).
void drawCover(QPainter& painter, const QImage& image, const QRectF& area) {
    if (image.isNull()) return;
    const qreal scale = std::max(area.width() / image.width(), area.height() / image.height());
    const QSizeF size(image.width() * scale, image.height() * scale);
    painter.drawImage(QRectF(area.center() - QPointF(size.width() / 2, size.height() / 2), size), image);
}

} // namespace

QString QmlPlayerBridge::pushPickImage() {
    return QFileDialog::getOpenFileName(nullptr, tr("选择图片"), QString(), tr("图片 (*.png *.jpg *.jpeg *.webp *.bmp)"));
}

bool QmlPlayerBridge::pushSetStreamScene(const QVariantMap& scene) {
    const bool card = scene.value("type").toString() == QLatin1String("card");
    const QString overlayPath = scene.value("overlayImage").toString(), overlayText = scene.value("overlayText").toString().left(80);
    if (!card && overlayPath.isEmpty() && overlayText.trimmed().isEmpty()) {
        engine::setPushStreamOverlay(nullptr);
        veyra::log::info("push-studio", "scene game (no overlay)");
        return true;
    }
    // The size recordings and streams are made at: the presented picture.
    const auto out = impl_->snapshot.metrics.resolution.output;
    const int W = out.width >= 64 && out.height >= 64 ? int(out.width) & ~1 : 1920;
    const int H = out.width >= 64 && out.height >= 64 ? int(out.height) & ~1 : 1080;
    QImage canvas(W, H, QImage::Format_ARGB32);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (card) {
        // Background: the chosen picture, or the Glide graphite with a green glow.
        const QImage background(scene.value("image").toString());
        if (!background.isNull()) {
            drawCover(painter, background, QRectF(0, 0, W, H));
            QLinearGradient shade(0, 0, 0, H);
            shade.setColorAt(0.0, QColor(10, 12, 15, 90));
            shade.setColorAt(1.0, QColor(10, 12, 15, 200));
            painter.fillRect(QRectF(0, 0, W, H), shade);
        } else {
            QLinearGradient base(0, 0, W, H);
            base.setColorAt(0.0, QColor(22, 26, 32));
            base.setColorAt(1.0, QColor(10, 12, 15));
            painter.fillRect(QRectF(0, 0, W, H), base);
            QRadialGradient glow(QPointF(W * 0.5, H * 0.62), H * 0.8);
            glow.setColorAt(0.0, QColor(141, 242, 55, 46));
            glow.setColorAt(1.0, QColor(141, 242, 55, 0));
            painter.fillRect(QRectF(0, 0, W, H), glow);
        }
        const QString title = scene.value("title").toString().left(60), subtitle = scene.value("subtitle").toString().left(120);
        QFont titleFont = sceneFont(H / 9, QFont::Bold, true);
        QFontMetricsF tm(titleFont);
        while (tm.horizontalAdvance(title) > W * 0.88 && titleFont.pixelSize() > 16) { titleFont.setPixelSize(titleFont.pixelSize() - 4); tm = QFontMetricsF(titleFont); }
        painter.setFont(titleFont);
        painter.setPen(QColor(255, 255, 255));
        const qreal titleY = H * 0.5 + (subtitle.isEmpty() ? tm.ascent() / 3 : 0);
        painter.drawText(QPointF((W - tm.horizontalAdvance(title)) / 2, titleY), title);
        // A short green rule under the title.
        painter.fillRect(QRectF(W / 2.0 - H * 0.06, titleY + tm.descent() + H * 0.02, H * 0.12, std::max(3, H / 240)), QColor(141, 242, 55));
        if (!subtitle.isEmpty()) {
            const QFont subFont = sceneFont(H / 24, QFont::Medium);
            const QFontMetricsF sm(subFont);
            painter.setFont(subFont);
            painter.setPen(QColor(205, 210, 216));
            painter.drawText(QRectF(W * 0.08, titleY + tm.descent() + H * 0.06, W * 0.84, sm.height() * 2.4), Qt::AlignHCenter | Qt::TextWordWrap, subtitle);
        }
        drawWordmark(painter, QPointF(W - H * 0.04, H - H * 0.045), H / 26);
    } else {
        // Game scene: an image and/or a text tag in one corner.
        const QString corner = scene.value("corner").toString();
        const bool right = corner == QLatin1String("tr") || corner == QLatin1String("br");
        const bool bottom = corner == QLatin1String("bl") || corner == QLatin1String("br");
        const qreal margin = H / 30.0;
        qreal x = right ? W - margin : margin, y = bottom ? H - margin : margin;
        const QImage logo(overlayPath);
        if (!logo.isNull()) {
            const qreal scale = std::min(H / 7.0 / logo.height(), W / 4.0 / logo.width());
            const QSizeF size(logo.width() * scale, logo.height() * scale);
            const QRectF rect(right ? x - size.width() : x, bottom ? y - size.height() : y, size.width(), size.height());
            painter.drawImage(rect, logo);
            y = bottom ? rect.top() - margin * 0.4 : rect.bottom() + margin * 0.4;
        }
        if (!overlayText.trimmed().isEmpty()) {
            const QFont font = sceneFont(H / 28, QFont::Bold);
            const QFontMetricsF m(font);
            const qreal pad = m.height() * 0.45, w = m.horizontalAdvance(overlayText) + 2 * pad, h = m.height() + pad;
            const QRectF pill(right ? x - w : x, bottom ? y - h : y, w, h);
            QPainterPath path;
            path.addRoundedRect(pill, h / 2, h / 2);
            painter.fillPath(path, QColor(14, 15, 18, 215));
            painter.setPen(QPen(QColor(141, 242, 55), std::max(1.5, H / 540.0)));
            painter.drawPath(path);
            painter.setFont(font);
            painter.setPen(QColor(255, 255, 255));
            painter.drawText(pill, Qt::AlignCenter, overlayText);
        }
    }
    painter.end();
    engine::setPushStreamOverlay(std::make_shared<engine::PushOverlayImage>(imageOf(canvas)));
    veyra::log::info("push-studio", std::format("scene {} {}x{}", card ? "card" : "game overlay", W, H));
    return true;
}

void QmlPlayerBridge::applyPushMix() {
    engine::PushMixSettings s;
    s.gameDb = float(impl_->prefInt("mixGameDb", 0));
    s.gameMuted = impl_->prefBool("mixGameMuted", false);
    s.micDb = float(impl_->prefInt("mixMicDb", 0));
    s.micMuted = impl_->prefBool("mixMicMuted", false);
    s.micHighPass = impl_->prefBool("micHighPass", false);
    s.micEq = impl_->prefBool("micEqOn", false);
    const QStringList parts = impl_->prefString("micEq").split(QLatin1Char(','));
    if (parts.size() == engine::kMicEqBands)
        for (int i = 0; i < engine::kMicEqBands; ++i) s.micEqDb[std::size_t(i)] = parts[i].toFloat();
    s.micGate = impl_->prefBool("micGate", false);
    s.micGateDb = float(impl_->prefInt("micGateDb", -45));
    s.micCompressor = impl_->prefBool("micCompressor", false);
    engine::setPushMix(s);
}

void QmlPlayerBridge::pushSetStudioMeters(bool on) {
    const bool wantMic = on && (impl_->prefBool("recordMic", false) || impl_->prefBool("streamMic", false));
    if (on && !gameTap) {
        gameTap = std::make_shared<GameMeterTap>();
        sink::addRecordingAudioSink(gameTap);
    } else if (!on && gameTap) {
        sink::removeRecordingAudioSink(gameTap.get());
        gameTap.reset();
    }
    if (wantMic && !micTap) {
        micTap = std::make_shared<MicMeterSink>();
        engine::attachMicrophone(micTap, impl_->prefString("recordMicDevice").toStdWString());
    } else if (!wantMic && micTap) {
        engine::detachMicrophone(micTap.get());
        micTap.reset();
    }
    veyra::log::info("push-studio", std::format("meters {} microphone={}", on ? "on" : "off", wantMic));
}

QVariantMap QmlPlayerBridge::pushStudioLevels() {
    const auto game = engine::pushGameMeter().take();
    const auto mic = engine::pushMicMeter().take();
    const auto status = engine::microphoneStatus();
    QVariantMap m;
    m["gamePeakDb"] = level(game.peakDb);
    m["gameRmsDb"] = level(game.rmsDb);
    m["micPeakDb"] = level(mic.peakDb);
    m["micRmsDb"] = level(mic.rmsDb);
    m["micOpen"] = status.open;
    m["micDevice"] = QString::fromStdWString(status.device);
    m["micError"] = status.error.empty() ? QString() : uiText(status.error);
    return m;
}

} // namespace veyra::ui
