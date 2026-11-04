// SPDX-License-Identifier: GPL-2.0-or-later
#include "spartanport_trophy_media.h"

#include <algorithm>
#include <cmath>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace SpartanToastMedia {

namespace fs = std::filesystem;

namespace {

// An opened file, its best stream of one kind and a decoder for it; freed in any case.
struct Input {
    AVFormatContext* format = nullptr;
    AVCodecContext* context = nullptr;
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    int stream = -1;

    Input(const fs::path& path, AVMediaType type) {
        const auto name = path.u8string();
        // Local files only: a "sound" that is really a playlist must not open other files or URLs.
        AVDictionary* options = nullptr;
        av_dict_set(&options, "protocol_whitelist", "file", 0);
        const int opened = packet && frame ?
            avformat_open_input(&format, reinterpret_cast<const char*>(name.c_str()), nullptr, &options) : -1;
        av_dict_free(&options);
        if (opened < 0) return;
        const AVCodec* codec = nullptr;
        if (avformat_find_stream_info(format, nullptr) < 0) return;
        const int found = av_find_best_stream(format, type, -1, -1, &codec, 0);
        if (found < 0) return;
        const AVCodecParameters* par = format->streams[found]->codecpar;
        if (type == AVMEDIA_TYPE_VIDEO && (par->width > kMaxPictureSide || par->height > kMaxPictureSide))
            return; // refused before decoding a single huge frame
        if (!(context = avcodec_alloc_context3(codec)) || avcodec_parameters_to_context(context, par) < 0)
            return;
        context->pkt_timebase = format->streams[found]->time_base;
        if (avcodec_open2(context, codec, nullptr) < 0) return;
        stream = found;
    }
    ~Input() {
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&context);
        avformat_close_input(&format);
    }
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    bool Ok() const { return stream >= 0; }
};

} // namespace

std::vector<float> DecodeSound(const fs::path& path) {
    std::vector<float> pcm;
    Input in(path, AVMEDIA_TYPE_AUDIO);
    if (!in.Ok()) return pcm;
    SwrContext* resampler = nullptr;
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    if (swr_alloc_set_opts2(&resampler, &stereo, AV_SAMPLE_FMT_FLT, kAudioRate, &in.context->ch_layout,
                            in.context->sample_fmt, in.context->sample_rate, 0, nullptr) < 0 ||
        swr_init(resampler) < 0) {
        swr_free(&resampler);
        return pcm;
    }
    const size_t limit = size_t(kAudioRate * kMaxSoundSeconds) * 2;
    auto convert = [&](const uint8_t** input, int samples) {
        const int room = swr_get_out_samples(resampler, samples);
        if (room <= 0) return;
        const size_t used = pcm.size();
        pcm.resize(used + size_t(room) * 2);
        uint8_t* output = reinterpret_cast<uint8_t*>(pcm.data() + used);
        const int made = swr_convert(resampler, &output, room, input, samples);
        pcm.resize(used + size_t(std::max(made, 0)) * 2);
    };
    auto drain = [&] {
        while (avcodec_receive_frame(in.context, in.frame) >= 0) {
            convert(const_cast<const uint8_t**>(in.frame->extended_data), in.frame->nb_samples);
            av_frame_unref(in.frame);
        }
    };
    while (pcm.size() < limit && av_read_frame(in.format, in.packet) >= 0) {
        if (in.packet->stream_index == in.stream && avcodec_send_packet(in.context, in.packet) >= 0) drain();
        av_packet_unref(in.packet);
    }
    avcodec_send_packet(in.context, nullptr);
    drain();
    convert(nullptr, 0);
    swr_free(&resampler);
    if (pcm.size() > limit) pcm.resize(limit);
    return pcm;
}

Animation DecodeAnimation(const fs::path& path, const AnimationLimits& limits) {
    Animation out;
    Input in(path, AVMEDIA_TYPE_VIDEO);
    if (!in.Ok()) return out;
    const AVRational base = in.format->streams[in.stream]->time_base;
    SwsContext* scale = nullptr;
    double first_ms = -1.0, last_start = 0.0;
    std::vector<double> starts;
    bool full = false;
    auto take = [&] {
        while (!full && avcodec_receive_frame(in.context, in.frame) >= 0) {
            AVFrame* f = in.frame;
            if (f->width <= 0 || f->height <= 0 || f->width > kMaxPictureSide || f->height > kMaxPictureSide) {
                av_frame_unref(f);
                full = true;
                break;
            }
            if (out.width == 0) { // the first frame sets the size of all of them
                const float fit = std::min(1.0f, float(limits.max_side) / float(std::max(f->width, f->height)));
                out.width = std::max(1, int(std::lround(f->width * fit)));
                out.height = std::max(1, int(std::lround(f->height * fit)));
            }
            scale = sws_getCachedContext(scale, f->width, f->height, AVPixelFormat(f->format), out.width,
                                         out.height, AV_PIX_FMT_RGBA, SWS_AREA, nullptr, nullptr, nullptr);
            const int64_t ts = f->best_effort_timestamp;
            double ms = ts == AV_NOPTS_VALUE ? last_start + 100.0 : double(ts) * av_q2d(base) * 1000.0;
            if (first_ms < 0.0) first_ms = ms;
            ms -= first_ms;
            if (!starts.empty() && ms <= starts.back()) ms = starts.back() + 100.0; // no time: 100 ms
            const size_t frame_bytes = size_t(out.width) * out.height * 4;
            const size_t pixels = (starts.size() + 1) * size_t(out.width) * out.height;
            if (!scale || ms >= limits.max_ms || int(starts.size()) >= limits.max_frames ||
                pixels > limits.max_pixels) {
                av_frame_unref(f);
                full = true;
                break;
            }
            const size_t used = out.pixels.size();
            out.pixels.resize(used + frame_bytes);
            uint8_t* dst[] = {out.pixels.data() + used};
            const int stride[] = {out.width * 4};
            if (sws_scale(scale, f->data, f->linesize, 0, f->height, dst, stride) != out.height) {
                out.pixels.resize(used);
            } else {
                starts.push_back(ms);
                last_start = ms;
                const int64_t duration = f->duration;
                if (duration > 0) out.frame_ends.push_back(float(ms + double(duration) * av_q2d(base) * 1000.0));
                else out.frame_ends.push_back(-1.0f); // set from the next frame's start
            }
            av_frame_unref(f);
        }
    };
    while (!full && av_read_frame(in.format, in.packet) >= 0) {
        if (in.packet->stream_index == in.stream && avcodec_send_packet(in.context, in.packet) >= 0) take();
        av_packet_unref(in.packet);
    }
    if (!full) {
        avcodec_send_packet(in.context, nullptr);
        take();
    }
    sws_freeContext(scale);
    // A frame ends when the next one starts (some files give no durations); the last lasts 100 ms
    // without one. Ends never go backwards.
    for (size_t i = 0; i < out.frame_ends.size(); ++i) {
        const float next = i + 1 < starts.size() ? float(starts[i + 1]) : float(starts[i] + 100.0);
        if (out.frame_ends[i] <= float(starts[i])) out.frame_ends[i] = next;
        if (i + 1 < starts.size()) out.frame_ends[i] = std::min(out.frame_ends[i], next);
        if (i > 0) out.frame_ends[i] = std::max(out.frame_ends[i], out.frame_ends[i - 1] + 1.0f);
    }
    if (out.frame_ends.empty()) out = {};
    return out;
}

Sheet PackFrames(const Animation& a) {
    Sheet sheet;
    const int n = a.Frames();
    if (n <= 0 || a.width <= 0 || a.height <= 0) return sheet;
    sheet.columns = std::max(1, int(std::ceil(std::sqrt(double(n)))));
    const int rows = (n + sheet.columns - 1) / sheet.columns;
    sheet.width = sheet.columns * a.width;
    sheet.height = rows * a.height;
    sheet.pixels.assign(size_t(sheet.width) * sheet.height * 4, 0);
    const size_t frame_bytes = size_t(a.width) * a.height * 4, row_bytes = size_t(a.width) * 4;
    for (int i = 0; i < n; ++i) {
        const unsigned char* src = a.pixels.data() + frame_bytes * size_t(i);
        const int x = (i % sheet.columns) * a.width, y = (i / sheet.columns) * a.height;
        for (int line = 0; line < a.height; ++line)
            std::memcpy(sheet.pixels.data() + (size_t(y + line) * sheet.width + x) * 4, src + row_bytes * line,
                        row_bytes);
    }
    return sheet;
}

} // namespace SpartanToastMedia
