// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#version 450

layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D texSampler;

layout (push_constant) uniform settings {
    float gamma;
    bool hdr;
    bool srgb_input;
    vec4 picture; // spartanport: x0, y0, x1, y1 of the part shown; the rest is black (cutscene bars)
    // spartanport: the player's picture controls on the SDR-encoded value (picture_adjust() below).
    float picture_power;      // display gamma / 2.2: 1 as made
    float picture_contrast;   // around mid grey: 1 as made
    float picture_brightness; // black level, -0.125 to 0.125 of the range: 0 as made
    bool picture_adjust;      // false: none of the three runs, so the neutral picture is unchanged
} pp;

const float cutoff = 0.0031308, a = 1.055, b = 0.055, d = 12.92;
vec3 gamma(vec3 rgb) {
    return mix(
        a * pow(rgb, vec3(1.0 / (2.4 + 1.0 - pp.gamma))) - b,
        d * rgb / pp.gamma,
        lessThan(rgb, vec3(cutoff))
    );
}

// Exact inverse of gamma() at unit gamma, for buffers that are sRGB encoded but must be sampled
// through a UNORM view because Vulkan has no sRGB variant of the 10-bit format.
vec3 degamma(vec3 rgb) {
    return mix(
        pow(max(rgb + b, 0.0) / a, vec3(2.4)),
        rgb / d,
        lessThan(rgb, vec3(d * cutoff))
    );
}

// spartanport: picture controls (from red1ty's Driveclub port): an SDR-encoded value in [0, 1] in,
// one out. The gamma power keeps black and white and moves the mid tones; contrast turns around
// mid grey. Brightness sets the black level as a TV's Brightness does: a positive value lifts
// black towards it and a negative one crushes the darkest values to black, and white stays white
// either way, so highlights do not clip. The input is clamped so pow() never sees a negative value.
vec3 picture_adjust(vec3 encoded) {
    vec3 v = clamp(encoded, 0.0, 1.0);
    // pow() is not exact on GPUs, so a gamma of 2.20 skips it and only brightness and contrast act.
    if (pp.picture_power != 1.0) v = pow(v, vec3(pp.picture_power));
    v = clamp((v - 0.5) * pp.picture_contrast + 0.5, 0.0, 1.0);
    const float lift = pp.picture_brightness;
    v = lift >= 0.0 ? v * (1.0 - lift) + lift : max(v + lift, 0.0) / (1.0 + lift);
    return clamp(v, 0.0, 1.0);
}

// spartanport: triangular dither of one 8-bit step (the SDR swapchain is R8G8B8A8 or B8G8R8A8,
// vk_swapchain.cpp), so a curve applied to an 8-bit picture does not show as bands in smooth dark
// gradients. Two independent values per pixel from the pcg2d hash (Jarzynski and Olano, "Hash
// Functions for GPU Rendering", 2020) make a triangular noise from -1 to +1 step, zero on average,
// the same in every frame. Values within half a step of black or white get none, so black and white
// stay exact (black stays black on an OLED): the sRGB encode of white is 0.99999994, not 1.0.
uvec2 pcg2d(uvec2 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;
    v = v ^ (v >> 16u);
    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;
    return v ^ (v >> 16u);
}

vec3 dither_8bit(vec3 v) {
    const vec2 n = vec2(pcg2d(uvec2(gl_FragCoord.xy))) * (1.0 / 4294967296.0);
    const float half_step = 0.5 / 255.0;
    const vec3 inside = vec3(greaterThan(v, vec3(half_step))) * vec3(lessThan(v, vec3(1.0 - half_step)));
    return v + inside * ((n.x + n.y - 1.0) / 255.0);
}

void main() {
    if (any(lessThan(uv, pp.picture.xy)) || any(greaterThan(uv, pp.picture.zw))) {
        color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    vec4 color_linear = texture(texSampler, uv);
    if (pp.hdr) {
        color = color_linear;
    } else {
        if (pp.srgb_input) color_linear.rgb = degamma(color_linear.rgb);
        vec3 encoded = gamma(color_linear.rgb);
        if (pp.picture_adjust) encoded = dither_8bit(picture_adjust(encoded));
        color = vec4(encoded, color_linear.a);
    }
}
