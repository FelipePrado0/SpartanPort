#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "../../gpu/shim/spartanport_settings.h"
#include "../../gpu/spartanportgpu.h"

extern "C" int spartan_validation_guest_enter(void);

static void Environment(const char* key, const char* value) {
#ifdef _WIN32
    assert(_putenv_s(key, value) == 0);
#else
    assert((value[0] ? setenv(key, value, 1) : unsetenv(key)) == 0);
#endif
}

static int Check(const char* scenario, const char* stage) {
    const int host_swapped = spartanportgpu_menu_confirm_swap();
    const int guest_enter = spartan_validation_guest_enter();
    const int expected_enter = host_swapped ? 0 : 1;
    std::printf("%s %s: host_swapped=%d guest_enter=%d expected_enter=%d %s\n",
                scenario, stage, host_swapped, guest_enter, expected_enter,
                guest_enter == expected_enter ? "PASS" : "FAIL");
    return guest_enter != expected_enter;
}

int main(int argc, char** argv) {
    assert(argc == 2);
    Environment("SPARTAN_CONFIG", argv[1]);
    int failures = 0;
    struct Scenario { const char* name; const char* primary; const char* alias; int initial; };
    for (const auto& scenario : {
             Scenario{"no override", "", "", 0},
             Scenario{"primary Cross", "0", "", 0},
             Scenario{"primary Circle", "1", "", 1},
             Scenario{"alias Circle", "", "circle", 1},
         }) {
        Environment("SPARTAN_MENU_CONFIRM_SWAP", scenario.primary);
        Environment("SPARTAN_MENU_CONFIRM", scenario.alias);
        SpartanSettings::Load();
        assert(spartanportgpu_menu_confirm_swap() == scenario.initial);
        failures += Check(scenario.name, "startup");
        // The production settings menu writes this same live atomic.
        SpartanSettings::Get().menu_confirm_swap = 1 - scenario.initial;
        failures += Check(scenario.name, "after live change");
    }
    std::printf("Live menu/runtime coherence failures: %d\n", failures);
    return failures ? 1 : 0;
}
