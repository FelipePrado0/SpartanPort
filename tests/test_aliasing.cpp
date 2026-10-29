// SPDX-License-Identifier: GPL-2.0-or-later
// Copy extents between images that share guest memory (texture_cache/aliasing.h); adapted from
// cuesta4/shadPS4 (eltutz) tests/test_aliasing.cpp. No game, Vulkan device or window required.
#include <cassert>
#include <cstdio>

#include <map>
#include <utility>

#include "video_core/amdgpu/resource.h"
#include "video_core/texture_cache/alias_state.h"
#include "video_core/texture_cache/aliasing.h"
#include "video_core/texture_cache/image_info.h"

using namespace VideoCore;

static ImageInfo Make(Extent3D size, vk::Format format = vk::Format::eR8G8B8A8Unorm) {
    ImageInfo info{};
    info.guest_address = 0x100000;
    info.num_bits = 32;
    info.pixel_format = format;
    info.type = AmdGpu::ImageType::Color2D;
    info.pitch = size.width;
    info.size = size;
    return info;
}

// Alias authority (alias_state.h) as TextureCache drives it: DeleteImage calls ForgetAliasMember
// while the deleted image's slot is still allocated (erased by a deferred operation), then the
// next UpdateImage of a surviving alias calls ResolveAliasBacking with IsLiveImage.
struct Slots {
    std::map<u32, u64> uids; // allocated slot index -> image uid
    bool IsLive(ImageId id, u64 uid) const {
        if (!id) return false;
        const auto it = uids.find(id.index);
        return it != uids.end() && it->second == uid;
    }
    auto Live() const {
        return [this](ImageId id, u64 uid) { return IsLive(id, uid); };
    }
};

static bool Names(const AliasState& state, ImageId id, u64 uid) {
    return (state.backing == id && state.backing_uid == uid) ||
           (state.writer == id && state.writer_uid == uid);
}

static void TestAliasAuthority() {
    const ImageId backing{1}, writer{2}, survivor{3};
    // The writer is evicted (its slot is still allocated until the deferred erase): the state
    // stops naming it, and once the slot is reused by another image (new uid) the survivor's
    // synchronization never treats the newcomer as the writer.
    {
        Slots slots{{{1, 10}, {2, 20}, {3, 30}}};
        AliasState state{.backing_uid = 10, .writer_uid = 20, .backing = backing,
                         .writer = writer, .members = 3};
        ForgetAliasMember(state, writer, 20, slots.Live());
        assert(!Names(state, writer, 20));
        slots.uids[2] = 21; // slot reuse: same ImageId, other image
        if (ResolveAliasBacking(state, slots.Live())) {
            assert(slots.IsLive(state.backing, state.backing_uid));
        }
        assert(!Names(state, writer, 21) && !Names(state, writer, 20));
        assert(state.members == 3);
    }
    // The backing is evicted while the writer (the newest contents) lives on: the survivors'
    // next synchronization must still find the writer, as SynchronizeAlias does when the
    // backing's slot is already erased. Otherwise they keep older contents for good.
    {
        Slots slots{{{1, 10}, {2, 20}, {3, 30}}};
        AliasState state{.backing_uid = 10, .writer_uid = 20, .backing = backing,
                         .writer = writer, .members = 3};
        ForgetAliasMember(state, backing, 10, slots.Live());
        assert(!Names(state, backing, 10));
        assert(ResolveAliasBacking(state, slots.Live()));
        assert(state.backing == writer && state.backing_uid == 20 && !state.writer);
    }
    // The backing is evicted with no writer: nothing is left to name.
    {
        Slots slots{{{1, 10}, {3, 30}}};
        AliasState state{.backing_uid = 10, .backing = backing, .members = 3};
        ForgetAliasMember(state, backing, 10, slots.Live());
        assert(!state.backing && !state.writer && state.members == 3);
    }
    // A stale reference: the backing named by an older uid of a reused slot is not live, the
    // writer is promoted; with no live writer the authority is reset.
    {
        Slots slots{{{1, 11}, {2, 20}}};
        AliasState state{.backing_uid = 10, .writer_uid = 20, .backing = backing,
                         .writer = writer, .members = 3};
        assert(ResolveAliasBacking(state, slots.Live()));
        assert(state.backing == writer && state.backing_uid == 20);
        slots.uids.erase(2);
        assert(!ResolveAliasBacking(state, slots.Live()));
        assert(!state.backing && !state.writer && state.members == 3);
    }
    // Deleting an image the state does not name (or the same slot under another uid) changes
    // nothing.
    {
        Slots slots{{{1, 10}, {2, 21}, {3, 30}}};
        AliasState state{.backing_uid = 10, .writer_uid = 20, .backing = backing,
                         .writer = writer, .members = 3};
        ForgetAliasMember(state, survivor, 30, slots.Live());
        ForgetAliasMember(state, writer, 21, slots.Live());
        assert(Names(state, backing, 10) && Names(state, writer, 20));
    }
}

int main() {
    TestAliasAuthority();
    // Same format: the smaller image is fully covered by the larger one.
    {
        const ImageInfo src = Make({1920, 1088, 1}), dst = Make({1920, 1080, 1});
        assert(GetAliasCopyExtent(src, dst) == dst.size);
        assert(GetAliasCopyExtent(dst, src) == dst.size);
    }
    // Compatible views of other formats: the common rows.
    {
        const ImageInfo src = Make({1920, 1088, 1}, vk::Format::eR8G8B8A8Uint);
        const ImageInfo dst = Make({1920, 1080, 1}, vk::Format::eR8G8B8A8Unorm);
        const Extent3D rows{1920, 1080, 1};
        assert(GetAliasCopyExtent(src, dst) == rows);
        assert(GetAliasCopyExtent(dst, src) == rows);
    }
    // Other memory layouts never alias.
    {
        const ImageInfo src = Make({1920, 1088, 1});
        ImageInfo dst = Make({1920, 1080, 1});
        dst.guest_address += 0x1000;
        assert(!GetAliasCopyExtent(src, dst));
        dst = Make({1920, 1080, 1});
        ++dst.pitch;
        assert(!GetAliasCopyExtent(src, dst));
        dst = Make({1920, 1080, 1});
        dst.props.is_depth = true;
        assert(!GetAliasCopyExtent(src, dst));
    }
    // Incompatible intersections.
    {
        const ImageInfo src = Make({1920, 1088, 1}, vk::Format::eR8G8B8A8Uint);
        ImageInfo dst = Make({1280, 1080, 1}, vk::Format::eR8G8B8A8Unorm);
        assert(!GetAliasCopyExtent(src, dst));
        dst = Make({1920, 1080, 1}, vk::Format::eR16G16B16A16Sfloat);
        assert(!GetAliasCopyExtent(src, dst));
        dst = Make({1920, 1080, 1}, vk::Format::eR8G8B8A8Unorm);
        dst.resources.levels = 2;
        assert(!GetAliasCopyExtent(src, dst));
    }
    std::puts("aliasing-test: OK");
    return 0;
}
