# First Light GPU isolation

The fork inherits a large shadPS4-derived Vulkan renderer from `gow3_pc`. Much of that core is
generic, but several features are explicitly game-specific.

## Disabled for CUSA00575 bring-up

The First Light branch prevents installation of:

- God of War III orb multiplier hooks;
- GOW3 cheat hooks;
- GOW3 actor-health hooks;
- GOW3 damage/stat multiplier hooks.

The launcher also starts with the GOW3-specific MSAA workaround and temporal/upscaler helpers off.

## Game profile

`gpu/games/firstlight.cpp` recognizes `CUSA00575`, but intentionally contains:

- no guest hooks;
- no dynamic-writer address ranges;
- no known buffer-copy shader hash.

This follows the game-profile architecture used by `bloodborne_pc`: the generic renderer should
remain generic, while proven game-specific knowledge lives in a profile.

## Headless import registration

`libSceGnmDriver` and `libSceVideoOut` can now register their NIDs without constructing the
Liverpool command processor, VideoOut driver, SDL window or Vulkan Presenter. This makes
`--cpu-only --check-imports` a real static/runtime import check.

## First GPU milestone

1. zero missing imports;
2. entry point reached;
3. first GNM command stream observed;
4. Vulkan presenter opens;
5. first clear frame.

Do not add First-Light-specific GPU workarounds before a reproducible trace shows why they are
needed.
