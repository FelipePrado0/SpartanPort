//! What the Controls page's live view knows about a controller: its family, the SDL gamepad names
//! the preview reports, and the dead zone a stick at rest asks for.

/// The controllers the page draws.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PadModel {
    DualSense,
    DualShock4,
    Xbox,
    SwitchPro,
}

impl PadModel {
    pub const ALL: [PadModel; 4] = [
        PadModel::DualSense,
        PadModel::DualShock4,
        PadModel::Xbox,
        PadModel::SwitchPro,
    ];

    /// The value of `pad_style`, which names the buttons.
    pub fn style(self) -> &'static str {
        match self {
            PadModel::DualSense | PadModel::DualShock4 => "playstation",
            PadModel::Xbox => "xbox",
            PadModel::SwitchPro => "nintendo",
        }
    }

    /// The value of `pad_art`.
    pub fn value(self) -> &'static str {
        match self {
            PadModel::DualSense => "dualsense",
            PadModel::DualShock4 => "dualshock4",
            PadModel::Xbox => "xbox",
            PadModel::SwitchPro => "switch_pro",
        }
    }

    /// The name on the page; product names are not translated.
    pub fn name(self) -> &'static str {
        match self {
            PadModel::DualSense => "DualSense",
            PadModel::DualShock4 => "DualShock 4",
            PadModel::Xbox => "Xbox",
            PadModel::SwitchPro => "Switch Pro",
        }
    }

    /// The saved picture, else the first one that fits the saved `style`.
    pub fn saved(value: &str, style: &str) -> PadModel {
        PadModel::ALL
            .into_iter()
            .find(|model| model.value() == value)
            .or_else(|| {
                PadModel::ALL
                    .into_iter()
                    .find(|model| model.style() == style)
            })
            .unwrap_or(PadModel::DualSense)
    }
}

const SONY: &str = "054c";
const MICROSOFT: &str = "045e";
const NINTENDO: &str = "057e";

/// Sony's USB products of the DualShock 4: both revisions and the wireless adapter.
const DUALSHOCK4: [&str; 3] = ["05c4", "09cc", "0ba0"];

/// USB ids in SDL's GUID, low byte first: the vendor at bytes 4 and 5, the product at 8 and 9.
fn usb_id(guid: &str, at: usize) -> Option<String> {
    let low = guid.get(at..at + 2)?;
    let high = guid.get(at + 2..at + 4)?;
    Some(format!("{high}{low}"))
}

/// The model of a controller: its USB vendor and product in SDL's GUID, else words of its name.
/// `None` for a generic pad, which keeps the player's choice.
pub fn detect_model(guid: &str, name: &str) -> Option<PadModel> {
    let product = usb_id(guid, 16);
    match usb_id(guid, 8).as_deref() {
        Some(SONY) if product.is_some_and(|id| DUALSHOCK4.contains(&id.as_str())) => {
            return Some(PadModel::DualShock4)
        }
        Some(SONY) => return Some(PadModel::DualSense),
        Some(MICROSOFT) => return Some(PadModel::Xbox),
        Some(NINTENDO) => return Some(PadModel::SwitchPro),
        _ => {}
    }
    let name = name.to_lowercase();
    let has = |words: &[&str]| words.iter().any(|word| name.contains(word));
    if has(&["dualshock", "ps4"]) {
        Some(PadModel::DualShock4)
    } else if has(&["dualsense", "playstation", "ps5"]) {
        Some(PadModel::DualSense)
    } else if has(&["xbox", "xinput", "x-box"]) {
        Some(PadModel::Xbox)
    } else if has(&["nintendo", "switch", "joy-con", "pro controller"]) {
        Some(PadModel::SwitchPro)
    } else {
        None
    }
}

/// The SDL gamepad name of a preview button (`PREVIEW_BUTTONS` and its extras).
pub fn sdl_button(preview: &str) -> Option<&'static str> {
    Some(match preview {
        "Cross / A" => "a",
        "Circle / B" => "b",
        "Square / X" => "x",
        "Triangle / Y" => "y",
        "Select / View / Share" => "back",
        "Guide" => "guide",
        "Start / Options" => "start",
        "L3" => "leftstick",
        "R3" => "rightstick",
        "L1 / LB" => "leftshoulder",
        "R1 / RB" => "rightshoulder",
        "D-pad up" => "dpup",
        "D-pad down" => "dpdown",
        "D-pad left" => "dpleft",
        "D-pad right" => "dpright",
        "Mute button" => "misc1",
        "Touchpad click" => "touchpad",
        _ => return None,
    })
}

/// How far a stick is from its centre, in percent, from SDL's two axes.
pub fn stick_percent(x: i16, y: i16) -> f32 {
    let (x, y) = (f32::from(x) / 32767.0, f32::from(y) / 32767.0);
    ((x * x + y * y).sqrt() * 100.0).min(100.0)
}

/// The most a dead zone can be, in percent; more would swallow the stick's useful travel.
pub const MAX_DEADZONE: u32 = 50;
/// What the suggestion adds to the drift measured, so the stick stays quiet as it wears.
const DRIFT_MARGIN: f32 = 2.0;

/// The dead zone for a stick that wandered `drift` percent at rest: 0 for a stick that stays
/// within the noise every pad has, else the drift and a margin, rounded up.
pub fn suggest_deadzone(drift: f32) -> u32 {
    if drift < 1.0 {
        return 0;
    }
    ((drift + DRIFT_MARGIN).ceil() as u32).min(MAX_DEADZONE)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_usb_ids_in_the_guid_name_the_model() {
        let sony = |product: &str| format!("030000004c050000{product}000000000000");
        assert_eq!(
            detect_model(&sony("e60c"), "Wireless Controller"),
            Some(PadModel::DualSense)
        );
        assert_eq!(
            detect_model(&sony("c405"), "Wireless Controller"),
            Some(PadModel::DualShock4)
        );
        assert_eq!(
            detect_model(&sony("cc09"), "Wireless Controller"),
            Some(PadModel::DualShock4)
        );
        assert_eq!(
            detect_model("030000005e040000ea02000000000000", "Controller"),
            Some(PadModel::Xbox)
        );
        assert_eq!(
            detect_model("030000007e0500000920000000000000", "Controller"),
            Some(PadModel::SwitchPro)
        );
    }

    #[test]
    fn a_generic_vendor_falls_back_on_the_name() {
        let guid = "03000000ad1b00000000000000000000";
        assert_eq!(
            detect_model(guid, "8BitDo Pro Controller"),
            Some(PadModel::SwitchPro)
        );
        assert_eq!(
            detect_model(guid, "XInput Controller #1"),
            Some(PadModel::Xbox)
        );
        assert_eq!(
            detect_model(guid, "DUALSHOCK 4"),
            Some(PadModel::DualShock4)
        );
        assert_eq!(detect_model(guid, "USB pad"), None);
        assert_eq!(detect_model("", "USB pad"), None);
    }

    #[test]
    fn a_saved_picture_comes_back() {
        for model in PadModel::ALL {
            assert_eq!(PadModel::saved(model.value(), model.style()), model);
        }
        assert_eq!(PadModel::saved("", "xbox"), PadModel::Xbox);
        assert_eq!(PadModel::saved("", "nonsense"), PadModel::DualSense);
    }

    #[test]
    fn every_preview_button_has_an_sdl_name() {
        for name in crate::core::joysticks::PREVIEW_BUTTONS {
            assert!(sdl_button(name).is_some(), "{name}");
        }
    }

    #[test]
    fn a_stick_reads_as_a_percentage_of_its_travel() {
        assert_eq!(stick_percent(0, 0), 0.0);
        assert_eq!(stick_percent(32767, 0), 100.0);
        assert_eq!(stick_percent(-32768, -32768), 100.0);
    }

    #[test]
    fn a_quiet_stick_needs_no_dead_zone() {
        assert_eq!(suggest_deadzone(0.5), 0);
        assert_eq!(suggest_deadzone(4.2), 7);
        assert_eq!(suggest_deadzone(80.0), MAX_DEADZONE);
    }
}
