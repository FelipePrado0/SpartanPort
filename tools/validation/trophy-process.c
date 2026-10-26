#define main service_contract_main
#include "../../tests/test_services.c"
#undef main

int main(int argc, char **argv) {
    assert(argc == 4);
    snprintf(mock_user_dir, sizeof(mock_user_dir), "%s", argv[2]);
    assert(!strcmp(trophy_dir(), argv[3]));
    trophy_load_once();
    assert(!strcmp(trophy_names[1], "Releasing the Floodgates"));
    if (!strcmp(argv[1], "write")) {
        assert(!(trophy_flags[0] & (1u << 1)));
        assert(trophy_unlock(1, 1, 1, NULL) == 0);
        assert(!strcmp(last_toast_title, "Releasing the Floodgates"));
        assert(!strcmp(last_toast_detail, "Kill Poseidon"));
    } else {
        assert(!strcmp(argv[1], "read"));
        assert(trophy_flags[0] & (1u << 1));
        assert(trophy_unlock(1, 1, 1, NULL) == TROPHY_ALREADY_UNLOCKED);
    }
    return 0;
}
