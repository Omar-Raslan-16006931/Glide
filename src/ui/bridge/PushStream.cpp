// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// QML bridge: Glide going live (engine/LiveStream.h, LiveRecorder in stream mode).
//
// The outputs live in the pref streamTargets ([{id, platform, name, server, enabled}], validated in
// Preferences.cpp). Their stream keys are kept apart, encrypted for this Windows user with DPAPI
// (CryptProtectData), under the pref streamKeys {id: base64}; the interface can set, test for and
// forget a key but never read it back, and keys are never logged.
#include "../QmlPlayerBridge_p.h"
#include "veyra/engine/LiveStream.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <windows.h>
#include <wincrypt.h>

namespace veyra::ui {
namespace {

QByteArray protect(const QByteArray& plain) {
    DATA_BLOB in{DWORD(plain.size()), reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()))};
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"Glide stream key", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return {};
    QByteArray result(reinterpret_cast<const char*>(out.pbData), int(out.cbData));
    LocalFree(out.pbData);
    return result;
}

QByteArray unprotect(const QByteArray& blob) {
    DATA_BLOB in{DWORD(blob.size()), reinterpret_cast<BYTE*>(const_cast<char*>(blob.data()))};
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return {};
    QByteArray result(reinterpret_cast<const char*>(out.pbData), int(out.cbData));
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return result;
}

QString stateText(engine::stream::State s) {
    using engine::stream::State;
    switch (s) {
    case State::Connecting: return QStringLiteral("connecting");
    case State::Live: return QStringLiteral("live");
    case State::Reconnecting: return QStringLiteral("reconnecting");
    case State::Failed: return QStringLiteral("failed");
    case State::Stopped: return QStringLiteral("stopped");
    default: return QStringLiteral("idle");
    }
}

}  // namespace

QVariantList QmlPlayerBridge::pushStreamPlatforms() const {
    QVariantList list;
    for (const auto& p : engine::stream::platforms()) {
        QVariantList servers;
        for (const auto& s : p.servers) servers << QVariantMap{{"name", QString::fromUtf8(s.name)}, {"url", QString::fromUtf8(s.url)}};
        list << QVariantMap{{"id", QString::fromUtf8(p.id)}, {"name", QString::fromUtf8(p.name)}, {"servers", servers},
                            {"maxVideoKbps", int(p.maxVideoKbps)}, {"maxAudioKbps", int(p.maxAudioKbps)},
                            {"needsRtmps", p.needsRtmps}, {"keyPage", QString::fromUtf8(p.keyPage)}};
    }
    return list;
}

bool QmlPlayerBridge::pushSetStreamKey(const QString& id, const QString& key) {
    const QString trimmed = key.trimmed();
    if (id.isEmpty() || id.size() > 40) return false;
    if (trimmed.isEmpty() || trimmed.size() > 512 || trimmed.contains(QLatin1Char(' '))) {
        emit notice(tr("串流密钥无效"), true);
        return false;
    }
    const QByteArray blob = protect(trimmed.toUtf8());
    if (blob.isEmpty()) { emit notice(tr("无法安全保存串流密钥"), true); return false; }
    QVariantMap keys = impl_->prefs.value(QStringLiteral("streamKeys")).toMap();
    keys[id] = QString::fromLatin1(blob.toBase64());
    const auto previous = impl_->prefs;
    impl_->prefs[QStringLiteral("streamKeys")] = keys;
    if (!impl_->savePrefs()) { impl_->prefs = previous; emit notice(tr("偏好保存失败，已保留原设置"), true); return false; }
    veyra::log::info("ui-stream", std::format("stream key saved for output {} (encrypted)", id.toStdString()));
    emit preferencesChanged();
    return true;
}

bool QmlPlayerBridge::pushHasStreamKey(const QString& id) const {
    return impl_->prefs.value(QStringLiteral("streamKeys")).toMap().contains(id);
}

void QmlPlayerBridge::pushForgetStreamKey(const QString& id) {
    QVariantMap keys = impl_->prefs.value(QStringLiteral("streamKeys")).toMap();
    if (!keys.remove(id)) return;
    impl_->prefs[QStringLiteral("streamKeys")] = keys;
    impl_->savePrefs();
    emit preferencesChanged();
}

bool QmlPlayerBridge::pushStartStream() {
    if (!hasSource()) { emit notice(tr("没有正在播放的画面，无法开播"), true); return false; }
    const QJsonArray targets = QJsonDocument::fromJson(impl_->prefString("streamTargets").toUtf8()).array();
    const QVariantMap keys = impl_->prefs.value(QStringLiteral("streamKeys")).toMap();
    engine::RecordingSettings options;
    unsigned capKbps = 50000, audioCap = 320;
    QStringList names;
    for (const auto item : targets) {
        const QJsonObject o = item.toObject();
        if (!o.value(QStringLiteral("enabled")).toBool(true)) continue;
        const QString id = o.value(QStringLiteral("id")).toString();
        const auto* platform = engine::stream::platform(o.value(QStringLiteral("platform")).toString().toStdString());
        if (!platform) continue;
        QString name = o.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) name = QString::fromUtf8(platform->name);
        QString server = o.value(QStringLiteral("server")).toString().trimmed();
        if (server.isEmpty() && !platform->servers.empty()) server = QString::fromUtf8(platform->servers.front().url);
        if (server.isEmpty()) { emit notice(tr("%1：请填写平台给你的服务器地址").arg(name), true); return false; }
        if (platform->needsRtmps && !server.startsWith(QStringLiteral("rtmps://"), Qt::CaseInsensitive)) {
            emit notice(tr("%1 只接受 rtmps:// 开头的服务器地址").arg(name), true);
            return false;
        }
        if (!keys.contains(id)) { emit notice(tr("%1 还没有串流密钥").arg(name), true); return false; }
        const QByteArray key = unprotect(QByteArray::fromBase64(keys.value(id).toString().toLatin1()));
        if (key.isEmpty()) { emit notice(tr("%1 的串流密钥无法读取，请重新输入").arg(name), true); return false; }
        const std::string url = engine::stream::joinUrl(server.toStdString(), key.toStdString());
        if (url.empty()) { emit notice(tr("%1 的服务器地址或串流密钥无效").arg(name), true); return false; }
        options.streams.push_back({name.toStdString(), url});
        capKbps = std::min(capKbps, platform->maxVideoKbps);
        audioCap = std::min(audioCap, platform->maxAudioKbps);
        names << name;
    }
    if (options.streams.empty()) { emit notice(tr("请先添加并开启至少一个直播平台"), true); return false; }
    // One encode feeds every output, so it follows the strictest platform.
    int mbps = std::clamp(impl_->prefInt("streamBitrateMbps", 6), 1, 50);
    const int capMbps = std::max(1, int(capKbps / 1000));
    if (mbps > capMbps) {
        emit notice(tr("码率已按平台上限降到 %1 Mbps").arg(capMbps), false);
        mbps = capMbps;
    }
    options.bitrateMbps = uint32_t(mbps);
    options.fpsLimit = uint32_t(impl_->prefInt("streamFps", 60) >= 60 ? 60 : 30);
    options.audio = impl_->prefBool("streamAudio", true);
    options.microphone = impl_->prefBool("streamMic", false);
    options.microphoneId = recordingMicrophoneId().toStdWString();
    options.microphoneTrack = false;
    options.audioKbps = std::min(160u, audioCap);
    options.preset = 4;
    std::wstring error;
    if (!impl_->engine.requestStreaming(options, error)) { emit notice(uiText(error), true); return false; }
    veyra::log::info("ui-stream", std::format("go live outputs={} [{}] bitrateMbps={} fps={} audio={} microphone={}",
                                              options.streams.size(), names.join(QStringLiteral(", ")).toStdString(), mbps,
                                              options.fpsLimit, options.audio, options.microphone));
    return true;
}

void QmlPlayerBridge::pushStopStream() {
    impl_->engine.stopStreaming();
    veyra::log::info("ui-stream", "stop requested");
}

QVariantMap QmlPlayerBridge::pushStreamStatus() const {
    const auto s = impl_->engine.streamingSnapshot();
    QVariantMap m;
    m["active"] = s.active;
    m["failed"] = s.failed;
    m["error"] = s.error.empty() ? QString() : uiText(s.error);
    m["encoder"] = QString::fromStdWString(s.encoder);
    m["seconds"] = s.seconds;
    m["frames"] = QVariant::fromValue(qulonglong(s.frames));
    m["dropped"] = QVariant::fromValue(qulonglong(s.dropped));
    m["audio"] = s.audio;
    QVariantList outputs;
    bool anyLive = false;
    for (const auto& t : s.streams) {
        anyLive = anyLive || t.state == engine::stream::State::Live;
        outputs << QVariantMap{{"name", QString::fromStdString(t.name)}, {"server", QString::fromStdString(t.server)},
                               {"state", stateText(t.state)}, {"error", QString::fromStdString(t.error)},
                               {"kbps", t.kbps}, {"megabytes", double(t.bytesSent) / 1e6},
                               {"framesSent", QVariant::fromValue(qulonglong(t.framesSent))},
                               {"framesDropped", QVariant::fromValue(qulonglong(t.framesDropped))},
                               {"reconnects", int(t.reconnects)}, {"liveSeconds", t.liveSeconds}};
    }
    m["outputs"] = outputs;
    m["live"] = anyLive;
    return m;
}

}  // namespace veyra::ui
