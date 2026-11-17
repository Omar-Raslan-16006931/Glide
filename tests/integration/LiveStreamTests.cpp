// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Going live (engine/LiveStream.h), without the internet:
//   - URL helpers: joining, validation, key redaction, the platform table;
//   - with a local MediaMTX server (PUSHFPS_STREAM_SERVER = path of the mediamtx program, its config
//     in PUSHFPS_STREAM_CONFIG) and the fixture tests/data/stream-test.flv (H.264 + AAC):
//       * two outputs at once, one over RTMP and one over RTMPS, each read back by an FFmpeg client
//         that must find H.264 video and AAC sound;
//       * a third output to a closed port keeps retrying without disturbing the other two;
//       * the server goes away: the outputs say "reconnecting"; it comes back: live again;
//       * stop() returns quickly even with an output that never connected.
#include "veyra/engine/LiveStream.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

using namespace veyra::engine::stream;
using namespace std::chrono_literals;

static int failures = 0;
static void check(bool ok, const std::string& label) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label.c_str());
    std::fflush(stdout);
    if (!ok) ++failures;
}

static void testUrls() {
    check(validServer("rtmp://live.example.com/app"), "rtmp server");
    check(validServer("rtmps://a.rtmps.youtube.com:443/live2"), "rtmps server with port");
    check(!validServer("http://example.com/app"), "http refused");
    check(!validServer("rtmp://"), "no host");
    check(!validServer("rtmp://exa mple.com/app"), "space refused");
    check(!validServer("rtmp://user:pass@example.com/app"), "credentials in the host refused");
    check(joinUrl("rtmp://h/app/", "abc123") == "rtmp://h/app/abc123", "join with trailing slash");
    check(joinUrl("rtmp://h/app", "/abc123") == "rtmp://h/app/abc123", "join with leading slash on the key");
    check(joinUrl("rtmp://h/app", "").empty(), "empty key refused");
    check(joinUrl("rtmp://h/app", "a b").empty(), "key with a space refused");
    check(joinUrl("ftp://h/app", "k").empty(), "bad server refused");
    check(redact("rtmp://h:1935/app/live_123_secret") == "rtmp://h:1935/app/****", "key redacted");
    check(redact("rtmps://h/live2/abcd-efgh?x=1") == "rtmps://h/live2/****", "key and query redacted");
    check(redact("rtmp://h/app").find("****") != std::string::npos, "no key: still nothing secret");
    check(platform("twitch") && platform("twitch")->maxVideoKbps == 6000 && platform("twitch")->servers.size() >= 10, "Twitch (OBS service data)");
    check(platform("youtube") && std::string(platform("youtube")->servers[0].url).rfind("rtmps://", 0) == 0, "YouTube RTMPS first");
    check(platform("kick") && platform("kick")->needsRtmps && std::string(platform("kick")->servers[0].url).rfind("rtmps://", 0) == 0, "Kick needs RTMPS");
    check(platform("tiktok") && platform("tiktok")->servers.empty(), "TikTok: the user pastes the server");
    check(platform("nope") == nullptr, "unknown platform");
    for (const auto& p : platforms())
        for (const auto& s : p.servers) check(validServer(s.url), std::string("valid preset: ") + p.name + " " + s.name);
}

// ---- local server -------------------------------------------------------------------------------

struct MediaMtx {
    std::string exe, config;
#ifdef _WIN32
    PROCESS_INFORMATION pi{};
#else
    pid_t pid = -1;
#endif
    bool start() {
#ifdef _WIN32
        std::string cmd = "\"" + exe + "\" \"" + config + "\"";
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return false;
#else
        pid = fork();
        if (pid == 0) {
            execl(exe.c_str(), exe.c_str(), config.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
        if (pid < 0) return false;
#endif
        std::this_thread::sleep_for(1500ms);
        return true;
    }
    void kill() {
#ifdef _WIN32
        if (pi.hProcess) {
            TerminateProcess(pi.hProcess, 0);
            WaitForSingleObject(pi.hProcess, 5000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            pi = {};
        }
#else
        if (pid > 0) {
            ::kill(pid, SIGKILL);
            waitpid(pid, nullptr, 0);
            pid = -1;
        }
#endif
    }
    ~MediaMtx() { kill(); }
};

struct Fixture {
    std::vector<AVPacket*> packets;
    AVCodecParameters* video = nullptr;
    AVCodecParameters* audio = nullptr;
    AVRational videoTb{1, 1000}, audioTb{1, 1000};
    int videoIndex = -1, audioIndex = -1;
    double seconds = 2;
    ~Fixture() {
        for (auto* p : packets) av_packet_free(&p);
        avcodec_parameters_free(&video);
        avcodec_parameters_free(&audio);
    }
    bool load(const std::string& path) {
        AVFormatContext* in = nullptr;
        if (avformat_open_input(&in, path.c_str(), nullptr, nullptr) < 0) return false;
        avformat_find_stream_info(in, nullptr);
        for (unsigned i = 0; i < in->nb_streams; ++i) {
            AVStream* s = in->streams[i];
            if (s->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoIndex < 0) { videoIndex = int(i); video = avcodec_parameters_alloc(); avcodec_parameters_copy(video, s->codecpar); videoTb = s->time_base; }
            if (s->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioIndex < 0) { audioIndex = int(i); audio = avcodec_parameters_alloc(); avcodec_parameters_copy(audio, s->codecpar); audioTb = s->time_base; }
        }
        AVPacket* p = av_packet_alloc();
        while (av_read_frame(in, p) >= 0) {
            if (p->stream_index == videoIndex || p->stream_index == audioIndex) packets.push_back(av_packet_clone(p));
            av_packet_unref(p);
        }
        av_packet_free(&p);
        avformat_close_input(&in);
        return video && audio && !packets.empty();
    }
};

// Reads a published stream back for a few seconds; true when H.264 video and AAC sound arrive.
static bool readBack(const std::string& url, int& videoPackets, int& audioPackets) {
    videoPackets = audioPackets = 0;
    AVFormatContext* in = nullptr;
    AVDictionary* o = nullptr;
    av_dict_set(&o, "rw_timeout", "8000000", 0);
    if (const int r = avformat_open_input(&in, url.c_str(), nullptr, &o); r < 0) {
        av_dict_free(&o);
        std::printf("  read back %s: open failed (%d)\n", url.c_str(), r);
        return false;
    }
    av_dict_free(&o);
    avformat_find_stream_info(in, nullptr);
    int v = -1, a = -1;
    for (unsigned i = 0; i < in->nb_streams; ++i) {
        if (in->streams[i]->codecpar->codec_id == AV_CODEC_ID_H264) v = int(i);
        if (in->streams[i]->codecpar->codec_id == AV_CODEC_ID_AAC) a = int(i);
    }
    AVPacket* p = av_packet_alloc();
    const auto until = std::chrono::steady_clock::now() + 3s;
    int ret = 0;
    while (std::chrono::steady_clock::now() < until && (ret = av_read_frame(in, p)) >= 0) {
        if (p->stream_index == v) ++videoPackets;
        if (p->stream_index == a) ++audioPackets;
        av_packet_unref(p);
    }
    std::printf("  read back %s: %d video and %d audio packets (last read %d)\n", url.c_str(), videoPackets, audioPackets, ret);
    av_packet_free(&p);
    avformat_close_input(&in);
    return v >= 0 && a >= 0 && videoPackets > 20 && audioPackets > 20;
}

// The server only lists a path once media arrives (outputs start at a keyframe), so retry for a while.
static bool readBackSoon(const std::string& url, int& videoPackets, int& audioPackets) {
    for (int attempt = 0; attempt < 6; ++attempt) {
        if (readBack(url, videoPackets, audioPackets)) return true;
        std::this_thread::sleep_for(1s);
    }
    return false;
}

static const char* name(State s) { return stateName(s); }

static void testLive(const std::string& exe, const std::string& config, const std::string& fixturePath) {
    Fixture fx;
    check(fx.load(fixturePath), "fixture loaded (H.264 + AAC)");
    if (!fx.video) return;
    MediaMtx server{exe, config};
    check(server.start(), "MediaMTX started");

    std::vector<std::string> logLines;
    std::mutex logLock;
    Outputs out([&](const std::string& line) {
        std::lock_guard lk(logLock);
        logLines.push_back(line);
        std::printf("  log: %s\n", line.c_str());
    });
    VideoFormat vf;
    vf.width = fx.video->width;
    vf.height = fx.video->height;
    vf.fps = 30;
    vf.extradata.assign(fx.video->extradata, fx.video->extradata + fx.video->extradata_size);
    std::string error;
    const std::string secret = "secret_key_123";
    const bool started = out.start({{"RTMP", "rtmp://127.0.0.1:1935/live/" + secret},
                                    {"RTMPS", "rtmps://127.0.0.1:1936/live/two"},
                                    {"Closed port", "rtmp://127.0.0.1:1/live/x"}},
                                   vf, fx.audio, error);
    check(started, "three outputs started " + error);

    // Feed the fixture in real time, looping, for the whole test.
    std::atomic<bool> feeding{true};
    std::thread feeder([&] {
        const auto t0 = std::chrono::steady_clock::now();
        for (int loop = 0; feeding; ++loop) {
            const int64_t offsetUs = int64_t(loop) * 2'000'000;
            for (AVPacket* p : fx.packets) {
                if (!feeding) break;
                const bool video = p->stream_index == fx.videoIndex;
                const int64_t ptsUs = av_rescale_q(p->pts, video ? fx.videoTb : fx.audioTb, AVRational{1, 1000000}) + offsetUs;
                const int64_t durUs = av_rescale_q(std::max<int64_t>(1, p->duration), video ? fx.videoTb : fx.audioTb, AVRational{1, 1000000});
                std::this_thread::sleep_until(t0 + std::chrono::microseconds(ptsUs));
                auto data = std::make_shared<const std::vector<uint8_t>>(p->data, p->data + p->size);
                if (video) out.pushVideo(data, ptsUs, durUs, (p->flags & AV_PKT_FLAG_KEY) != 0);
                else out.pushAudio(data, ptsUs, durUs);
            }
        }
    });

    auto stateOf = [&](size_t i) { return out.status()[i].state; };
    auto waitFor = [&](size_t i, State want, std::chrono::seconds limit) {
        const auto until = std::chrono::steady_clock::now() + limit;
        while (std::chrono::steady_clock::now() < until) {
            if (stateOf(i) == want) return true;
            std::this_thread::sleep_for(100ms);
        }
        return false;
    };
    bool ok = waitFor(0, State::Live, 15s);
    check(ok, std::string("RTMP output live (state ") + name(stateOf(0)) + ")");
    ok = waitFor(1, State::Live, 15s);
    check(ok, std::string("RTMPS output live (state ") + name(stateOf(1)) + ")");
    int v = 0, a = 0;
    ok = readBackSoon("rtmp://127.0.0.1:1935/live/" + secret, v, a);
    check(ok, "RTMP stream reads back with H.264 and AAC (" + std::to_string(v) + " video, " + std::to_string(a) + " audio)");
    ok = readBackSoon("rtmp://127.0.0.1:1935/live/two", v, a);
    check(ok, "RTMPS stream reads back with H.264 and AAC (" + std::to_string(v) + " video, " + std::to_string(a) + " audio)");
    const auto st = out.status();
    check(st[0].bytesSent > 10000 && st[0].framesSent > 30, "bytes and frames counted");
    check(st[2].state == State::Connecting || st[2].state == State::Reconnecting, std::string("closed port keeps retrying (") + name(st[2].state) + ")");
    check(!st[2].error.empty(), "closed port shows an error: " + st[2].error);
    check(st[0].server.find(secret) == std::string::npos && st[0].server.find("****") != std::string::npos, "status shows the server without the key");
    {
        std::lock_guard lk(logLock);
        bool leaked = false;
        for (const auto& l : logLines) leaked = leaked || l.find(secret) != std::string::npos;
        check(!leaked, "the stream key never appears in the log");
    }

    // The server goes away and comes back.
    server.kill();
    ok = waitFor(0, State::Reconnecting, 20s);
    check(ok, std::string("server gone: reconnecting (") + name(stateOf(0)) + ")");
    check(server.start(), "MediaMTX restarted");
    ok = waitFor(0, State::Live, 40s);
    check(ok, std::string("server back: live again (") + name(stateOf(0)) + ")");
    ok = readBackSoon("rtmp://127.0.0.1:1935/live/" + secret, v, a);
    check(ok, "after the reconnect the stream reads back (" + std::to_string(v) + " video, " + std::to_string(a) + " audio)");
    check(out.status()[0].reconnects >= 1, "reconnect counted");

    const auto t = std::chrono::steady_clock::now();
    feeding = false;
    feeder.join();
    out.stop();
    const double stopSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count();
    check(stopSeconds < 5.0, "stop() returns quickly (" + std::to_string(stopSeconds) + " s)");
    check(!out.running(), "nothing running after stop");
    const auto end = out.status();
    check(end[0].state == State::Stopped && end[1].state == State::Stopped, "outputs stopped");
}

int main() {
    testUrls();
    const char* exe = std::getenv("PUSHFPS_STREAM_SERVER");
    const char* config = std::getenv("PUSHFPS_STREAM_CONFIG");
    const char* fixture = std::getenv("PUSHFPS_STREAM_FIXTURE");
    if (exe && config && fixture) testLive(exe, config, fixture);
    else std::printf("SKIP live tests (PUSHFPS_STREAM_SERVER / _CONFIG / _FIXTURE not set)\n");
    // Refusals.
    Outputs out;
    std::string error;
    VideoFormat vf;
    vf.width = 64; vf.height = 64; vf.extradata = {0, 0, 0, 1};
    check(!out.start({}, vf, nullptr, error), "no targets refused");
    check(!out.start({{"x", "http://bad/app/k"}}, vf, nullptr, error), "bad URL refused");
    vf.hevc = true;
    check(!out.start({{"x", "rtmp://h/app/k"}}, vf, nullptr, error), "HEVC refused for FLV");
    std::printf("%s\n", failures ? "LiveStream tests FAILED" : "LiveStream tests passed");
    return failures ? 1 : 0;
}
