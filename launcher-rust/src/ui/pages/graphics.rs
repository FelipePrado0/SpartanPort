//! The Graphics page: window, upscaling, effects, textures, overlay and screenshots.
//!
//! Upscaling shows the switch, the upscaler and its preset; the upscaler outputs the Render
//! resolution of the Performance page. `output_res`, `live_resolution` and `object_motion` stay
//! parked.

use iced::widget::{button, column, text};
use iced::Element;

use crate::core::i18n::tr;
use crate::core::options::{ANISOTROPY, DISPLAY_MODES, FRAME_GENERATION, PRESETS, UPSCALERS};
use crate::ui::app::{Message, State};
use crate::ui::theme;
use crate::ui::widgets::{checkbox, choice_pairs, row, section, select, select_pairs};

pub fn view(state: &State) -> Element<'_, Message> {
    let lang = state.lang.as_str();
    let hint = |english: &str| tr(lang, english);
    let ini = |key: &str| state.store.text(key);
    // Each preset with the size the game renders at for the chosen Render resolution (Performance
    // page), as the game's menus show it: "Quality (1706x960)".
    let output =
        crate::core::options::resolution_size(&ini("render_resolution")).unwrap_or((1920, 1080));
    let preset_pairs: Vec<(String, String)> = choice_pairs(PRESETS, lang)
        .into_iter()
        .map(|(value, label)| {
            let name = label
                .rsplit_once(" (")
                .map_or(label.as_str(), |(name, _)| name)
                .to_string();
            match crate::core::graphics_options::upscale_render_size(&value, output) {
                Some((w, h)) => (value, format!("{name} ({w}x{h})")),
                None => (value, label),
            }
        })
        .collect();

    // The list the probe made, or just the primary default while it has not answered.
    let pairs: Vec<(String, String)> = if state.monitor_pairs.is_empty() {
        crate::core::gpu::monitor_options(&[])
    } else {
        state.monitor_pairs.clone()
    }
    .into_iter()
    .map(|(value, label)| {
        let label = if value.is_empty() {
            tr(lang, &label)
        } else {
            label
        };
        (value, label)
    })
    .collect();
    let monitor = state.store.text("monitor");

    // The Python marks a row that needs a new start of the game with a small `restart` note.
    let anisotropy_label = format!(
        "{}  ·  {}",
        tr(lang, "Anisotropic filtering"),
        tr(lang, "restart")
    );

    let shots = button(text(tr(lang, "Open the screenshots folder")))
        .style(theme::button_flat)
        .on_press(Message::OpenFolder(
            state
                .paths
                .user_dir(&state.store.text("user_dir"))
                .join("screenshots"),
        ));

    column![
        section(state.rtl, &tr(lang, "Window")),
        row(
            state.rtl,
            &tr(lang, "Display mode"),
            select("display_mode", DISPLAY_MODES, &ini("display_mode"), lang),
            Some(&hint(
                "Borderless covers the screen without changing the display mode."
            )),
        ),
        row(
            state.rtl,
            &tr(lang, "Monitor"),
            select_pairs("monitor", &pairs, &monitor),
            Some(&hint(
                "The screen the game opens its window on. Primary is the monitor the system considers the main one."
            )),
        ),
        checkbox(
            state.rtl,
            "vsync",
            &tr(lang, "VSync"),
            state.store.flag("vsync"),
            Some(&hint("Off allows tearing for the lowest latency.")),
        ),
        checkbox(
            state.rtl,
            "hdr",
            &tr(lang, "Allow HDR output"),
            state.store.flag("hdr"),
            Some(&hint("When HDR is on in Windows and the display supports it.")),
        ),
        section(state.rtl, &tr(lang, "Upscaling")),
        row(
            state.rtl,
            &tr(lang, "Upscaler"),
            select("upscaler", UPSCALERS, &ini("upscaler"), lang),
            Some(&hint("FSR 4.1 requires AMD Adrenalin 26.10.1 or newer. The log shows the active provider; fallback to FSR 3.1 is not FSR 4.1.")),
        ),
        row(
            state.rtl,
            &format!("{}  ·  {}", tr(lang, "Quality preset"), tr(lang, "restart")),
            select_pairs("preset", &preset_pairs, &ini("preset")),
            Some(&hint(
                "Share of the Render resolution (Performance page) the game renders at; the upscaler rebuilds the full Render resolution. Lower renders faster. Native AA keeps the full resolution."
            )),
        ),
        row(
            state.rtl,
            &tr(lang, "Frame generation"),
            select("frame_generation", FRAME_GENERATION, &ini("frame_generation"), lang),
            Some(&hint(
                "A generated frame between game frames: up to twice the frames on screen. Needs an upscaler. AMD FSR: any graphics card."
            )),
        ),
        section(state.rtl, &tr(lang, "Effects")),
        checkbox(
            state.rtl,
            "motion_blur",
            &tr(lang, "Motion blur"),
            state.store.flag("motion_blur"),
            Some(&hint(
                "The game's own motion blur on fast moving objects and the camera. Off removes it."
            )),
        ),
        checkbox(
            state.rtl,
            "depth_of_field",
            &tr(lang, "Depth of field"),
            state.store.flag("depth_of_field"),
            Some(&hint(
                "The game's own blur of distant objects, strongest in cutscenes. Off keeps the whole picture sharp."
            )),
        ),
        section(state.rtl, &tr(lang, "Textures")),
        row(
            state.rtl,
            &anisotropy_label,
            select("anisotropy", ANISOTROPY, &ini("anisotropy"), lang),
            Some(&hint(
                "Sharper floors and walls seen at an angle, at little cost on most graphics cards."
            )),
        ),
        section(state.rtl, &tr(lang, "Overlay")),
        checkbox(
            state.rtl,
            "show_fps",
            &tr(lang, "Show performance overlay"),
            state.store.flag("show_fps"),
            Some(&hint(
                "Items, size and corner: in-game menu (Insert / R3+L2) > Overlay."
            )),
        ),
        section(state.rtl, &tr(lang, "Screenshots")),
        row(
            state.rtl,
            &tr(lang, "F12 in the game"),
            shots.into(),
            Some(&hint(
                "Saves the game's image, without the overlay, as a PNG in the screenshots folder of the saves."
            )),
        ),
    ]
    .spacing(2)
    .into()
}
