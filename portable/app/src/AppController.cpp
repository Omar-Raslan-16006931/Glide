// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "AppController.h"

#include "FrameView.h"
#include "Settings.h"
#include "VideoFrames.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QSysInfo>
#include <QWindow>

#if QT_CONFIG(permissions)
#include <QPermissions>
#endif

#include <algorithm>
#include <cmath>

namespace pushfps {

namespace {
QString formatLabel(const QCameraFormat& f) {
    QString pixel;
    switch (f.pixelFormat()) {
    case QVideoFrameFormat::Format_NV12: pixel = QStringLiteral("NV12"); break;
    case QVideoFrameFormat::Format_YUYV: pixel = QStringLiteral("YUY2"); break;
    case QVideoFrameFormat::Format_UYVY: pixel = QStringLiteral("UYVY"); break;
    case QVideoFrameFormat::Format_YUV420P: pixel = QStringLiteral("I420"); break;
    case QVideoFrameFormat::Format_P010: pixel = QStringLiteral("P010"); break;
    case QVideoFrameFormat::Format_Jpeg: pixel = QStringLiteral("MJPEG"); break;
    default: pixel = QStringLiteral("RGB"); break;
    }
    return QStringLiteral("%1×%2 · %3 Hz · %4").arg(f.resolution().width()).arg(f.resolution().height())
        .arg(QString::number(double(f.maxFrameRate()), 'f', f.maxFrameRate() == float(int(f.maxFrameRate())) ? 0 : 2)).arg(pixel);
}
}  // namespace

AppController::AppController(QQmlEngine* qml, CatalogTranslator* translator, QString startPage, QObject* parent)
    : QObject(parent), qml_(qml), translator_(translator), startPage_(std::move(startPage)) {
    settings_ = Storage::open();
    auto& s = *settings_;
    targetMode_ = std::clamp(s.value(QStringLiteral("targetMode"), 3).toInt(), 0, 3);
    pacing_ = s.value(QStringLiteral("pacingRepair"), false).toBool();
    hud_ = s.value(QStringLiteral("hudLock"), false).toBool();
    dedup_ = s.value(QStringLiteral("removeDuplicates"), true).toBool();
    quality_ = std::clamp(s.value(QStringLiteral("quality"), 1).toInt(), 0, 1);
    audioOn_ = s.value(QStringLiteral("audioPassthrough"), true).toBool();
    volume_ = std::clamp(s.value(QStringLiteral("volume"), 1.0).toDouble(), 0.0, 1.0);
    brightness_ = std::clamp(s.value(QStringLiteral("brightness"), 0.0).toDouble(), -0.5, 0.5);
    contrast_ = std::clamp(s.value(QStringLiteral("contrast"), 1.0).toDouble(), 0.5, 1.5);
    saturation_ = std::clamp(s.value(QStringLiteral("saturation"), 1.0).toDouble(), 0.0, 2.0);
    gamma_ = std::clamp(s.value(QStringLiteral("gamma"), 1.0).toDouble(), 0.5, 2.0);
    showStats_ = s.value(QStringLiteral("showStats"), false).toBool();
    onTop_ = s.value(QStringLiteral("alwaysOnTop"), false).toBool();
    language_ = s.value(QStringLiteral("language"), QStringLiteral("auto")).toString();

    engine_ = std::make_shared<veyra::portable::Engine>();
    applyEngineSettings();
    FrameView::setEngine(engine_);
    source_ = new CaptureSource(engine_, this);
    audio_ = new AudioPassthrough(this);
    devices_ = new QMediaDevices(this);
    connect(devices_, &QMediaDevices::videoInputsChanged, this, &AppController::refreshDevices);
    connect(devices_, &QMediaDevices::audioInputsChanged, this, &AppController::refreshDevices);
    connect(devices_, &QMediaDevices::audioOutputsChanged, this, &AppController::refreshDevices);
    connect(source_, &CaptureSource::errorOccurred, this, &AppController::setError);
    connect(audio_, &AudioPassthrough::errorOccurred, this, &AppController::setError);
    connect(source_, &CaptureSource::started, this, &AppController::runningChanged);
    connect(source_, &CaptureSource::stopped, this, &AppController::runningChanged);
    statsTimer_.setInterval(500);
    connect(&statsTimer_, &QTimer::timeout, this, &AppController::updateStats);
    statsTimer_.start();

    const QString savedCamera = s.value(QStringLiteral("camera")).toString();
    cameras_ = QMediaDevices::videoInputs();
    for (int i = 0; i < cameras_.size(); ++i)
        if (QString::fromUtf8(cameras_[i].id()) == savedCamera) videoIndex_ = i;
    refreshDevices();
}

AppController::~AppController() {
    audio_->stop();
    source_->stop();
    FrameView::setEngine(nullptr);
}

void AppController::save(const QString& key, const QVariant& value) {
    settings_->setValue(key, value);
}

void AppController::setError(const QString& text) {
    if (error_ == text) return;
    error_ = text;
    emit errorTextChanged();
}

void AppController::clearError() { setError({}); }

void AppController::applyEngineSettings() {
    veyra::portable::EngineSettings s;
    s.frameGeneration = targetMode_ != 0;
    s.targetFps = targetMode_ == 1 ? 60.0 : (targetMode_ == 2 ? 120.0 : 0.0);
    s.pacingRepair = pacing_;
    s.hudLock = hud_;
    s.removeDuplicates = dedup_;
    s.quality = quality_;
    if (engine_) engine_->setSettings(s);
}

// ---- devices ------------------------------------------------------------------------------------

void AppController::refreshDevices() {
    const QByteArray current = videoIndex_ >= 0 && videoIndex_ < cameras_.size() ? cameras_[videoIndex_].id() : QByteArray();
    cameras_ = QMediaDevices::videoInputs();
    videoIndex_ = 0;
    for (int i = 0; i < cameras_.size(); ++i)
        if (cameras_[i].id() == current) videoIndex_ = i;
    inputs_ = QMediaDevices::audioInputs();
    outputs_ = QMediaDevices::audioOutputs();
    audioInIndex_ = std::clamp(audioInIndex_, 0, int(inputs_.size()));
    audioOutIndex_ = std::clamp(audioOutIndex_, 0, int(outputs_.size()));
    if (videoIndex_ < cameras_.size()) {
        const auto list = cameras_[videoIndex_].videoFormats();
        if (formatIndex_ < 0 || formatIndex_ >= list.size()) formatIndex_ = CaptureSource::preferredFormat(list);
    } else {
        formatIndex_ = -1;
    }
    emit devicesChanged();
}

QStringList AppController::videoDevices() const {
    QStringList out;
    for (const auto& c : cameras_) out << c.description();
    return out;
}

void AppController::setVideoDeviceIndex(int i) {
    if (i < 0 || i >= cameras_.size() || i == videoIndex_) return;
    videoIndex_ = i;
    formatIndex_ = CaptureSource::preferredFormat(cameras_[i].videoFormats());
    save(QStringLiteral("camera"), QString::fromUtf8(cameras_[i].id()));
    emit devicesChanged();
}

QStringList AppController::formats() const {
    QStringList out;
    if (videoIndex_ >= 0 && videoIndex_ < cameras_.size())
        for (const auto& f : cameras_[videoIndex_].videoFormats()) out << formatLabel(f);
    return out;
}

void AppController::setFormatIndex(int i) {
    if (i == formatIndex_) return;
    formatIndex_ = i;
    emit devicesChanged();
}

QStringList AppController::audioInputs() const {
    QStringList out{tr("自动")};
    for (const auto& d : inputs_) out << d.description();
    return out;
}

void AppController::setAudioInputIndex(int i) {
    if (i < 0 || i > inputs_.size() || i == audioInIndex_) return;
    audioInIndex_ = i;
    emit devicesChanged();
    if (running() && source_->kind() == CaptureSource::Kind::Camera) startAudioIfWanted();
}

QStringList AppController::audioOutputs() const {
    QStringList out{tr("跟随系统默认")};
    for (const auto& d : outputs_) out << d.description();
    return out;
}

void AppController::setAudioOutputIndex(int i) {
    if (i < 0 || i > outputs_.size() || i == audioOutIndex_) return;
    audioOutIndex_ = i;
    emit devicesChanged();
    if (running() && source_->kind() == CaptureSource::Kind::Camera) startAudioIfWanted();
}

// ---- sources ------------------------------------------------------------------------------------

bool AppController::running() const { return source_ && source_->kind() != CaptureSource::Kind::None; }

QString AppController::sourceKind() const {
    switch (source_ ? source_->kind() : CaptureSource::Kind::None) {
    case CaptureSource::Kind::Camera: return QStringLiteral("camera");
    case CaptureSource::Kind::File: return QStringLiteral("file");
    case CaptureSource::Kind::TestPattern: return QStringLiteral("test");
    default: return {};
    }
}

void AppController::startCamera() {
    clearError();
#if QT_CONFIG(permissions)
    // macOS asks the user once; a capture card counts as a camera, its sound as a microphone.
    QCameraPermission camera;
    switch (qApp->checkPermission(camera)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(camera, this, [this](const QPermission& p) {
            if (p.status() == Qt::PermissionStatus::Granted) startCameraNow();
            else setError(tr("Glide 没有使用采集卡的权限，请在系统设置的隐私与安全性中允许"));
        });
        return;
    case Qt::PermissionStatus::Denied:
        setError(tr("Glide 没有使用采集卡的权限，请在系统设置的隐私与安全性中允许"));
        return;
    case Qt::PermissionStatus::Granted:
        break;
    }
#endif
    startCameraNow();
}

void AppController::startCameraNow() {
    if (videoIndex_ < 0 || videoIndex_ >= cameras_.size()) {
        setError(tr("未检测到采集设备"));
        return;
    }
    const QCameraDevice device = cameras_[videoIndex_];
    const auto list = device.videoFormats();
    const QCameraFormat format = formatIndex_ >= 0 && formatIndex_ < list.size() ? list[formatIndex_] : QCameraFormat();
    audio_->stop();
    if (source_->startCamera(device, format)) {
        startedAt_ = nowNs();
        startAudioIfWanted();
    }
}

void AppController::startAudioIfWanted() {
    audio_->stop();
    if (!audioOn_ || source_->kind() != CaptureSource::Kind::Camera) return;
    auto begin = [this] {
        const QString name = videoIndex_ < cameras_.size() ? cameras_[videoIndex_].description() : QString();
        const QAudioDevice in = audioInIndex_ > 0 && audioInIndex_ <= inputs_.size() ? inputs_[audioInIndex_ - 1] : AudioPassthrough::matchingInput(name);
        const QAudioDevice out = audioOutIndex_ > 0 && audioOutIndex_ <= outputs_.size() ? outputs_[audioOutIndex_ - 1] : QMediaDevices::defaultAudioOutput();
        if (in.isNull()) return;  // no capture-card sound found: nothing to pass through
        audio_->setVolume(float(volume_));
        audio_->start(in, out);
    };
#if QT_CONFIG(permissions)
    QMicrophonePermission mic;
    if (qApp->checkPermission(mic) == Qt::PermissionStatus::Undetermined) {
        qApp->requestPermission(mic, this, [begin](const QPermission& p) { if (p.status() == Qt::PermissionStatus::Granted) begin(); });
        return;
    }
    if (qApp->checkPermission(mic) == Qt::PermissionStatus::Denied) return;
#endif
    begin();
}

void AppController::startFile(const QUrl& url) {
    clearError();
    audio_->stop();
    if (source_->startFile(url, float(volume_))) startedAt_ = nowNs();
}

void AppController::startTestPattern(bool uneven) {
    clearError();
    audio_->stop();
    TestPattern::Options o;
    o.uneven = uneven;
    if (source_->startTestPattern(o)) startedAt_ = nowNs();
}

void AppController::stop() {
    audio_->stop();
    source_->stop();
    engine_->reset();
    stats_.clear();
    emit statsChanged();
}

// ---- settings -----------------------------------------------------------------------------------

void AppController::setTargetMode(int m) {
    m = std::clamp(m, 0, 3);
    if (m == targetMode_) return;
    targetMode_ = m;
    save(QStringLiteral("targetMode"), m);
    applyEngineSettings();
    emit settingsChanged();
}

void AppController::setPacingRepair(bool v) {
    if (v == pacing_) return;
    pacing_ = v;
    // Frame Pacing Repair re-times what duplicate removal finds.
    if (v && !dedup_) { dedup_ = true; save(QStringLiteral("removeDuplicates"), true); }
    save(QStringLiteral("pacingRepair"), v);
    applyEngineSettings();
    emit settingsChanged();
}

void AppController::setHudLock(bool v) {
    if (v == hud_) return;
    hud_ = v;
    save(QStringLiteral("hudLock"), v);
    applyEngineSettings();
    emit settingsChanged();
}

void AppController::setRemoveDuplicates(bool v) {
    if (v == dedup_) return;
    dedup_ = v;
    if (!v && pacing_) { pacing_ = false; save(QStringLiteral("pacingRepair"), false); }
    save(QStringLiteral("removeDuplicates"), v);
    applyEngineSettings();
    emit settingsChanged();
}

void AppController::setQuality(int q) {
    q = std::clamp(q, 0, 1);
    if (q == quality_) return;
    quality_ = q;
    save(QStringLiteral("quality"), q);
    applyEngineSettings();
    emit settingsChanged();
}

void AppController::setAudioPassthrough(bool v) {
    if (v == audioOn_) return;
    audioOn_ = v;
    save(QStringLiteral("audioPassthrough"), v);
    emit settingsChanged();
    if (v) startAudioIfWanted();
    else audio_->stop();
}

void AppController::setVolume(qreal v) {
    v = std::clamp(v, 0.0, 1.0);
    if (std::abs(v - volume_) < 1e-6) return;
    volume_ = v;
    audio_->setVolume(float(v));
    source_->setFileVolume(float(v));
    save(QStringLiteral("volume"), v);
    emit settingsChanged();
}

void AppController::setBrightness(qreal v) { v = std::clamp(v, -0.5, 0.5); if (v != brightness_) { brightness_ = v; save(QStringLiteral("brightness"), v); emit settingsChanged(); } }
void AppController::setContrast(qreal v) { v = std::clamp(v, 0.5, 1.5); if (v != contrast_) { contrast_ = v; save(QStringLiteral("contrast"), v); emit settingsChanged(); } }
void AppController::setSaturation(qreal v) { v = std::clamp(v, 0.0, 2.0); if (v != saturation_) { saturation_ = v; save(QStringLiteral("saturation"), v); emit settingsChanged(); } }
void AppController::setGamma(qreal v) { v = std::clamp(v, 0.5, 2.0); if (v != gamma_) { gamma_ = v; save(QStringLiteral("gamma"), v); emit settingsChanged(); } }

void AppController::resetPicture() {
    setBrightness(0);
    setContrast(1);
    setSaturation(1);
    setGamma(1);
}

void AppController::setShowStats(bool v) {
    if (v == showStats_) return;
    showStats_ = v;
    save(QStringLiteral("showStats"), v);
    emit settingsChanged();
}

void AppController::setAlwaysOnTop(bool v) {
    if (v == onTop_) return;
    onTop_ = v;
    save(QStringLiteral("alwaysOnTop"), v);
    emit settingsChanged();
}

void AppController::setLanguage(const QString& code) {
    if (code == language_) return;
    language_ = code;
    save(QStringLiteral("language"), code);
    translator_->load(code);
    if (qml_) qml_->retranslate();
    emit settingsChanged();
    emit devicesChanged();  // "Auto" / "Follow the system default" entries
}

QStringList AppController::languageCodes() const {
    QStringList out{QStringLiteral("auto")};
    out << CatalogTranslator::codes();
    return out;
}

QStringList AppController::languageNames() const {
    // Each language in its own name; the first entry is translated.
    return {tr("跟随系统"), QStringLiteral("简体中文"), QStringLiteral("繁體中文"), QStringLiteral("English"),
            QStringLiteral("日本語"), QStringLiteral("Nederlands"), QStringLiteral("Deutsch"), QStringLiteral("Español"),
            QStringLiteral("Français"), QStringLiteral("Italiano"), QStringLiteral("Português (Brasil)"), QStringLiteral("Türkçe"),
            QStringLiteral("العربية"), QStringLiteral("Polski"), QStringLiteral("Русский")};
}

bool AppController::portable() const { return Storage::portable(); }

QString AppController::version() const { return QStringLiteral(PUSHFPS_PORTABLE_VERSION); }

QString AppController::platformName() const {
    return QSysInfo::prettyProductName() + QStringLiteral(" · ") + QSysInfo::currentCpuArchitecture();
}

QString AppController::newScreenshotPath() const {
    return Storage::screenshotFolder() + QStringLiteral("/Glide-") +
           QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")) + QStringLiteral(".png");
}

void AppController::openScreenshotFolder() const {
    QDesktopServices::openUrl(QUrl::fromLocalFile(Storage::screenshotFolder()));
}

// ---- statistics ---------------------------------------------------------------------------------

void AppController::updateStats() {
    if (!running()) return;
    const auto st = engine_->stats();
    QVariantMap m;
    m[QStringLiteral("transportFps")] = st.transportFps;
    m[QStringLiteral("contentFps")] = st.contentFps;
    m[QStringLiteral("outputFps")] = st.outputFps;
    m[QStringLiteral("latencyMs")] = st.latencyMs;
    m[QStringLiteral("estimateMs")] = st.estimateMs;
    m[QStringLiteral("delayMs")] = st.delayMs;
    m[QStringLiteral("duplicates")] = qulonglong(st.duplicates);
    m[QStringLiteral("sceneCuts")] = qulonglong(st.sceneCuts);
    m[QStringLiteral("lateFields")] = qulonglong(st.lateFields);
    m[QStringLiteral("pacingShare")] = st.kept > 0 ? double(st.pacingRepaired) / double(st.kept) : 0.0;
    m[QStringLiteral("width")] = st.width;
    m[QStringLiteral("height")] = st.height;
    m[QStringLiteral("framesRead")] = source_->framesRead();
    stats_ = m;
    emit statsChanged();
    // Sound waits as long as the picture does.
    audio_->setDelayMs(targetMode_ != 0 ? st.delayMs : 0.0);
    // Nothing arrives: say so instead of a black window.
    if (source_->framesRead() == 0 && startedAt_ > 0 && nowNs() - startedAt_ > 5'000'000'000LL && error_.isEmpty())
        setError(tr("没有收到画面：检查采集卡的连接、主机是否开机，以及采集卡是否被其他程序占用"));
}

}  // namespace pushfps
