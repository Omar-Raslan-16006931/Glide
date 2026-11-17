// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// finalizeMp4: a fragmented MP4 written like a live recording (1 MHz video
// clock, uneven frame times, AAC sound, frag_keyframe+empty_moov) becomes a
// standard MP4 with the same packets, a 90 kHz video clock and no fragments.
// Uses FFmpeg's own MPEG-4 and AAC encoders so no binary fixture is needed.
#include "veyra/engine/Mp4Finalize.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

namespace fs = std::filesystem;
static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

namespace {
struct Written { int video = 0, audio = 0; double seconds = 0; };

// Writes `path` as LiveRecorder does: fragmented MP4, video time base 1/1000000.
bool writeFragmented(const fs::path& path, Written& written) {
    const int width = 320, height = 180, frames = 150;
    AVFormatContext* mux = nullptr;
    if (avformat_alloc_output_context2(&mux, nullptr, "mp4", nullptr) < 0 || !mux) return false;
    const AVCodec* vcodec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    const AVCodec* acodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!vcodec || !acodec) { std::fprintf(stderr, "encoders missing\n"); return false; }
    AVCodecContext* v = avcodec_alloc_context3(vcodec);
    v->width = width; v->height = height; v->pix_fmt = AV_PIX_FMT_YUV420P;
    v->time_base = AVRational{1, 50000}; v->framerate = AVRational{60, 1};
    v->gop_size = 60; v->max_b_frames = 0; v->bit_rate = 2000000;
    v->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (avcodec_open2(v, vcodec, nullptr) < 0) return false;
    AVCodecContext* a = avcodec_alloc_context3(acodec);
    a->sample_rate = 48000; a->sample_fmt = AV_SAMPLE_FMT_FLTP; a->bit_rate = 128000;
    av_channel_layout_default(&a->ch_layout, 2);
    a->time_base = AVRational{1, 48000};
    a->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (avcodec_open2(a, acodec, nullptr) < 0) return false;

    AVStream* vs = avformat_new_stream(mux, nullptr);
    avcodec_parameters_from_context(vs->codecpar, v);
    vs->time_base = AVRational{1, 1000000}; vs->avg_frame_rate = AVRational{60, 1};
    AVStream* as = avformat_new_stream(mux, nullptr);
    avcodec_parameters_from_context(as->codecpar, a);
    as->time_base = AVRational{1, 48000};
    av_dict_set(&as->metadata, "handler_name", "Veyra sound", 0);
    if (avio_open(&mux->pb, path.string().c_str(), AVIO_FLAG_WRITE) < 0) return false;
    AVDictionary* options = nullptr;
    av_dict_set(&options, "movflags", "frag_keyframe+empty_moov+default_base_moof", 0);
    if (avformat_write_header(mux, &options) < 0) return false;
    av_dict_free(&options);

    AVPacket* pkt = av_packet_alloc();
    auto drain = [&](AVCodecContext* c, AVStream* s, bool video) {
        while (avcodec_receive_packet(c, pkt) == 0) {
            av_packet_rescale_ts(pkt, c->time_base, s->time_base);
            pkt->stream_index = s->index;
            if (av_interleaved_write_frame(mux, pkt) < 0) return false;
            (video ? written.video : written.audio)++;
        }
        return true;
    };
    AVFrame* frame = av_frame_alloc();
    frame->format = v->pix_fmt; frame->width = width; frame->height = height;
    av_frame_get_buffer(frame, 0);
    int64_t us = 0;
    for (int i = 0; i < frames; ++i) {
        av_frame_make_writable(frame);
        for (int y = 0; y < height; ++y) std::memset(frame->data[0] + y * frame->linesize[0], (i * 3 + y) & 255, width);
        for (int y = 0; y < height / 2; ++y) { std::memset(frame->data[1] + y * frame->linesize[1], 128, width / 2); std::memset(frame->data[2] + y * frame->linesize[2], 128, width / 2); }
        frame->pts = us / 20; // encoder clock 1/50000
        us += (i % 3 == 0) ? 16000 : 17333; // uneven, like presented frames
        if (avcodec_send_frame(v, frame) < 0 || !drain(v, vs, true)) return false;
    }
    avcodec_send_frame(v, nullptr); drain(v, vs, true);
    written.seconds = double(us) / 1e6;
    AVFrame* sound = av_frame_alloc();
    sound->format = a->sample_fmt; sound->nb_samples = a->frame_size; sound->sample_rate = 48000;
    av_channel_layout_copy(&sound->ch_layout, &a->ch_layout);
    av_frame_get_buffer(sound, 0);
    for (int64_t at = 0; at < int64_t(written.seconds * 48000); at += a->frame_size) {
        av_frame_make_writable(sound);
        for (int ch = 0; ch < 2; ++ch) {
            auto* d = reinterpret_cast<float*>(sound->data[ch]);
            for (int n = 0; n < a->frame_size; ++n) d[n] = 0.2f * std::sin(float(at + n) * 0.05f);
        }
        sound->pts = at;
        if (avcodec_send_frame(a, sound) < 0 || !drain(a, as, false)) return false;
    }
    avcodec_send_frame(a, nullptr); drain(a, as, false);
    const bool ok = av_write_trailer(mux) >= 0;
    avio_closep(&mux->pb);
    avformat_free_context(mux);
    av_frame_free(&frame); av_frame_free(&sound); av_packet_free(&pkt);
    avcodec_free_context(&v); avcodec_free_context(&a);
    return ok;
}

// Top-level box names of an MP4 file.
std::vector<std::string> topBoxes(const fs::path& path) {
    std::vector<std::string> names;
    std::ifstream file(path, std::ios::binary);
    for (;;) {
        unsigned char header[16];
        if (!file.read(reinterpret_cast<char*>(header), 8)) break;
        uint64_t size = (uint64_t(header[0]) << 24) | (uint64_t(header[1]) << 16) | (uint64_t(header[2]) << 8) | header[3];
        names.emplace_back(reinterpret_cast<char*>(header + 4), 4);
        uint64_t headerSize = 8;
        if (size == 1) {
            if (!file.read(reinterpret_cast<char*>(header + 8), 8)) break;
            size = 0; for (int i = 8; i < 16; ++i) size = (size << 8) | header[i];
            headerSize = 16;
        }
        if (size == 0) break; // to the end of the file
        if (size < headerSize) break;
        file.seekg(std::streamoff(size - headerSize), std::ios::cur);
    }
    return names;
}
bool has(const std::vector<std::string>& names, const char* name) {
    for (const auto& n : names) if (n == name) return true;
    return false;
}
} // namespace

int main() {
    const fs::path dir = fs::temp_directory_path() / "pushfps-mp4-finalize-test";
    std::error_code code;
    fs::remove_all(dir, code);
    fs::create_directories(dir);
    const fs::path file = dir / "Glide-compare-test.mp4";

    Written written;
    CHECK(writeFragmented(file, written));
    const auto before = topBoxes(file);
    CHECK(has(before, "moof"));
    std::printf("fragmented: video=%d audio=%d seconds=%.3f boxes=%zu\n", written.video, written.audio, written.seconds, before.size());

    const auto result = veyra::engine::finalizeMp4(file);
    std::printf("finalize ok=%d video=%lld audio=%lld error=%s\n", result.ok, result.videoPackets, result.audioPackets, result.error.c_str());
    CHECK(result.ok);
    CHECK(result.videoPackets == written.video);
    CHECK(result.audioPackets == written.audio);
    CHECK(!fs::exists(dir / "Glide-compare-test.finishing.mp4"));

    const auto after = topBoxes(file);
    CHECK(!has(after, "moof"));
    CHECK(has(after, "moov"));
    CHECK(has(after, "mdat"));

    AVFormatContext* in = nullptr;
    CHECK(avformat_open_input(&in, file.string().c_str(), nullptr, nullptr) >= 0);
    if (in) {
        CHECK(avformat_find_stream_info(in, nullptr) >= 0);
        CHECK(in->nb_streams == 2);
        int video = -1;
        for (unsigned i = 0; i < in->nb_streams; ++i) if (in->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) video = int(i);
        CHECK(video >= 0);
        if (video >= 0) {
            const AVStream* s = in->streams[video];
            std::printf("video time base %d/%d frames=%lld duration=%.3f s\n", s->time_base.num, s->time_base.den,
                        (long long)s->nb_frames, s->duration * av_q2d(s->time_base));
            CHECK(s->time_base.num == 1 && s->time_base.den == 90000);
            CHECK(s->nb_frames == written.video);
            CHECK(std::fabs(s->duration * av_q2d(s->time_base) - written.seconds) < 0.05);
        }
        for (unsigned i = 0; i < in->nb_streams; ++i) {
            if (in->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) continue;
            const AVDictionaryEntry* handler = av_dict_get(in->streams[i]->metadata, "handler_name", nullptr, 0);
            CHECK(handler && std::string(handler->value) == "Veyra sound");
        }
        // Every packet decodes in order with increasing timestamps.
        AVPacket* pkt = av_packet_alloc();
        int64_t last = INT64_MIN; int count = 0;
        while (av_read_frame(in, pkt) >= 0) {
            if (pkt->stream_index == video) { CHECK(pkt->dts > last); last = pkt->dts; ++count; }
            av_packet_unref(pkt);
        }
        CHECK(count == written.video);
        av_packet_free(&pkt);
        avformat_close_input(&in);
    }

    // A file that is not an MP4 is left alone.
    const fs::path junk = dir / "not-a-video.mp4";
    { std::ofstream out(junk, std::ios::binary); out << "not a video"; }
    const auto refused = veyra::engine::finalizeMp4(junk);
    CHECK(!refused.ok && !refused.error.empty());
    CHECK(fs::file_size(junk) == 11);
    CHECK(!fs::exists(dir / "not-a-video.finishing.mp4"));

    fs::remove_all(dir, code);
    if (failures) { std::fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
    std::printf("Mp4FinalizeTests passed\n");
    return 0;
}
