// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// Glide going live: the encoded picture and sound go to one or more RTMP / RTMPS servers at once
// (Twitch, YouTube, Kick, TikTok, Facebook, any custom server), each on its own connection and thread.
//
// The packets come from the recorder's encoder (LiveRecorder in stream mode), so streaming to two
// platforms costs one encode. Each output:
//   - connects with FFmpeg's FLV muxer over rtmp:// or rtmps:// (TLS by the system: Schannel on Windows);
//   - starts at a keyframe, timestamps from zero;
//   - never blocks the encoder: a slow connection drops whole groups of pictures (up to the next
//     keyframe) instead of falling behind, and counts them;
//   - reconnects after a dropped connection (2, 4, 8 ... 30 s, up to kMaxReconnects times in a row).
// Stream keys are never written to the log: URLs are logged with the key replaced.
//
// Server lists and recommended settings follow OBS Studio's rtmp-services (GPL-2.0-or-later):
// keyframe every 2 s, H.264 + AAC, the platform's maximum bitrates.
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct AVCodecParameters;

namespace veyra::engine::stream {

// ---- URL helpers (pure, unit-tested) ------------------------------------------------------------

// rtmp:// or rtmps:// with a host, no spaces or control characters.
bool validServer(std::string_view server);
// Server + "/" + key (one slash between them). Empty if the server is invalid or the key is empty
// or contains spaces / control characters.
std::string joinUrl(std::string_view server, std::string_view key);
// The URL with everything after the application path replaced by "****" (for logs and the interface).
std::string redact(std::string_view url);

struct Server {
    const char* name;
    const char* url;
};
struct Platform {
    const char* id;               // "twitch", "youtube", "kick", "tiktok", "facebook", "trovo", "custom"
    const char* name;
    std::vector<Server> servers;  // ingest servers, the default first; empty: the user pastes the platform's server URL
    unsigned maxVideoKbps;        // the platform's recommended maximum
    unsigned maxAudioKbps;
    bool needsRtmps;              // the platform only accepts rtmps://
    const char* keyPage;          // where the user finds the stream key (shown as text)
};
const std::vector<Platform>& platforms();
const Platform* platform(std::string_view id);

// ---- outputs ------------------------------------------------------------------------------------

struct Target {
    std::string name;   // shown in the interface ("Twitch")
    std::string url;    // full URL including the stream key (never logged)
};

enum class State { Idle, Connecting, Live, Reconnecting, Failed, Stopped };

struct TargetStatus {
    std::string name;
    std::string server;        // redacted URL
    State state = State::Idle;
    std::string error;         // last error (English technical text from FFmpeg, or ours)
    uint64_t bytesSent = 0;
    double kbps = 0;           // over the last few seconds
    uint64_t framesSent = 0;
    uint64_t framesDropped = 0;
    unsigned reconnects = 0;
    double liveSeconds = 0;    // since the current connection started
};

struct VideoFormat {
    int width = 0, height = 0;
    int fps = 60;
    bool hevc = false;                    // FLV here carries H.264 only: HEVC is refused
    std::vector<uint8_t> extradata;       // SPS/PPS (Annex B or avcC)
    int64_t bitRate = 0;                  // bits per second (metadata only)
};

class Outputs {
public:
    static constexpr unsigned kMaxReconnects = 20;
    using Logger = std::function<void(const std::string&)>;
    explicit Outputs(Logger log = {});
    ~Outputs();
    Outputs(const Outputs&) = delete;
    Outputs& operator=(const Outputs&) = delete;

    // Starts one thread per target. audio may be null (no sound). Returns false (with a reason) if the
    // formats or targets are unusable; per-target connection errors show in status().
    bool start(const std::vector<Target>& targets, const VideoFormat& video, const AVCodecParameters* audio, std::string& error);
    // Encoder thread: timestamps in microseconds on one clock (the recorder's), increasing per stream.
    void pushVideo(std::shared_ptr<const std::vector<uint8_t>> data, int64_t ptsUs, int64_t durationUs, bool key);
    void pushAudio(std::shared_ptr<const std::vector<uint8_t>> data, int64_t ptsUs, int64_t durationUs);
    // Ends every connection (sends what is queued for up to a second, then closes).
    void stop();
    std::vector<TargetStatus> status() const;
    bool running() const;

private:
    struct Connection;
    std::vector<std::unique_ptr<Connection>> connections_;
    Logger log_;
};

const char* stateName(State s);

}  // namespace veyra::engine::stream
