// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdio>
#include <fstream>

#include "video_core/renderer_vulkan/vk_precompile_state.h"

#undef NDEBUG
#include <cassert>

namespace fs = std::filesystem;
using namespace Vulkan;

static PrecompileMarker Done() {
    return {.manifest_id = 1,
            .meta_version = 2,
            .binary_version = 3,
            .pipeline_version = 4,
            .profile_hash = 5,
            .driver_hash = 6,
            .shaders_done = true,
            .pipelines_done = true};
}

static void Table() {
    const auto now = Done();
    const auto everything = [](const PrecompileWork& w) {
        return w.restart_cache && w.shaders && w.keys && w.pipelines && !w.new_only;
    };

    assert(!DecideWork(now, now).Any());
    assert(everything(DecideWork(std::nullopt, now)));

    auto stored = now;
    stored.profile_hash = 50; // other GPU
    assert(everything(DecideWork(stored, now)));

    stored = now;
    stored.binary_version = 30;
    assert(everything(DecideWork(stored, now)));

    stored = now;
    stored.meta_version = 20;
    assert(everything(DecideWork(stored, now)));

    stored = now;
    stored.driver_hash = 60; // only the driver: the SPIR-V still holds
    auto work = DecideWork(stored, now);
    assert(work.pipelines && !work.shaders && !work.keys && !work.restart_cache);

    stored = now;
    stored.pipeline_version = 40;
    work = DecideWork(stored, now);
    assert(work.pipelines && work.keys && !work.shaders && !work.restart_cache);

    stored = now;
    stored.manifest_id = 10; // only what is new
    work = DecideWork(stored, now);
    assert(work.shaders && work.keys && work.pipelines && work.new_only && !work.restart_cache);

    stored = now;
    stored.shaders_done = false; // skipped halfway: goes on from where it stopped
    work = DecideWork(stored, now);
    assert(work.shaders && work.pipelines && !work.restart_cache);

    stored = now;
    stored.pipelines_done = false;
    work = DecideWork(stored, now);
    assert(work.pipelines && !work.shaders && !work.restart_cache);
}

static void MarkerFormat() {
    auto marker = Done();
    marker.pipelines_done = false;
    auto bytes = EncodeMarker(marker);
    const auto back = DecodeMarker(bytes);
    assert(back);
    assert(back->manifest_id == 1 && back->driver_hash == 6 && back->pipeline_version == 4);
    assert(back->shaders_done && !back->pipelines_done);

    assert(!DecodeMarker({}));
    assert(!DecodeMarker(std::span<const u8>{bytes.data(), 8}));
    bytes[0] ^= 0xFF;
    assert(!DecodeMarker(bytes));
    assert(DecideWork(DecodeMarker(bytes), Done()).restart_cache);
}

static void Crashes() {
    const auto dir = fs::temp_directory_path() / "spartanport-precompile-state-test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const auto journal = dir / "journal.bin";
    const auto skip = dir / "skip.txt";

    // Nothing happened: nothing to skip.
    assert(LoadSkipList(journal, skip, "gpu:driver1").empty());

    // A run that ends well leaves no journal.
    {
        CrashJournal run;
        assert(run.Open(journal, 4));
        run.Mark(1, 0xAA);
        run.Clear(1);
        run.Finish();
    }
    assert(!fs::exists(journal));
    assert(LoadSkipList(journal, skip, "gpu:driver1").empty());

    // A crash: the slots still marked are what the driver held. Flushed data is all a dead process
    // leaves, and the destructor does not clear slots, so this run stands for it.
    {
        CrashJournal run;
        assert(run.Open(journal, 4));
        run.Mark(0, 0x11);
        run.Mark(2, 0x22);
        run.Mark(3, 0x33);
        run.Clear(0);
    }
    assert(fs::exists(journal));
    const std::vector<u64> crashed{0x22, 0x33};
    assert(LoadSkipList(journal, skip, "gpu:driver1") == crashed);
    assert(!fs::exists(journal));

    // Same driver: still skipped, once each, after another crash on one of them.
    {
        CrashJournal run;
        assert(run.Open(journal, 1));
        run.Mark(0, 0x22);
    }
    assert(LoadSkipList(journal, skip, "gpu:driver1") == crashed);

    // A new driver tries them again; the old one still skips them.
    assert(LoadSkipList(journal, skip, "gpu:driver2").empty());
    assert(LoadSkipList(journal, skip, "gpu:driver1") == crashed);

    // Damaged lines are left out.
    std::ofstream(skip, std::ios::app) << "gpu:driver1\n\nnot a line\n";
    assert(LoadSkipList(journal, skip, "gpu:driver1") == crashed);

    // The skip list cannot be written: the crash is still skipped now, and the journal stays so
    // the next start records it.
    const auto blocked = dir / "blocked";
    fs::create_directories(blocked); // a folder where the list file should be
    {
        CrashJournal run;
        assert(run.Open(journal, 1));
        run.Mark(0, 0x77);
    }
    assert(LoadSkipList(journal, blocked, "gpu:driver1") == std::vector<u64>{0x77});
    assert(fs::exists(journal));
    fs::remove_all(dir);
}

static void ListByRegion() {
    const auto dir = fs::temp_directory_path() / "spartanport-shader-lists";
    fs::remove_all(dir);
    fs::create_directories(dir);
    // No list at all: the game's own name, which the preparation then finds missing.
    assert(ShaderListPath(dir, "CUSA01740") == dir / "CUSA01740.spm");

    // Another region of the game uses the list it ships with.
    std::ofstream(dir / "CUSA01623.spm") << "list";
    for (const char* region : {"CUSA01715", "CUSA01720", "CUSA01740"}) {
        assert(ShaderListPath(dir, region) == dir / "CUSA01623.spm");
    }
    assert(ShaderListPath(dir, "CUSA01623") == dir / "CUSA01623.spm");

    // Another game never takes it; a region with its own list keeps it.
    assert(ShaderListPath(dir, "CUSA99999") == dir / "CUSA99999.spm");
    std::ofstream(dir / "CUSA01740.spm") << "own";
    assert(ShaderListPath(dir, "CUSA01740") == dir / "CUSA01740.spm");
    fs::remove_all(dir);

    // The list itself names its game: the shipped one is accepted by the other regions only.
    assert(ShaderListFitsGame("CUSA01623", "CUSA01623"));
    for (const char* region : {"CUSA01715", "CUSA01720", "CUSA01740"}) {
        assert(ShaderListFitsGame("CUSA01623", region));
        assert(ShaderListFitsGame(region, region));
    }
    assert(!ShaderListFitsGame("CUSA01623", "CUSA99999"));
    assert(!ShaderListFitsGame("CUSA01740", "CUSA01623"));
}

int main() {
    Table();
    MarkerFormat();
    Crashes();
    ListByRegion();
    std::printf("PASS: precompile state\n");
    return 0;
}
