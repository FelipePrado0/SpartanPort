// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#version 450 core
#extension GL_EXT_samplerless_texture_functions : require

layout (binding = 0, set = 0) uniform texture2D color;

layout (location = 0) in vec2 uv;

void main()
{
    ivec2 coord = ivec2(uv * vec2(textureSize(color, 0).xy));
#ifdef SAMPLE_FALLBACK
    // spartanport: SPARTAN_LIGHTING_TEST=msdepth: a sample that reads 0 (a channel the single-channel
    // source never wrote) takes the red channel instead.
    vec4 val = texelFetch(color, coord, 0);
    gl_FragDepth = (gl_SampleID == 0 || val[gl_SampleID] != 0.0) ? val[gl_SampleID] : val.r;
#else
    gl_FragDepth = texelFetch(color, coord, 0)[gl_SampleID];
#endif
}
