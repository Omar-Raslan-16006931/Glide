// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "veyra/engine/Mp4Finalize.h"

#include <cstdint>
#include <format>
#include <system_error>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
}

namespace veyra::engine {
namespace {
std::string avText(int rc) {
    char text[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(rc, text, sizeof(text));
    return std::format("{} ({})", text, rc);
}
std::string utf8Path(const std::filesystem::path& path) {
    const auto u8 = path.u8string();
    return std::string(u8.begin(), u8.end());
}
} // namespace

Mp4FinalizeResult finalizeMp4(const std::filesystem::path& path) {
    Mp4FinalizeResult result;
    auto temp = path;
    temp.replace_filename(path.stem().native() + std::filesystem::path(".finishing").native() + path.extension().native());

    AVFormatContext* in = nullptr;
    AVFormatContext* out = nullptr;
    AVPacket* packet = av_packet_alloc();
    bool tempCreated = false;
    struct Cleanup {
        AVFormatContext*& in; AVFormatContext*& out; AVPacket*& packet;
        ~Cleanup() {
            if (out) { if (out->pb) avio_closep(&out->pb); avformat_free_context(out); out = nullptr; }
            if (in) avformat_close_input(&in);
            av_packet_free(&packet);
        }
    } cleanup{in, out, packet};
    auto fail = [&](std::string why) {
        result.ok = false;
        result.error = std::move(why);
        if (out) { if (out->pb) avio_closep(&out->pb); avformat_free_context(out); out = nullptr; }
        if (tempCreated) { std::error_code code; std::filesystem::remove(temp, code); }
        return result;
    };
    if (!packet) return fail("out of memory");

    int rc = avformat_open_input(&in, utf8Path(path).c_str(), nullptr, nullptr);
    if (rc < 0) return fail("cannot open the recording: " + avText(rc));
    if ((rc = avformat_find_stream_info(in, nullptr)) < 0) return fail("cannot read the recording's streams: " + avText(rc));
    if ((rc = avformat_alloc_output_context2(&out, nullptr, "mp4", nullptr)) < 0 || !out) return fail("cannot create the MP4 writer: " + avText(rc));

    std::vector<int> map(in->nb_streams, -1);
    int videoStreams = 0;
    for (unsigned i = 0; i < in->nb_streams; ++i) {
        const AVStream* source = in->streams[i];
        const auto type = source->codecpar->codec_type;
        if (type != AVMEDIA_TYPE_VIDEO && type != AVMEDIA_TYPE_AUDIO) continue;
        AVStream* target = avformat_new_stream(out, nullptr);
        if (!target) return fail("cannot add a stream");
        if ((rc = avcodec_parameters_copy(target->codecpar, source->codecpar)) < 0) return fail("cannot copy stream parameters: " + avText(rc));
        target->codecpar->codec_tag = 0;
        target->time_base = source->time_base;
        target->avg_frame_rate = source->avg_frame_rate;
        target->disposition = source->disposition;
        av_dict_copy(&target->metadata, source->metadata, 0);
        if (type == AVMEDIA_TYPE_VIDEO) ++videoStreams;
        map[i] = target->index;
    }
    if (videoStreams != 1) return fail(std::format("expected one video stream, found {}", videoStreams));
    av_dict_copy(&out->metadata, in->metadata, 0);

    if ((rc = avio_open(&out->pb, utf8Path(temp).c_str(), AVIO_FLAG_WRITE)) < 0) return fail("cannot create " + utf8Path(temp.filename()) + ": " + avText(rc));
    tempCreated = true;
    // 90 kHz is the usual video clock; the index stays at the end of the file
    // (no faststart pass: that would rewrite a long recording a second time).
    AVDictionary* options = nullptr;
    av_dict_set(&options, "video_track_timescale", "90000", 0);
    rc = avformat_write_header(out, &options);
    av_dict_free(&options);
    if (rc < 0) return fail("cannot write the MP4 header: " + avText(rc));

    std::vector<int64_t> lastDts(out->nb_streams, INT64_MIN);
    while ((rc = av_read_frame(in, packet)) >= 0) {
        const int index = packet->stream_index;
        if (index < 0 || unsigned(index) >= map.size() || map[size_t(index)] < 0) { av_packet_unref(packet); continue; }
        const int target = map[size_t(index)];
        av_packet_rescale_ts(packet, in->streams[index]->time_base, out->streams[target]->time_base);
        if (packet->dts == AV_NOPTS_VALUE) packet->dts = packet->pts;
        // Rescaling to the coarser clock can make two timestamps equal; keep them increasing.
        if (packet->dts != AV_NOPTS_VALUE && lastDts[size_t(target)] != INT64_MIN && packet->dts <= lastDts[size_t(target)]) {
            const int64_t shift = lastDts[size_t(target)] + 1 - packet->dts;
            packet->dts += shift;
            if (packet->pts != AV_NOPTS_VALUE) packet->pts += shift;
        }
        if (packet->dts != AV_NOPTS_VALUE) lastDts[size_t(target)] = packet->dts;
        packet->stream_index = target;
        packet->pos = -1;
        const bool video = out->streams[target]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO;
        if ((rc = av_interleaved_write_frame(out, packet)) < 0) return fail("cannot write a packet: " + avText(rc));
        if (video) ++result.videoPackets; else ++result.audioPackets;
    }
    if (rc != AVERROR_EOF) return fail("cannot read the recording: " + avText(rc));
    if (!result.videoPackets) return fail("the recording has no video");
    if ((rc = av_write_trailer(out)) < 0) return fail("cannot finish the MP4: " + avText(rc));
    if ((rc = avio_closep(&out->pb)) < 0) return fail("cannot close the MP4: " + avText(rc));
    avformat_free_context(out); out = nullptr;
    avformat_close_input(&in);

    std::error_code code;
    std::filesystem::rename(temp, path, code); // replaces the fragmented file
    if (code) return fail("cannot replace the recording: " + code.message());
    tempCreated = false;
    result.ok = true;
    return result;
}

} // namespace veyra::engine
