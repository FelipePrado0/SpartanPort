// SPDX-License-Identifier: GPL-2.0-or-later
// Saved FSR 4 choices must be safe before the first rendered frame on older GPUs.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <initializer_list>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "spartanport_settings.h"

extern "C" uint64_t spartanportgpu_native_setting(unsigned function);

static void Environment(const char* name, const char* value) {
#ifdef _WIN32
    assert(_putenv_s(name, value ? value : "") == 0);
#else
    assert((value ? setenv(name, value, 1) : unsetenv(name)) == 0);
#endif
}

int main() {
    using namespace SpartanSettings;
#ifdef _WIN32
    char folder[MAX_PATH], path[MAX_PATH];
    assert(GetTempPathA(MAX_PATH, folder));
    assert(GetTempFileNameA(folder, "spr", 0, path));
#else
    char path[] = "/tmp/spartanport-upscaler-test-XXXXXX";
    const int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
#endif
    Environment("SPARTAN_CONFIG", path);
    Environment("SPARTAN_UPSCALER", nullptr);
    Environment("SPARTAN_UPSCALE_PRESET", nullptr);
    Environment("SPARTAN_RENDER_RES", nullptr);
    auto& s = Get();
    {
        FILE* config = std::fopen(path, "w");
        assert(config);
        std::fputs("upscaler=dlss\nframe_generation=dlss\n", config);
        std::fclose(config);
        Load();
        assert(s.upscaler == UpscalerOff && s.frame_generation == 0);
    }
    s.upscaler = UpscalerTaa;
    s.preset = Performance;
    assert(RenderPreset() == NativeAA && s.preset == Performance);
    Save();
    s.upscaler = UpscalerOff;
    Load();
    assert(s.upscaler == UpscalerTaa && s.preset == Performance);
    assert(RenderPreset() == NativeAA && !ResolutionNeedsRestart());
    s.upscaler = UpscalerFsr3;
    assert(RenderPreset() == Performance);
    for (bool fsr4 : {false, true}) {
        for (bool fsr411 : {false, true}) {
            for (int requested = 0; requested < UpscalerCount; ++requested) {
                FILE* config = std::fopen(path, "w");
                assert(config);
                std::fprintf(config, "upscaler=%s\npreset=3\noutput_res=2560x1440\n",
                             UpscalerName(requested));
                std::fclose(config);
                s.fsr4_problem = nullptr;
                Load();
                assert(s.upscaler == requested);
                ConfigureUpscalerSupport(fsr4, fsr411);
                const bool unsupported = (requested == UpscalerFsr4 && !fsr4) ||
                    (requested == UpscalerFsr411 && !fsr411);
                assert(s.upscaler == (unsupported ? UpscalerFsr3 : requested));
                assert(s.fsr4_supported == fsr4);
                assert(s.fsr411_supported == fsr411);
                assert(bool(s.fsr4_problem.load()) == unsupported);
                assert(s.preset == Performance && s.output_res == 2);
                // Startup patch settings still describe the already applied guest patches.
                assert(s.startup_upscaler == requested);
                ConfigureUpscalerSupport(fsr4, fsr411);
                assert(s.upscaler == (unsupported ? UpscalerFsr3 : requested));
            }
        }
    }
    std::string provider_problem = "AMD Vulkan runtime missing from this driver branch";
    s.upscaler = UpscalerFsr411;
    ConfigureUpscalerSupport(false, false, provider_problem.c_str());
    provider_problem[0] = 'X';
    assert(std::string_view(s.fsr4_problem.load()) ==
           "AMD Vulkan runtime missing from this driver branch");
    s.startup_preset = Quality;
    s.startup_upscaler = UpscalerFsr3;
    s.upscaler = UpscalerFsr3;
    s.preset = Performance;
    assert(RenderPreset() == Performance && !ResolutionNeedsRestart());
    Environment("SPARTAN_RENDER_RES", "1706x960");
    assert(RenderPreset() == Quality && ResolutionNeedsRestart());
    s.preset = Quality;
    assert(!ResolutionNeedsRestart());
    s.upscaler = UpscalerOff;
    assert(ResolutionNeedsRestart());
    s.upscaler = UpscalerFsr4;
    assert(!ResolutionNeedsRestart()); // provider change at the same patched size is live
    s.upscaler = UpscalerTaa;          // TAA needs native guest targets
    assert(ResolutionNeedsRestart());
    s.upscaler = UpscalerFsr4;
    const int output = s.startup_output_res;
    s.output_res = (output + 1) % OutputCount;
    assert(ResolutionNeedsRestart());
    s.output_res = output;
    Environment("SPARTAN_RENDER_RES", "");
    assert(!FixedRenderSession() && !ResolutionNeedsRestart());
    Environment("SPARTAN_RENDER_RES", nullptr);

    // The preset sizes the menus show: the table the launcher's tests read too.
    FILE* table = std::fopen(SPARTAN_SIZES_TABLE, "r");
    assert(table);
    char line[256];
    int cases = 0;
    while (std::fgets(line, sizeof(line), table)) {
        int preset, ow, oh, rw, rh;
        if (line[0] == '#' || std::sscanf(line, "%d %d %d %d %d", &preset, &ow, &oh, &rw, &rh) != 5) continue;
        const auto [w, h] = UpscaleRenderSize(preset, ow, oh);
        if (w != rw || h != rh) std::printf("preset %d %dx%d: %dx%d, want %dx%d\n", preset, ow, oh, w, h, rw, rh);
        assert(w == rw && h == rh);
        ++cases;
    }
    std::fclose(table);
    assert(cases >= 10);

    // Upscaler and preset rows (Display menu): any preset or on/off change needs the restart that
    // patches the render size, also when the game started without an upscaler.
    s.upscaler = s.startup_upscaler = UpscalerOff;
    s.preset = s.startup_preset = Quality;
    assert(!GraphicsNeedRestart());
    spartanportgpu_native_setting(0xf65); // Upscaler right: Off -> FSR 3.1
    assert(s.upscaler == UpscalerFsr3 && GraphicsNeedRestart());
    spartanportgpu_native_setting(0xf63); // left: back to Off
    assert(s.upscaler == UpscalerOff && !GraphicsNeedRestart());
    spartanportgpu_native_setting(0xf66); // Preset right: Quality -> Balanced (launcher order)
    assert(s.preset == Balanced && GraphicsNeedRestart());
    spartanportgpu_native_setting(0xf64); // left: back to Quality
    spartanportgpu_native_setting(0xf64); // left: Quality -> Ultra Quality
    assert(s.preset == UltraQuality);
    s.render_resolution = 0; // native: 1920x1080 output
    const char* label = reinterpret_cast<const char*>(spartanportgpu_native_setting(0xf62));
    // The name only: the native row has no room for the size (the Insert menu shows it).
    assert(std::strstr(label, "Ultra Quality") && !std::strchr(label, '('));
    label = reinterpret_cast<const char*>(spartanportgpu_native_setting(0xf68));
    assert(std::strstr(label, "[ArrowSelectionL]"));
    s.preset = s.startup_preset;
    // The open FSR 4 follows FSR 3.1 on every system.
    s.upscaler = UpscalerFsr3;
    spartanportgpu_native_setting(0xf65);
    assert(s.upscaler == UpscalerFsr4);
#ifdef _WIN32
    // TAA follows FSR 4.1; unsupported FSR 4.1 is labelled in both languages.
    s.upscaler = UpscalerFsr411;
    spartanportgpu_native_setting(0xf65);
    assert(s.upscaler == UpscalerTaa);
    s.upscaler = UpscalerFsr411;
    s.fsr411_supported = false;
    label = reinterpret_cast<const char*>(spartanportgpu_native_setting(0xf61));
    assert(std::strstr(label, "FSR 4.1") && std::strstr(label, "unavailable"));
    // Portuguese: the launcher's preset names and the accent.
    Environment("SPARTAN_LANGUAGE", "pt");
    label = reinterpret_cast<const char*>(spartanportgpu_native_setting(0xf61));
    assert(std::strstr(label, "(indisponível)"));
    s.preset = Quality;
    label = reinterpret_cast<const char*>(spartanportgpu_native_setting(0xf62));
    assert(std::strstr(label, "Qualidade") && !std::strstr(label, "Quality"));
    s.preset = UltraPerformance;
    label = reinterpret_cast<const char*>(spartanportgpu_native_setting(0xf62));
    assert(std::strstr(label, "Desempenho máximo"));
    Environment("SPARTAN_LANGUAGE", nullptr);
    spartanportgpu_native_setting(0xf65);
    assert(s.upscaler == UpscalerTaa);
#endif
    s.upscaler = s.startup_upscaler;

    // Frame generation: saved, read back, invalid values off; AMD FSR switches live.
    s.frame_generation = 1;
    s.frame_generation_frames = 1;
    Save();
    s.frame_generation = 0;
    Load();
    assert(s.frame_generation == 1 && s.frame_generation_frames == 1);
    assert(!GraphicsNeedRestart());
    for (const char* bad : {"frame_generation=fast\n", "frame_generation=fsr\nframe_generation_frames=2\n",
                            "frame_generation=fsr\nframe_generation_frames=3\n",
                            "frame_generation=fsr\nframe_generation_frames=9\n"}) {
        FILE* config = std::fopen(path, "w");
        assert(config);
        std::fputs(bad, config);
        std::fclose(config);
        s.frame_generation = 1;
        s.frame_generation_frames = 1;
        Load();
        assert(s.frame_generation_frames == 1);
    }
    assert(s.frame_generation == 1 && s.frame_generation_frames == 1);
    FILE* config = std::fopen(path, "w");
    assert(config);
    std::fputs("frame_generation=fast\n", config);
    std::fclose(config);
    Load();
    assert(s.frame_generation == 0);
    s.frame_generation = 1; // FSR switches live
    assert(!GraphicsNeedRestart());
    // Display menu's Frame Generation row cycles between Off and AMD FSR 2x.
    s.frame_generation = 0;
    s.frame_generation_frames = 1;
    const auto fg_label = [](unsigned function) {
        return std::string(reinterpret_cast<const char*>(spartanportgpu_native_setting(function)));
    };
    assert(fg_label(0xf69).find("Off") != std::string::npos);
    assert(fg_label(0xf6b).find("AMD FSR 2x") != std::string::npos && s.frame_generation == 1);
    assert(fg_label(0xf6c).find("[ArrowSelectionL]") == 0);
    assert(fg_label(0xf6b).find("Off") != std::string::npos && s.frame_generation == 0);
    assert(fg_label(0xf6a).find("AMD FSR 2x") != std::string::npos && s.frame_generation == 1);
    s.frame_generation = 0;
    std::remove(path);
    std::puts("Upscaler support: saved choices and FSR 3.1 fallback PASS");
}
