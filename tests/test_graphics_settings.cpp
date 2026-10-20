// spartanport.ini graphics keys (spartanport_settings.cpp, spartanport_graphics.h).
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include "spartanport_graphics.h"
#include "spartanport_settings.h"

namespace G = SpartanGraphics;

static int SetEnvironment(const char* name, const char* value) {
#if defined(_WIN32)
    return _putenv_s(name, value);
#else
    return value[0] ? setenv(name, value, 1) : unsetenv(name);
#endif
}

static std::string Read(const char* path) {
    std::ifstream in(path);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

int main() {
    assert(G::ParseDisplayMode("borderless") == G::Borderless && G::ParseDisplayMode("bogus") == G::Windowed);
    // Monitor preference (window.cpp): empty means the primary display, and that default is distinct
    // from an explicit index 0. The launcher's value is untrusted text, so anything else falls back.
    assert(G::ParseMonitor("") == G::PrimaryMonitor && G::PrimaryMonitor != 0);
    assert(G::ParseMonitor("0") == 0);
    assert(G::ParseMonitor("1") == 1 && G::ParseMonitor("2") == 2 && G::ParseMonitor("42") == 42);
    assert(G::ParseMonitor("007") == 7);  // leading zeros are still an index
    assert(G::ParseMonitor(" 1") == G::PrimaryMonitor && G::ParseMonitor("1 ") == G::PrimaryMonitor);
    assert(G::ParseMonitor("abc") == G::PrimaryMonitor && G::ParseMonitor("1x") == G::PrimaryMonitor);
    assert(G::ParseMonitor("-1") == G::PrimaryMonitor && G::ParseMonitor("+1") == G::PrimaryMonitor);
    // Too large for an int falls back instead of wrapping into a bogus index: the exact INT_MAX + 1
    // boundary, and an input far past any int.
    const std::string widest = std::to_string(std::numeric_limits<int>::max());
    assert(G::ParseMonitor(widest) == std::numeric_limits<int>::max());
    assert(G::ParseMonitor(std::to_string(std::numeric_limits<int>::max() + 1LL)) == G::PrimaryMonitor);
    assert(G::ParseMonitor("99999999999999999999") == G::PrimaryMonitor);
    assert(G::ParseFpsLimit(240) == 240 && G::ParseFpsLimit(144) == 144 && G::ParseFpsLimit(180) == 180 && G::ParseFpsLimit(100) == 100 && G::ParseFpsLimit(75) == 0);
    assert(G::ParseResolution("900p") == 3 && G::ParseResolution("1440p") == 4 && G::ParseResolution("8K") == 22);
    // #238: 900p (1600x900) sits between 720p and 1440p in the built-in list.
    assert(G::Resolutions[3] == "900p" && G::Resolutions[2] == "720p" && G::Resolutions[4] == "1440p");
    // The window's starting size (window.cpp): built-in names, a mod's size, native otherwise.
    assert(G::ResolutionSize("720p") == std::make_pair(1280, 720));
    assert(G::ResolutionSize("900p") == std::make_pair(1600, 900));
    assert(G::ResolutionSize("4K") == std::make_pair(3840, 2160));
    assert(G::ResolutionSize("3440x1440") == std::make_pair(3440, 1440));
    assert(G::ResolutionSize("native") == std::make_pair(1920, 1080));
    assert(G::ResolutionSize("8K") == std::make_pair(7680, 4320));
    assert(G::ParseResolution("invalid") == 0);
    for (const auto name : G::Resolutions) assert(G::ResolutionSize(name).first > 0);
    assert(G::ParseEngineFps(240) == 240 && G::ParseEngineFps(144) == 144 && G::ParseEngineFps(180) == 180 && G::ParseEngineFps(100) == 100 && G::ParseEngineFps(90) == 120);
    // R3 + L1 + R1 / F9: the next rate this launch supports, back to 60 after the last.
    assert(G::NextEngineFps(60, 240) == 100 && G::NextEngineFps(100, 240) == 120);
    assert(G::NextEngineFps(120, 240) == 144 && G::NextEngineFps(144, 240) == 180 && G::NextEngineFps(180, 240) == 240 && G::NextEngineFps(240, 240) == 60);
    assert(G::NextEngineFps(100, 120) == 120 && G::NextEngineFps(120, 120) == 60 && G::NextEngineFps(60, 60) == 60);
    assert(G::ParseFramesQueued(2) == 2 && G::ParseFramesQueued(0) == 0 && G::ParseFramesQueued(5) == 1);
    assert(G::ParseRcasStrength(100) == 100 && G::ParseRcasStrength(101) == 75 && G::ParseRcasStrength(-1) == 75);
    assert(G::RcasAttenuation(100) == 0.0f && G::RcasAttenuation(0) == 2.0f && G::RcasAttenuation(75) == 0.5f);
    assert(G::FsrPassRuns(true, false, 1280, 720, 2560, 1440) && !G::FsrPassRuns(false, false, 1280, 720, 2560, 1440));
    assert(G::FsrPassRuns(false, true, 1920, 1080, 1920, 1080) && !G::FsrPassRuns(true, false, 1920, 1080, 1920, 1080));
    assert(!G::FsrPassRuns(true, true, 2560, 1440, 1920, 1080));  // larger image: regular scaling
    assert(G::EffectiveFps(0, 120) == 120 && G::EffectiveFps(240, 120) == 120 && G::EffectiveFps(60, 240) == 60);
    // An unpatched engine steps 1/60 s per frame: presenting faster speeds the game up.
    assert(G::PresentLimit(120, 60, 60) == 60 && G::PresentLimit(240, 120, 120) == 120);
    assert(G::PresentLimit(240, 240, 240) == 240 && G::PresentLimit(30, 60, 240) == 30);
    assert(G::PresentLimit(0, 60, 60) == 0 && G::PresentLimit(0, 240, 240) == 0);  // 0: paced by vblank
    // A live engine rate below the vblank (it runs at the highest rate): unlimited is the engine rate.
    assert(G::PresentLimit(0, 60, 240) == 60 && G::PresentLimit(0, 100, 240) == 100);
    assert(G::PresentLimit(240, 100, 240) == 100);

    const char* path = "graphics-settings-test.ini";
    {
        std::ofstream out(path);
        out << "display_mode=borderless\nvsync=0\nfps_limit=240\nasync_shaders=0\n"
               "render_resolution=1440p\nengine_fps=240\ndeferred_readback=0\nstale_readback=1\n"
               "fsr1=1\nrcas=0\nrcas_strength=40\nframes_queued=2\n";
    }
    SetEnvironment("SPARTAN_CONFIG", path);
    SetEnvironment("SPARTAN_ASYNC_SHADERS", "");
    auto& v = SpartanSettings::Get();
    SpartanSettings::Load();
    assert(v.display_mode == G::Borderless && !v.vsync && v.fps_limit == 240 && !v.async_shaders);
    assert(v.render_resolution == 4 && v.engine_fps == 240 && !v.deferred_readback && v.stale_readback);
    // FSR 1 left the menus in 2.2.0: a saved fsr1=1 (or SPARTAN_FSR1) no longer turns it on.
    assert(!v.fsr1 && !v.rcas && v.rcas_strength == 40 && v.frames_queued == 2);
    SetEnvironment("SPARTAN_FSR1", "1");
    SpartanSettings::Load();
    assert(!v.fsr1);
    SetEnvironment("SPARTAN_FSR1", "");
    assert(!SpartanSettings::GraphicsNeedRestart());
    assert(v.applied_engine_fps == 240);  // no SPARTAN_ENGINE_FPS: the configured rate
    SetEnvironment("SPARTAN_ENGINE_FPS", "60");    // run.py: the frame rate patch did not apply
    SpartanSettings::Load();
    assert(v.engine_fps == 240 && v.applied_engine_fps == 60 && !SpartanSettings::GraphicsNeedRestart());
    SetEnvironment("SPARTAN_ENGINE_FPS", "");
    v.render_resolution = 0;
    assert(SpartanSettings::GraphicsNeedRestart());
    v.render_resolution = 4;
    v.fps_limit = 60;   // live: no restart because of them
    v.engine_fps = 120;
    assert(!SpartanSettings::GraphicsNeedRestart());
    v.engine_fps = 240;

    SpartanSettings::Save();
    const std::string saved = Read(path);
    for (const char* line : {"display_mode=borderless\n", "vsync=0\n", "fps_limit=60\n", "async_shaders=0\n",
                             "render_resolution=1440p\n", "engine_fps=240\n", "deferred_readback=0\n",
                             "stale_readback=1\n", "fsr1=0\n", "rcas=0\n", "rcas_strength=40\n",
                             "frames_queued=2\n"}) {
        assert(saved.find(line) != std::string::npos);
    }

    {
        std::ofstream out(path);
        out << "render_resolution=8K\n";
    }
    SpartanSettings::Load();
    assert(v.render_resolution == G::ParseResolution("8K"));
    SpartanSettings::Save();
    assert(Read(path).find("render_resolution=8K\n") != std::string::npos);

    {
        // The resolutions of the enabled mods' patch files (run.py: SPARTAN_EXTRA_RESOLUTIONS) follow the built-in
        // ones and keep their names through a load and a save; repeated or built-in names are not added twice.
        SetEnvironment("SPARTAN_EXTRA_RESOLUTIONS", "3840x1080;2880x1200;;2880x1200;4K;3440x1440");
        const auto names = G::ResolutionNames();
        assert(names.size() == 25 && names[22] == "8K" && names[23] == "3840x1080" && names[24] == "2880x1200");
        {
            std::ofstream out(path);
            out << "render_resolution=2880x1200\n";
        }
        SpartanSettings::Load();
        assert(v.render_resolution == 24);
        SpartanSettings::Save();
        assert(Read(path).find("render_resolution=2880x1200\n") != std::string::npos);
        SetEnvironment("SPARTAN_EXTRA_RESOLUTIONS", "");  // the mod is off: its resolution is unknown, so native
        SpartanSettings::Load();
        assert(v.render_resolution == 0 && G::ResolutionNames().size() == G::Resolutions.size());
    }

    {
        std::ofstream out(path);
        out << "display_mode=huge\nfps_limit=7\nrender_resolution=invalid\nengine_fps=1\n"
               "rcas_strength=250\nframes_queued=9\n";
    }
    SetEnvironment("SPARTAN_ASYNC_SHADERS", "1");  // the environment still wins at start
    v.async_shaders = false;
    SpartanSettings::Load();
    assert(v.display_mode == G::Windowed && v.fps_limit == 0 && v.render_resolution == 0 && v.engine_fps == 120);
    assert(v.rcas_strength == 75 && v.frames_queued == 1);
    assert(v.async_shaders);
    {
        // Audio (src/runtime_audio.c): the master volume is 0 to 200 percent, the speaker layout is
        // named by key, anything unknown is automatic; both survive a save and the environment wins.
        namespace S = SpartanSettings;
        assert(S::ParseAudioChannels("auto") == S::AudioChannelsAuto && S::ParseAudioChannels("stereo") == S::AudioStereo);
        assert(S::ParseAudioChannels("5.1") == S::Audio51 && S::ParseAudioChannels("7.1") == S::Audio71);
        assert(S::ParseAudioChannels("2.0") == S::AudioStereo && S::ParseAudioChannels("6") == S::Audio51);
        assert(S::ParseAudioChannels("8") == S::Audio71 && S::ParseAudioChannels("") == S::AudioChannelsAuto);
        assert(S::ParseAudioChannels("bogus") == S::AudioChannelsAuto && S::ParseAudioChannels("7") == S::AudioChannelsAuto);
        for (int c = 0; c < S::AudioChannelsCount; ++c) assert(S::ParseAudioChannels(S::AudioChannelKeys[c]) == c);
        {
            std::ofstream out(path);
            out << "master_volume=150\naudio_channels=5.1\n";
        }
        SpartanSettings::Load();
        assert(v.master_volume == 150 && v.audio_channels == S::Audio51);
        SpartanSettings::Save();
        const std::string audio_ini = Read(path);
        assert(audio_ini.find("master_volume=150\n") != std::string::npos);
        assert(audio_ini.find("audio_channels=5.1\n") != std::string::npos);
        {
            std::ofstream out(path);
            out << "master_volume=900\naudio_channels=7.1\n";
        }
        SpartanSettings::Load();
        assert(v.master_volume == 200 && v.audio_channels == S::Audio71);
        {
            std::ofstream out(path);
            out << "master_volume=-5\naudio_channels=bogus\n";
        }
        SpartanSettings::Load();
        assert(v.master_volume == 0 && v.audio_channels == S::AudioChannelsAuto);
        {
            std::ofstream out(path);
            out << "master_volume=\n";
        }
        SpartanSettings::Load();
        assert(v.master_volume == 100);
        _putenv_s("SPARTAN_AUDIO_VOLUME", "80");
        _putenv_s("SPARTAN_AUDIO_CHANNELS", "stereo");
        SpartanSettings::Load();
        assert(v.master_volume == 80 && v.audio_channels == S::AudioStereo);
        _putenv_s("SPARTAN_AUDIO_VOLUME", "");
        _putenv_s("SPARTAN_AUDIO_CHANNELS", "");
    }

    {
        std::ofstream out(path);
        out << "cheat_double_jumps=1\ncheat_very_hard=1\n";
    }
    SpartanSettings::Load();
    SpartanSettings::Save();
    const std::string cheats = Read(path);
    assert(cheats.find("cheat_double_jumps=1\n") != std::string::npos);
    assert(cheats.find("cheat_very_hard=1\n") != std::string::npos);
    std::remove(path);
    std::puts("PASS: graphics settings");
}
