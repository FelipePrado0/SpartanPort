// SPDX-License-Identifier: GPL-2.0-or-later
// SpartanPort: trophy unlock notifications in the themes Card, Card with a cinematic platinum and
// Corner popup (the toast drawn over the game), their timeline, sounds and the player's own
// pictures and animations. Real unlocks and the menu's previews go through the same queue and
// renderer. The theme Original is the overlay's own notice (spartanport_overlay.cpp), which only
// takes the sound from here. Everything the player can change is in <user>/custom_trophy
// (docs/TROPHIES.md, Notifications); without any file the Original notice shows and no sound plays.

#pragma once

#include <array>
#include <string>
#include "imgui.h"
#include "spartanport_trophy_toast_model.h"

namespace SpartanToast {

Options GetOptions();
/// Applies at once (volume) or from the next toast; saved on the worker thread.
void SetOptions(const Options& options);

/// What a toast shows of a trophy.
struct Entry {
    std::string name, detail;
    Grade grade = Grade::Unknown;
};
/// The trophy's name, description and grade (any thread).
using TrophyLookup = Entry (*)(int id);
/// The trophy's artwork as an ImGui texture (the trophy list's); empty when there is none.
using IconSource = ImTextureRef (*)(int id);

/// Once, with the ImGui context: reads the options, loads the fonts, pictures and animations,
/// then decodes the sounds and opens the audio stream on the worker thread (never during a toast).
void Start(ImFontAtlas* atlas, IconSource icons, TrophyLookup lookup);
/// The words a toast uses when notifications.ini sets none: the card's second line and the grade
/// names (by Grade; index 0 unused). English until set.
void SetDefaultTexts(const std::string& unlocked, const std::array<std::string, 5>& grade_names);
/// Decodes the sound files again (after the player replaced them). Pictures and animations are
/// read when the game starts.
void ReloadSounds();
/// The files in use, for the menu: "Bronze: bronze.mp3  ·  Silver: none ..." and the pictures.
std::string SoundSummary();
std::string PictureSummary();

/// Any thread. preview: shown and heard like an unlock, nothing is saved. grade: 1-4 shows the
/// toast as that grade (bronze..platinum) whatever the trophy's own.
void Enqueue(int id, bool preview, int grade = 0);
/// Any thread: the sound of a trophy's grade (or of `grade`, 1-4), for the Original notice.
void PlaySound(int id, int grade = 0);
void Clear();
bool Busy();
/// Present thread, inside the ImGui frame. scale: display height / 1080. top: pixels from the top
/// of the screen the card keeps clear (the FPS counter), 0 for none.
void Draw(float scale, float top = 0.0f);

/// Tests and the offscreen preview: a fixed clock in ms (< 0: steady_clock again).
void SetClockOverride(double ms);
/// Theme forced regardless of the options (offscreen preview), or Count for none.
void SetThemeOverride(Theme theme);
struct DrawStats {
    double last_us, avg_us, max_us;
    unsigned frames;
    unsigned vertices; ///< added to the draw list by the last frame
};
DrawStats Stats();
void ResetStats();

} // namespace SpartanToast
