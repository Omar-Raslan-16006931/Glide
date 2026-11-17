// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "veyra/engine/LiveStream.h"

#include <cerrno>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
}

namespace veyra::engine::stream {

// ---- URL helpers --------------------------------------------------------------------------------

namespace {
bool cleanText(std::string_view s) {
    for (unsigned char c : s)
        if (c <= 32 || c == 127) return false;
    return true;
}
std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = char((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
    return out;
}
}  // namespace

bool validServer(std::string_view server) {
    if (server.size() < 9 || server.size() > 512 || !cleanText(server)) return false;
    const std::string l = lower(server.substr(0, 8));
    size_t hostStart;
    if (l.rfind("rtmps://", 0) == 0) hostStart = 8;
    else if (l.rfind("rtmp://", 0) == 0) hostStart = 7;
    else return false;
    const size_t hostEnd = server.find_first_of("/?", hostStart);
    const std::string_view host = server.substr(hostStart, hostEnd == std::string_view::npos ? std::string_view::npos : hostEnd - hostStart);
    return !host.empty() && host.front() != ':' && host.find('@') == std::string_view::npos;
}

std::string joinUrl(std::string_view server, std::string_view key) {
    if (!validServer(server) || key.empty() || key.size() > 512 || !cleanText(key)) return {};
    while (!key.empty() && key.front() == '/') key.remove_prefix(1);
    if (key.empty()) return {};
    std::string url(server);
    while (!url.empty() && url.back() == '/') url.pop_back();
    return url + "/" + std::string(key);
}

std::string redact(std::string_view url) {
    // scheme://host[:port]/app/KEY -> scheme://host[:port]/app/****
    const size_t scheme = url.find("://");
    if (scheme == std::string_view::npos) return "****";
    const size_t pathStart = url.find('/', scheme + 3);
    if (pathStart == std::string_view::npos) return std::string(url);
    const size_t appEnd = url.find('/', pathStart + 1);
    if (appEnd == std::string_view::npos) return std::string(url.substr(0, pathStart + 1)) + "****";
    return std::string(url.substr(0, appEnd + 1)) + "****";
}

const std::vector<Platform>& platforms() {
    // Servers and limits from OBS Studio's rtmp-services (plugins/rtmp-services/data/services.json,
    // GPL-2.0-or-later) where OBS lists the platform; Kick's from Kick's help pages; TikTok and custom
    // servers are pasted by the user (TikTok gives a server per stream).
    static const std::vector<Platform> list = {
        {"twitch", "Twitch",
         {{"US East (N. Virginia)", "rtmp://use10.contribute.live-video.net/app/"},
          {"US East (Ohio)", "rtmp://use20.contribute.live-video.net/app/"},
          {"US West (Oregon)", "rtmp://usw20.contribute.live-video.net/app/"},
          {"Europe (Frankfurt)", "rtmp://euc10.contribute.live-video.net/app/"},
          {"Europe (Ireland)", "rtmp://euw10.contribute.live-video.net/app/"},
          {"Europe (Paris)", "rtmp://euw30.contribute.live-video.net/app/"},
          {"Europe (Stockholm)", "rtmp://eun10.contribute.live-video.net/app/"},
          {"South America (Sao Paulo)", "rtmp://sae10.contribute.live-video.net/app/"},
          {"Asia Pacific (Tokyo)", "rtmp://apn10.contribute.live-video.net/app/"},
          {"Asia Pacific (Seoul)", "rtmp://apn20.contribute.live-video.net/app/"},
          {"Asia Pacific (Singapore)", "rtmp://aps10.contribute.live-video.net/app/"},
          {"Asia Pacific (Sydney)", "rtmp://aps20.contribute.live-video.net/app/"},
          {"Asia Pacific (Mumbai)", "rtmp://aps30.contribute.live-video.net/app/"}},
         6000, 320, false, "dashboard.twitch.tv -> Settings -> Stream"},
        {"youtube", "YouTube",
         {{"Primary (RTMPS)", "rtmps://a.rtmps.youtube.com:443/live2"},
          {"Primary (RTMP)", "rtmp://a.rtmp.youtube.com/live2"}},
         51000, 160, false, "studio.youtube.com -> Go live -> Stream"},
        {"kick", "Kick",
         {{"Kick (RTMPS)", "rtmps://fa723fc1b171.global-contribute.live-video.net:443/app"}},
         8000, 320, true, "kick.com -> Creator Dashboard -> Channel -> Stream URL and Key"},
        {"tiktok", "TikTok", {}, 6000, 192, false, "TikTok LIVE Producer (needs LIVE access) -> Server URL and Stream Key"},
        {"facebook", "Facebook Live", {{"Default", "rtmps://rtmp-api.facebook.com:443/rtmp/"}}, 9000, 128, true, "facebook.com/live/producer -> Streaming software"},
        {"trovo", "Trovo", {{"Default", "rtmp://livepush.trovo.live/live/"}}, 9000, 160, false, "studio.trovo.live -> Stream settings"},
        {"custom", "Custom", {}, 50000, 320, false, ""},
    };
    return list;
}

const Platform* platform(std::string_view id) {
    for (const auto& p : platforms())
        if (id == p.id) return &p;
    return nullptr;
}

const char* stateName(State s) {
    switch (s) {
    case State::Idle: return "idle";
    case State::Connecting: return "connecting";
    case State::Live: return "live";
    case State::Reconnecting: return "reconnecting";
    case State::Failed: return "failed";
    case State::Stopped: return "stopped";
    }
    return "?";
}

// ---- one connection -----------------------------------------------------------------------------

namespace {
std::string avText(int code) {
    // Readable text for the network errors the platforms cause most; on Windows FFmpeg otherwise prints
    // "Error number -138 occurred". 10053/10054/10060/10061 are the Winsock codes FFmpeg passes on there.
    if (code == AVERROR(ETIMEDOUT) || code == -10060) return "timed out (server not reachable)";
    if (code == AVERROR(ECONNREFUSED) || code == -10061) return "connection refused";
    if (code == AVERROR(ECONNRESET) || code == AVERROR(EPIPE) || code == -10053 || code == -10054) return "connection closed by the server";
    char buffer[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(code, buffer, sizeof(buffer));
    return buffer;
}
constexpr int64_t kQueueLimitUs = 4'000'000;          // more than 4 s waiting: drop to the next keyframe
constexpr size_t kQueueLimitBytes = size_t(96) << 20;  // or more than 96 MB
}  // namespace

struct Outputs::Connection {
    struct Packet {
        std::shared_ptr<const std::vector<uint8_t>> data;
        int64_t ptsUs = 0, durationUs = 0;
        bool video = true, key = false;
    };
    Target target;
    std::string server;  // redacted
    VideoFormat video;
    AVCodecParameters* audio = nullptr;  // owned copy
    Logger log;

    mutable std::mutex m;
    std::condition_variable cv;
    std::deque<Packet> queue;
    size_t queuedBytes = 0;
    bool stopping = false;
    std::atomic<bool> abort{false};
    TargetStatus st;
    std::chrono::steady_clock::time_point liveSince{}, rateSince{};
    uint64_t rateBytes = 0;
    std::thread thread;

    ~Connection() {
        if (thread.joinable()) thread.join();
        avcodec_parameters_free(&audio);
    }

    void say(const std::string& text) { if (log) log(target.name + " (" + server + "): " + text); }
    void setState(State s, const std::string& error = {}) {
        std::lock_guard lk(m);
        st.state = s;
        if (!error.empty()) st.error = error;
        if (s == State::Live) { liveSince = std::chrono::steady_clock::now(); st.error.clear(); }
    }

    static int interruptCb(void* opaque) { return static_cast<Connection*>(opaque)->abort.load() ? 1 : 0; }

    void push(Packet p) {
        {
            std::lock_guard lk(m);
            if (stopping) return;
            queue.push_back(std::move(p));
            queuedBytes += queue.back().data ? queue.back().data->size() : 0;
            // Too far behind (a slow connection): drop everything waiting and start again at the next
            // keyframe. A whole group of pictures goes, so the picture never breaks up.
            const int64_t span = queue.back().ptsUs - queue.front().ptsUs;
            if (span > kQueueLimitUs || queuedBytes > kQueueLimitBytes) {
                uint64_t frames = 0;
                for (const auto& q : queue) frames += q.video ? 1 : 0;
                st.framesDropped += frames;
                queue.clear();
                queuedBytes = 0;
                needKey = true;
            }
        }
        cv.notify_one();
    }
    bool needKey = true;  // under m

    // Opens the FLV connection. Returns null with a reason on failure.
    AVFormatContext* open(std::string& reason) {
        AVFormatContext* mux = nullptr;
        int rc = avformat_alloc_output_context2(&mux, nullptr, "flv", target.url.c_str());
        if (rc < 0 || !mux) { reason = "FLV muxer unavailable: " + avText(rc); return nullptr; }
        AVStream* vs = avformat_new_stream(mux, nullptr);
        AVStream* as = audio ? avformat_new_stream(mux, nullptr) : nullptr;
        if (!vs || (audio && !as)) { reason = "cannot create streams"; avformat_free_context(mux); return nullptr; }
        AVCodecParameters* vp = vs->codecpar;
        vp->codec_type = AVMEDIA_TYPE_VIDEO;
        vp->codec_id = AV_CODEC_ID_H264;
        vp->width = video.width;
        vp->height = video.height;
        vp->bit_rate = video.bitRate;
        if (!video.extradata.empty()) {
            vp->extradata = static_cast<uint8_t*>(av_mallocz(video.extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
            if (!vp->extradata) { reason = "out of memory"; avformat_free_context(mux); return nullptr; }
            std::memcpy(vp->extradata, video.extradata.data(), video.extradata.size());
            vp->extradata_size = int(video.extradata.size());
        }
        vs->time_base = AVRational{1, 1000};
        vs->avg_frame_rate = AVRational{std::max(1, video.fps), 1};
        if (as) {
            if (avcodec_parameters_copy(as->codecpar, audio) < 0) { reason = "cannot copy the sound format"; avformat_free_context(mux); return nullptr; }
            as->time_base = AVRational{1, 1000};
        }
        AVDictionary* options = nullptr;
        av_dict_set(&options, "rw_timeout", "10000000", 0);   // 10 s without progress: give up this connection
        av_dict_set(&options, "tcp_nodelay", "1", 0);
        AVIOInterruptCB cb{&Connection::interruptCb, this};
        rc = avio_open2(&mux->pb, target.url.c_str(), AVIO_FLAG_WRITE, &cb, &options);
        av_dict_free(&options);
        if (rc < 0) { reason = "cannot connect: " + avText(rc); avformat_free_context(mux); return nullptr; }
        mux->interrupt_callback = cb;
        mux->max_interleave_delta = 1'000'000;  // never hold the picture more than 1 s waiting for sound
        AVDictionary* header = nullptr;
        av_dict_set(&header, "flvflags", "no_duration_filesize", 0);  // a live stream cannot seek back
        rc = avformat_write_header(mux, &header);
        av_dict_free(&header);
        if (rc < 0) { reason = "server refused the stream: " + avText(rc); avio_closep(&mux->pb); avformat_free_context(mux); return nullptr; }
        return mux;
    }

    void close(AVFormatContext*& mux, bool clean) {
        if (!mux) return;
        if (clean) av_write_trailer(mux);
        avio_closep(&mux->pb);
        avformat_free_context(mux);
        mux = nullptr;
    }

    void run() {
        unsigned failures = 0;
        while (true) {
            {
                std::lock_guard lk(m);
                if (stopping) break;
            }
            setState(failures ? State::Reconnecting : State::Connecting);
            std::string reason;
            AVFormatContext* mux = open(reason);
            if (!mux) {
                ++failures;
                say("connect failed (" + std::to_string(failures) + "): " + reason);
                { std::lock_guard lk(m); st.error = reason; if (failures > 1) ++st.reconnects; }
                if (failures >= kMaxReconnects) { setState(State::Failed, reason); break; }
                // Wait 2, 4, 8 ... 30 s, dropping what arrives meanwhile.
                const auto wait = std::chrono::seconds(std::min(30, 1 << std::min(failures, 5u)));
                std::unique_lock lk(m);
                cv.wait_for(lk, wait, [&] { return stopping; });
                st.framesDropped += std::count_if(queue.begin(), queue.end(), [](const Packet& p) { return p.video; });
                queue.clear(); queuedBytes = 0; needKey = true;
                continue;
            }
            say("live");
            setState(State::Live);
            { std::lock_guard lk(m); needKey = true; rateSince = std::chrono::steady_clock::now(); rateBytes = 0; }
            int64_t base = INT64_MIN, lastVideo = INT64_MIN, lastAudio = INT64_MIN;
            bool ok = true, ending = false;
            while (ok) {
                std::deque<Packet> batch;
                {
                    std::unique_lock lk(m);
                    cv.wait_for(lk, std::chrono::milliseconds(50), [&] { return stopping || !queue.empty(); });
                    // Stopping: send what is queued (it is short), then close.
                    batch.swap(queue);
                    queuedBytes = 0;
                    ending = stopping;
                }
                for (auto& p : batch) {
                    bool skip = false;
                    {
                        std::lock_guard lk(m);
                        if (needKey) {
                            if (p.video && p.key) needKey = false;
                            else { skip = true; if (p.video) ++st.framesDropped; }
                        }
                    }
                    if (skip || !p.data || p.data->empty()) continue;
                    if (base == INT64_MIN) base = p.ptsUs;
                    AVPacket* out = av_packet_alloc();
                    if (!out || av_new_packet(out, int(p.data->size())) < 0) { av_packet_free(&out); ok = false; break; }
                    std::memcpy(out->data, p.data->data(), p.data->size());
                    const int index = p.video ? 0 : 1;
                    if (!p.video && !audio) { av_packet_free(&out); continue; }
                    out->stream_index = index;
                    int64_t ts = av_rescale_q(std::max<int64_t>(0, p.ptsUs - base), AVRational{1, 1000000}, mux->streams[index]->time_base);
                    int64_t& last = p.video ? lastVideo : lastAudio;
                    if (ts <= last) ts = last + 1;
                    last = ts;
                    out->pts = out->dts = ts;
                    out->duration = av_rescale_q(std::max<int64_t>(1, p.durationUs), AVRational{1, 1000000}, mux->streams[index]->time_base);
                    if (p.key) out->flags |= AV_PKT_FLAG_KEY;
                    const size_t bytes = p.data->size();
                    const int rc = av_interleaved_write_frame(mux, out);
                    av_packet_free(&out);
                    if (rc < 0) {
                        if (!abort.load()) { say("write failed: " + avText(rc)); std::lock_guard lk(m); st.error = "connection lost: " + avText(rc); }
                        ok = false;
                        break;
                    }
                    std::lock_guard lk(m);
                    st.bytesSent += bytes;
                    rateBytes += bytes;
                    if (p.video) ++st.framesSent;
                    const auto now = std::chrono::steady_clock::now();
                    const double secs = std::chrono::duration<double>(now - rateSince).count();
                    if (secs >= 2.0) { st.kbps = double(rateBytes) * 8.0 / 1000.0 / secs; rateSince = now; rateBytes = 0; }
                }
                if (ending) break;
            }
            close(mux, ok);
            if (ending) break;
            // The connection dropped: try again from the next keyframe.
            ++failures;
            { std::lock_guard lk(m); ++st.reconnects; }
            if (failures >= kMaxReconnects) { setState(State::Failed, "too many reconnects"); break; }
            setState(State::Reconnecting);
            std::unique_lock lk(m);
            cv.wait_for(lk, std::chrono::seconds(2), [&] { return stopping; });
            failures = std::min(failures, 3u);  // a connection that was live starts the back-off low again
        }
        std::lock_guard lk(m);
        if (st.state != State::Failed) st.state = State::Stopped;
        st.kbps = 0;
    }
};

Outputs::Outputs(Logger log) : log_(std::move(log)) {}

Outputs::~Outputs() { stop(); }

bool Outputs::start(const std::vector<Target>& targets, const VideoFormat& video, const AVCodecParameters* audio, std::string& error) {
    stop();
    connections_.clear();
    if (targets.empty()) { error = "no stream targets"; return false; }
    if (video.hevc) { error = "streams carry H.264 only"; return false; }
    if (video.width <= 0 || video.height <= 0 || video.extradata.empty()) { error = "no picture format yet"; return false; }
    for (const auto& t : targets) {
        if (t.url.size() < 10 || !validServer(t.url)) { error = "invalid stream URL for " + t.name; return false; }
    }
    for (const auto& t : targets) {
        auto c = std::make_unique<Connection>();
        c->target = t;
        c->server = redact(t.url);
        c->video = video;
        c->log = log_;
        if (audio) {
            c->audio = avcodec_parameters_alloc();
            if (!c->audio || avcodec_parameters_copy(c->audio, audio) < 0) { error = "cannot copy the sound format"; return false; }
        }
        c->st.name = t.name;
        c->st.server = c->server;
        c->st.state = State::Connecting;
        connections_.push_back(std::move(c));
    }
    for (auto& c : connections_) {
        Connection* raw = c.get();
        raw->thread = std::thread([raw] { raw->run(); });
    }
    return true;
}

void Outputs::pushVideo(std::shared_ptr<const std::vector<uint8_t>> data, int64_t ptsUs, int64_t durationUs, bool key) {
    for (auto& c : connections_) c->push({data, ptsUs, durationUs, true, key});
}

void Outputs::pushAudio(std::shared_ptr<const std::vector<uint8_t>> data, int64_t ptsUs, int64_t durationUs) {
    for (auto& c : connections_) c->push({data, ptsUs, durationUs, false, false});
}

void Outputs::stop() {
    for (auto& c : connections_) {
        { std::lock_guard lk(c->m); c->stopping = true; }
        c->cv.notify_all();
    }
    // A connection stuck in a network call gets a second, then is interrupted.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    for (auto& c : connections_) {
        while (c->thread.joinable()) {
            {
                std::lock_guard lk(c->m);
                if (c->st.state == State::Stopped || c->st.state == State::Failed) break;
            }
            if (std::chrono::steady_clock::now() > deadline) { c->abort = true; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (c->thread.joinable()) c->thread.join();
    }
}

std::vector<TargetStatus> Outputs::status() const {
    std::vector<TargetStatus> out;
    for (const auto& c : connections_) {
        std::lock_guard lk(c->m);
        TargetStatus s = c->st;
        if (s.state == State::Live) s.liveSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - c->liveSince).count();
        out.push_back(std::move(s));
    }
    return out;
}

bool Outputs::running() const {
    for (const auto& c : connections_) {
        std::lock_guard lk(c->m);
        if (c->st.state != State::Stopped && c->st.state != State::Failed) return true;
    }
    return false;
}

}  // namespace veyra::engine::stream
