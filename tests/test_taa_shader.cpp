// SPDX-License-Identifier: GPL-2.0-or-later
// Run the production TAA shader on synthetic inputs, including undefined/invalid history.
#include <cassert>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <fstream>
#include <vector>
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/texture_cache/image.h"
#include "video_core/host_shaders/taa_comp.h"
#include "video_core/host_shaders/taa_sharpen_comp.h"
#include <vk_mem_alloc.h>

int main(int argc, char** argv) {
    using namespace Vulkan;
    Instance instance(0, false);
    static vk::detail::DynamicLoader loader;
    vk::detail::DispatchLoaderDynamic d;
    d.init(loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
    d.init(instance.GetInstance()); d.init(instance.GetDevice());
    const auto device = instance.GetDevice();
    Scheduler scheduler(instance);
    // Odd dimensions and a pixel beyond 2000 exercise bounds and subpixel precision.
    constexpr u32 W = 2049, H = 7, N = W * H, X = 2010, center = 3 * W + X;
    std::array<VideoCore::UniqueImage, 7> images;
    std::array<vk::UniqueImageView, 7> views;
    for (u32 i = 0; i < images.size(); ++i) {
        const auto format = i != 4 && i != 6 ? vk::Format::eR32G32B32A32Sfloat
                                  : vk::Format::eR16G16B16A16Sfloat;
        images[i] = VideoCore::UniqueImage(device, instance.GetAllocator());
        images[i].Create({.imageType = vk::ImageType::e2D, .format = format,
            .extent = {W,H,1}, .mipLevels = 1, .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1, .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
                     vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst});
        views[i] = device.createImageViewUnique({.image = vk::Image(images[i]),
            .viewType = vk::ImageViewType::e2D, .format = format,
            .subresourceRange = {vk::ImageAspectFlagBits::eColor,0,1,0,1}},nullptr,d).value;
    }
    std::array<vk::DescriptorSetLayoutBinding,7> bindings{};
    for (u32 i = 0; i < bindings.size(); ++i) {
        bindings[i] = {.binding=i,
            .descriptorType=i<4 ? vk::DescriptorType::eCombinedImageSampler : vk::DescriptorType::eStorageImage,
            .descriptorCount=1,.stageFlags=vk::ShaderStageFlagBits::eCompute};
    }
    auto descriptors = device.createDescriptorSetLayoutUnique({
        .flags=vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
        .bindingCount=u32(bindings.size()),.pBindings=bindings.data()},nullptr,d).value;
    const vk::PushConstantRange push{vk::ShaderStageFlagBits::eCompute,0,128};
    auto layout = device.createPipelineLayoutUnique({.setLayoutCount=1,.pSetLayouts=&*descriptors,
        .pushConstantRangeCount=1,.pPushConstantRanges=&push},nullptr,d).value;
    std::vector<u32> override_code;
    if (argc > 1) {
        std::ifstream source(argv[1],std::ios::binary|std::ios::ate);
        const auto bytes=source.tellg();
        assert(source && bytes>0 && bytes%4==0);
        override_code.resize(size_t(bytes)/4);
        source.seekg(0); source.read(reinterpret_cast<char*>(override_code.data()),bytes);
        assert(source);
    }
    auto module = device.createShaderModuleUnique({
        .codeSize=override_code.empty() ? sizeof(TAA_COMP) : override_code.size()*4,
        .pCode=override_code.empty() ? TAA_COMP : override_code.data()},nullptr,d).value;
    auto pipeline = device.createComputePipelineUnique({}, {
        .stage={.stage=vk::ShaderStageFlagBits::eCompute,.module=*module,.pName="main"},
        .layout=*layout},nullptr,d).value;
    auto sampler = device.createSamplerUnique({.magFilter=vk::Filter::eNearest,
        .minFilter=vk::Filter::eNearest,.mipmapMode=vk::SamplerMipmapMode::eNearest,
        .addressModeU=vk::SamplerAddressMode::eClampToEdge,
        .addressModeV=vk::SamplerAddressMode::eClampToEdge,
        .addressModeW=vk::SamplerAddressMode::eClampToEdge},nullptr,d).value;
    VkBuffer staging{}; VmaAllocation allocation{}; VmaAllocationInfo ai{};
    const VkBufferCreateInfo bi{.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size=N*16*4, .usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    const VmaAllocationCreateInfo ac{.flags=VMA_ALLOCATION_CREATE_MAPPED_BIT |
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, .usage=VMA_MEMORY_USAGE_AUTO,
        .requiredFlags=VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    assert(vmaCreateBuffer(instance.GetAllocator(),&bi,&ac,&staging,&allocation,&ai)==VK_SUCCESS);
    const auto barrier = [&](vk::CommandBuffer cmd, vk::PipelineStageFlags2 source,
                             vk::AccessFlags2 source_access, vk::PipelineStageFlags2 dest,
                             vk::AccessFlags2 dest_access) {
        const vk::MemoryBarrier2 b{.srcStageMask=source,.srcAccessMask=source_access,
            .dstStageMask=dest,.dstAccessMask=dest_access};
        cmd.pipelineBarrier2({.memoryBarrierCount=1,.pMemoryBarriers=&b},d);
    };
    const auto half = [](u16 v) {
        const int exponent=(v>>10)&31;
        const float mantissa=float(v&1023);
        return (v&0x8000 ? -1.f : 1.f)*
            (exponent==0 ? std::ldexp(mantissa,-24) : std::ldexp(1.f+mantissa/1024.f,exponent-15));
    };
    const auto run = [&](const char* label, u32 reset, float history, float history_depth,
                         float motion, float high, float expected, bool poison=false,
                         float scene_depth=0.5f, float z_translation=0.f,
                         float jitter_x=0.f, bool edge=false, float depth_slope=0.f,
                         bool thin_edge=false, bool history_edge=false,
                         bool thin_history=false) {
        auto* pixels = static_cast<float*>(ai.pMappedData);
        for (u32 image = 0; image < 4; ++image) {
            for (u32 p = 0; p < N; ++p) {
                float* c=pixels+image*N*4+p*4;
                c[0]=c[1]=c[2]=image==0 ? (p==center ? 0.25f : high) :
                    image==1 ? scene_depth : image==2 ? 0.f : history;
                c[3]=image==3 ? history_depth : 1.f;
                if (image==2) c[0]=motion;
                if (image==1) c[0]=std::clamp(scene_depth+(int(p%W)-int(X))*depth_slope,0.f,1.f);
                if (image==1 && thin_edge && p%W==X-1) c[0]=0.99f;
                if (image==3 && poison) c[0]=c[1]=c[2]=std::numeric_limits<float>::quiet_NaN();
                // A tap of another surface (behind this one) must not blend in.
                if (image==3 && edge && p%W==X+1) c[3]=0.75f;
                // A one-pixel line in front in the history only (missed by this jitter phase).
                if (image==3 && thin_history && p%W!=X) c[3]=scene_depth;
                if (image==3 && history_edge && p%W!=X) c[0]=c[1]=c[2]=0.25f;
            }
        }
        // A dark neighbor and a bright neighbor make history clipping measurable.
        pixels[(center-1)*4]=pixels[(center-1)*4+1]=pixels[(center-1)*4+2]=0.f;
        const auto cmd=scheduler.CommandBuffer();
        std::array<vk::ImageMemoryBarrier2,7> transitions{};
        for (u32 i=0;i<7;++i) {
            transitions[i]={.srcStageMask=vk::PipelineStageFlagBits2::eAllCommands,
                .srcAccessMask=vk::AccessFlagBits2::eNone,.dstStageMask=vk::PipelineStageFlagBits2::eAllCommands,
                .dstAccessMask=vk::AccessFlagBits2::eMemoryRead|vk::AccessFlagBits2::eMemoryWrite,
                .oldLayout=vk::ImageLayout::eUndefined,.newLayout=vk::ImageLayout::eGeneral,
                .image=vk::Image(images[i]),.subresourceRange={vk::ImageAspectFlagBits::eColor,0,1,0,1}};
        }
        cmd.pipelineBarrier2({.imageMemoryBarrierCount=7,.pImageMemoryBarriers=transitions.data()},d);
        for (u32 i=0;i<4;++i) {
            const vk::BufferImageCopy region{.bufferOffset=i*N*16,
                .imageSubresource={vk::ImageAspectFlagBits::eColor,0,0,1},.imageExtent={W,H,1}};
            cmd.copyBufferToImage(staging,vk::Image(images[i]),vk::ImageLayout::eGeneral,region,d);
        }
        barrier(cmd,vk::PipelineStageFlagBits2::eTransfer,vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eComputeShader,vk::AccessFlagBits2::eShaderRead);
        std::array<vk::DescriptorImageInfo,7> infos{};
        std::array<vk::WriteDescriptorSet,7> writes{};
        for (u32 i=0;i<7;++i) {
            infos[i]={.sampler=i<4 ? *sampler : vk::Sampler{}, .imageView=*views[i],
                .imageLayout=vk::ImageLayout::eGeneral};
            writes[i]={.dstBinding=i,.descriptorCount=1,.descriptorType=bindings[i].descriptorType,
                .pImageInfo=&infos[i]};
        }
        struct Params {
            float x,y;u32 reset,pad;
            std::array<float,4> proj,prev_proj,previous_z;
            std::array<float,16> reproject;
        } params{jitter_x,0,reset,0,{1,1,1,-0.05f},{1,1,1,-0.05f},
                 {0,0,1,z_translation},{}};
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute,*pipeline,d);
        cmd.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute,*layout,0,writes,d);
        cmd.pushConstants(*layout,vk::ShaderStageFlagBits::eCompute,0,sizeof(params),&params,d);
        cmd.dispatch((W+7)/8,(H+7)/8,1,d);
        barrier(cmd,vk::PipelineStageFlagBits2::eComputeShader,vk::AccessFlagBits2::eShaderWrite,
                vk::PipelineStageFlagBits2::eTransfer,vk::AccessFlagBits2::eTransferRead);
        for (u32 i=4;i<6;++i) {
            const vk::BufferImageCopy region{.bufferOffset=(i-4)*16,
                .imageSubresource={vk::ImageAspectFlagBits::eColor,0,0,1},
                .imageOffset={s32(X),3,0},.imageExtent={1,1,1}};
            cmd.copyImageToBuffer(vk::Image(images[i]),vk::ImageLayout::eGeneral,staging,region,d);
        }
        barrier(cmd,vk::PipelineStageFlagBits2::eTransfer,vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eHost,vk::AccessFlagBits2::eHostRead);
        scheduler.Finish();
        const auto* result=static_cast<u16*>(ai.pMappedData);
        if (std::abs(half(result[0])-expected)>=0.001f) {
            std::fprintf(stderr,"TAA regression: %s: actual %.6f, expected %.6f\n",
                         label,half(result[0]),expected);
            std::fflush(stdout);
        }
        assert(std::abs(half(result[0])-expected)<0.001f);
        const auto* next=reinterpret_cast<const float*>(static_cast<const char*>(ai.pMappedData)+16);
        assert(half(result[3])==1.f &&
               std::abs(next[3]-(thin_edge ? 0.99f : scene_depth+jitter_x*depth_slope))<1e-7f);
        assert(std::abs(next[0]-expected)<0.001f);
        std::printf("TAA shader: %s PASS (%.3f)\n",label,half(result[0]));
    };
    run("reset ignores poisoned history",1,0.75f,0.5f,0,1,0.25f,true);
    run("reset unjitters current color",1,0.75f,0.5f,0,1,0.625f,true,0.5f,0,0.5f);
    run("temporal accumulation",0,0.75f,0.5f,0,1,0.74f);
    run("motion reduces persistence",0,0.75f,0.5f,4,1,0.7075f);
    run("neighborhood clipping",0,1,0.5f,0,0.3f,0.299f);
    // Foreground history over this surface without motion (an object that moved away, or a
    // thin one this jitter phase missed) is kept but clipped to the current neighbourhood.
    run("static disocclusion is clipped",0,0.75f,0.25f,0,0.3f,0.299f);
    run("moving disocclusion is rejected",0,0.75f,0.25f,2,1,0.25f);
    run("out-of-frame motion",0,0.75f,0.5f,50,1,0.25f);
    run("invalid history",0,0.75f,0.5f,0,1,0.25f,true);
    run("far depth retains full precision",0,0.75f,0.99998f,0,1,0.74f,false,0.99998f);
    run("far unrelated surface rejected",0,0.75f,0.9998f,2,1,0.25f,false,0.99998f);
    run("camera translation preserves history",0,0.75f,0.75f,0,1,0.74f,false,0.5f,0.1f);
    run("bilinear edge keeps matching tap",0,0.75f,0.5f,0.5f,1,
        0.25f+0.5f*(0.98f-0.13f*0.5f/8.f)*0.5f,false,0.5f,0,0,true);
    run("sky retains history",0,0.75f,1.f,0,1,0.74f,false,1.f);
    run("sky rejects geometry",0,0.75f,0.99998f,0,1,0.25f,false,1.f);
    run("sloping depth positive jitter",0,0.75f,0.99036f,0,1,0.746f,
        false,0.99f,0,0.4f,false,0.0009f);
    run("sloping depth negative jitter",0,0.75f,0.98964f,0,1,0.738f,
        false,0.99f,0,-0.4f,false,0.0009f);
    run("thin foreground retains history",0,0.75f,0.99f,0,1,0.74f,
        false,1.f,0,0,false,0,true);
    run("thin line missed by this frame keeps history",0,0.75f,0.25f,0,1,0.74f,
        false,0.5f,0,0,false,0,false,false,true);
    run("tiny motion at large pixel coordinate",0,0.75f,0.5f,-0.00003f,1,0.739985f,
        false,0.5f,0,0,false,0,false,true);
    std::array<vk::DescriptorSetLayoutBinding,2> sharpen_bindings{};
    for (u32 i=0;i<2;++i) sharpen_bindings[i]={.binding=i,
        .descriptorType=vk::DescriptorType::eStorageImage,.descriptorCount=1,
        .stageFlags=vk::ShaderStageFlagBits::eCompute};
    auto sharpen_desc=device.createDescriptorSetLayoutUnique({
        .flags=vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
        .bindingCount=2,.pBindings=sharpen_bindings.data()},nullptr,d).value;
    const vk::PushConstantRange sharpen_push{vk::ShaderStageFlagBits::eCompute,0,4};
    auto sharpen_layout=device.createPipelineLayoutUnique({.setLayoutCount=1,.pSetLayouts=&*sharpen_desc,
        .pushConstantRangeCount=1,.pPushConstantRanges=&sharpen_push},nullptr,d).value;
    auto sharpen_module=device.createShaderModuleUnique({.codeSize=sizeof(TAA_SHARPEN_COMP),
        .pCode=TAA_SHARPEN_COMP},nullptr,d).value;
    auto sharpen_pipeline=device.createComputePipelineUnique({}, {
        .stage={.stage=vk::ShaderStageFlagBits::eCompute,.module=*sharpen_module,.pName="main"},
        .layout=*sharpen_layout},nullptr,d).value;
    const auto sharpen=[&](const char* label,float strength,float scale,bool flat) {
        auto* input=static_cast<float*>(ai.pMappedData);
        for (u32 p=0;p<N;++p) {
            for (u32 c=0;c<3;++c) input[p*4+c]=(p==center && !flat ? 0.55f : 0.5f)*scale;
            input[p*4+3]=0.99998f;
        }
        const auto cmd=scheduler.CommandBuffer();
        barrier(cmd,vk::PipelineStageFlagBits2::eAllCommands,
                vk::AccessFlagBits2::eMemoryRead|vk::AccessFlagBits2::eMemoryWrite,
                vk::PipelineStageFlagBits2::eTransfer,vk::AccessFlagBits2::eTransferWrite);
        const vk::BufferImageCopy upload{.imageSubresource={vk::ImageAspectFlagBits::eColor,0,0,1},
            .imageExtent={W,H,1}};
        cmd.copyBufferToImage(staging,vk::Image(images[5]),vk::ImageLayout::eGeneral,upload,d);
        barrier(cmd,vk::PipelineStageFlagBits2::eTransfer,vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderStorageRead|vk::AccessFlagBits2::eShaderStorageWrite);
        const std::array<vk::DescriptorImageInfo,2> si{{
            {.imageView=*views[5],.imageLayout=vk::ImageLayout::eGeneral},
            {.imageView=*views[4],.imageLayout=vk::ImageLayout::eGeneral}}};
        std::array<vk::WriteDescriptorSet,2> sw{};
        for (u32 i=0;i<2;++i) sw[i]={.dstBinding=i,.descriptorCount=1,
            .descriptorType=vk::DescriptorType::eStorageImage,.pImageInfo=&si[i]};
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute,*sharpen_pipeline,d);
        cmd.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute,*sharpen_layout,0,sw,d);
        cmd.pushConstants(*sharpen_layout,vk::ShaderStageFlagBits::eCompute,0,4,&strength,d);
        cmd.dispatch((W+7)/8,(H+7)/8,1,d);
        barrier(cmd,vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderStorageRead|vk::AccessFlagBits2::eShaderStorageWrite,
                vk::PipelineStageFlagBits2::eTransfer,vk::AccessFlagBits2::eTransferRead);
        const vk::BufferImageCopy history_copy{.bufferOffset=N*16,
            .imageSubresource={vk::ImageAspectFlagBits::eColor,0,0,1},.imageExtent={W,H,1}};
        cmd.copyImageToBuffer(vk::Image(images[5]),vk::ImageLayout::eGeneral,staging,history_copy,d);
        const vk::BufferImageCopy output_copy{.bufferOffset=N*32,
            .imageSubresource={vk::ImageAspectFlagBits::eColor,0,0,1},.imageExtent={W,H,1}};
        cmd.copyImageToBuffer(vk::Image(images[4]),vk::ImageLayout::eGeneral,staging,output_copy,d);
        barrier(cmd,vk::PipelineStageFlagBits2::eTransfer,vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eHost,vk::AccessFlagBits2::eHostRead);
        scheduler.Finish();
        const auto* bytes=static_cast<const char*>(ai.pMappedData);
        assert(!std::memcmp(bytes,bytes+N*16,N*16)); // Color and depth history remain exact.
        const auto* output=reinterpret_cast<const u16*>(bytes+N*32);
        for (u32 p=0;p<N;++p) {
            for (u32 c=0;c<3;++c) {
                const float value=half(output[p*4+c]);
                assert(std::isfinite(value) && value>=0 && value<=std::max(1.f,scale));
                if (flat || strength<=0) {
                    if (std::abs(value-input[p*4+c])>=0.003f)
                        std::fprintf(stderr,"RCAS %s pixel %u: %.6f expected %.6f\n",label,p,value,input[p*4+c]);
                    assert(std::abs(value-input[p*4+c])<0.003f);
                }
            }
            assert(half(output[p*4+3])==1.f);
        }
        const float value=half(output[center*4]);
        if (!flat && strength>0 && scale==1) assert(value>input[center*4]+0.002f);
        std::printf("TAA RCAS: %s PASS (%.4f), history unchanged\n",label,value);
        return value;
    };
    sharpen("zero bypass",0,1,false);
    sharpen("lower limit",-1,1,false);
    const float middle=sharpen("half strength",0.5f,1,false);
    const float maximum=sharpen("full strength",1,1,false);
    assert(maximum>middle);
    assert(sharpen("above one (extra pass for FSR)",2,1,false)>maximum);
    sharpen("flat gray",1,1,true);
    sharpen("flat black",1,0,true);
    sharpen("flat HDR",1,8,true);
    sharpen("HDR detail",1,8,false);
    vmaDestroyBuffer(instance.GetAllocator(),staging,allocation);
}
