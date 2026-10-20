// SPDX-License-Identifier: GPL-2.0-or-later
#include "spartanport_png.h"

#include <cstdio>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

namespace SpartanPng {

namespace {
constexpr int kMaxSide = 4096; // save icons are 228x128; anything far larger is not one
}

bool DecodeRgba(const unsigned char* data, size_t size, Image& out) {
    out = {};
    if (!data || !size || size > 64u << 20) {
        return false;
    }
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_PNG);
    AVCodecContext* context = codec ? avcodec_alloc_context3(codec) : nullptr;
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    bool ok = context && packet && frame && avcodec_open2(context, codec, nullptr) == 0 &&
              av_new_packet(packet, int(size)) == 0;
    if (ok) {
        std::memcpy(packet->data, data, size);
        ok = avcodec_send_packet(context, packet) == 0 && avcodec_send_packet(context, nullptr) == 0 &&
             avcodec_receive_frame(context, frame) == 0 && frame->width > 0 && frame->height > 0 &&
             frame->width <= kMaxSide && frame->height <= kMaxSide;
    }
    if (ok) {
        SwsContext* sws = sws_getContext(frame->width, frame->height, AVPixelFormat(frame->format),
                                         frame->width, frame->height, AV_PIX_FMT_RGBA, SWS_POINT,
                                         nullptr, nullptr, nullptr);
        ok = sws != nullptr;
        if (ok) {
            out.width = frame->width;
            out.height = frame->height;
            out.rgba.resize(size_t(out.width) * out.height * 4);
            uint8_t* planes[4] = {out.rgba.data(), nullptr, nullptr, nullptr};
            const int strides[4] = {out.width * 4, 0, 0, 0};
            ok = sws_scale(sws, frame->data, frame->linesize, 0, frame->height, planes, strides) ==
                 frame->height;
            sws_freeContext(sws);
        }
    }
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&context);
    if (!ok) {
        out = {};
    }
    return ok;
}

bool LoadRgba(const char* path, Image& out) {
    out = {};
    FILE* file = path ? std::fopen(path, "rb") : nullptr;
    if (!file) {
        return false;
    }
    std::vector<unsigned char> bytes;
    unsigned char chunk[65536];
    size_t n;
    while ((n = std::fread(chunk, 1, sizeof(chunk), file)) > 0 && bytes.size() <= (64u << 20)) {
        bytes.insert(bytes.end(), chunk, chunk + n);
    }
    std::fclose(file);
    return DecodeRgba(bytes.data(), bytes.size(), out);
}

} // namespace SpartanPng
