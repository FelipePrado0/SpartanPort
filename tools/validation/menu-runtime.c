#include "../../src/runtime_services.c"

int spartan_validation_guest_enter(void) {
    int32_t value = -1;
    return system_param(1000, &value) == 0 ? value : -1;
}
