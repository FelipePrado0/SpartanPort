//! The probes the launcher runs for lists: Vulkan GPUs, SDL displays and the gamepad.

use std::ffi::OsString;
use std::path::{Path, PathBuf};
use std::time::Duration;

use indexmap::IndexMap;

use super::env::Lookup;
use super::paths::Paths;
use super::platform::tool::{inherited_env_with, Stderr, ToolCall, Tools, Window};

/// One line of `spartanport-gpu-capabilities --list`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct GpuRow {
    pub index: String,
    pub name: String,
    pub kind: String,
    /// The game's multisampling default on that driver (`off` or `on`), from newer tools.
    pub msaa: Option<String>,
    /// The GPU the game picks without `SPARTAN_GPU_ID`.
    pub chosen: bool,
}

/// What the Automatic multisampling entry says about the chosen card.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum MsaaDecision {
    Unknown,
    Known { name: String, off: bool },
}

/// One monitor of `spartanport-probe --displays`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Display {
    pub index: String,
    pub name: String,
    pub primary: bool,
    pub x: i64,
    pub y: i64,
    pub width: i64,
    pub height: i64,
}

fn all_ascii_digits(text: &str) -> bool {
    !text.is_empty() && text.bytes().all(|b| b.is_ascii_digit())
}

/// `-?[0-9]+` as an integer; anything else (signs, blanks, underscores) is no number.
fn number(column: &str) -> Option<i64> {
    let digits = column.strip_prefix('-').unwrap_or(column);
    if all_ascii_digits(digits) {
        column.parse().ok()
    } else {
        None
    }
}

/// One row per line of `spartanport-gpu-capabilities --list`: index, name, type and, from the
/// tools that print them, the game's multisampling default and `1` on the game's pick. Other
/// lines are skipped.
pub fn parse_gpu_list(text: &str) -> Vec<GpuRow> {
    text.lines()
        .filter_map(|line| {
            let columns: Vec<&str> = line.split('\t').collect();
            if columns.len() < 3 || !all_ascii_digits(columns[0]) {
                return None;
            }
            Some(GpuRow {
                index: columns[0].to_string(),
                name: columns[1].to_string(),
                kind: columns[2].to_string(),
                msaa: columns.get(3).map(|c| c.to_string()),
                chosen: columns.get(4) == Some(&"1"),
            })
        })
        .collect()
}

/// The row the game will run on: the Graphics card setting when it is in the list, else the GPU
/// the tool marks as the game's pick; `None` when neither is known.
pub fn chosen_gpu<'a>(rows: &'a [GpuRow], gpu_id: &str) -> Option<&'a GpuRow> {
    rows.iter()
        .find(|row| row.index == gpu_id)
        .or_else(|| rows.iter().find(|row| row.chosen))
}

/// The game's multisampling decision for the chosen card, `Unknown` unless the tool said `off` or `on`.
pub fn no_msaa_decision(rows: &[GpuRow], gpu_id: &str) -> MsaaDecision {
    match chosen_gpu(rows, gpu_id) {
        Some(row) => match row.msaa.as_deref() {
            Some(value @ ("off" | "on")) => MsaaDecision::Known {
                name: row.name.clone(),
                off: value == "off",
            },
            _ => MsaaDecision::Unknown,
        },
        None => MsaaDecision::Unknown,
    }
}

/// Runs `program` for at most `limit`; `None` on a spawn error, a timeout or, when `must_succeed`,
/// a non-zero exit. The output is read lossily as UTF-8.
fn run(
    tools: &dyn Tools,
    program: PathBuf,
    args: &[&str],
    env: Option<&IndexMap<String, String>>,
    limit: Duration,
    must_succeed: bool,
) -> Option<String> {
    let out = tools
        .run(&ToolCall {
            program: program.into(),
            args: args.iter().map(OsString::from).collect(),
            env,
            cwd: None,
            limit: Some(limit),
            stderr: Stderr::Null,
            window: Window::Hidden,
        })
        .ok()?;
    if must_succeed && out.code != Some(0) {
        return None;
    }
    Some(out.stdout)
}

/// The Vulkan GPUs in the order the game numbers them, or empty when the tool cannot tell.
pub fn list_gpus(tools: &dyn Tools, paths: &Paths) -> Vec<GpuRow> {
    let Some(tool) = paths.find_tool("spartanport-gpu-capabilities") else {
        return Vec::new();
    };
    run(tools, tool, &["--list"], None, Duration::from_secs(5), true)
        .map(|out| parse_gpu_list(&out))
        .unwrap_or_default()
}

/// One monitor per line of `spartanport-probe --displays`: index, name, primary, x, y, width,
/// height. A line is kept only with exactly seven well-formed columns, so a monitor the tool could
/// not describe leaves a gap in the indices instead of a wrong entry. Only LF (or CRLF) ends a
/// row; other Unicode separators stay inside the name.
pub fn parse_display_list(text: &str) -> Vec<Display> {
    text.split('\n')
        .filter_map(|raw| {
            let line = raw.strip_suffix('\r').unwrap_or(raw);
            let columns: Vec<&str> = line.split('\t').collect();
            if columns.len() != 7 || !all_ascii_digits(columns[0]) {
                return None;
            }
            let primary = match columns[2] {
                "0" => false,
                "1" => true,
                _ => return None,
            };
            let x = number(columns[3])?;
            let y = number(columns[4])?;
            let width = number(columns[5])?;
            let height = number(columns[6])?;
            if width <= 0 || height <= 0 {
                return None;
            }
            Some(Display {
                index: columns[0].to_string(),
                name: columns[1].to_string(),
                primary,
                x,
                y,
                width,
                height,
            })
        })
        .collect()
}

/// What the Monitor list shows: the name and the resolution, the position when the monitor is not
/// at the origin, and a `(primary)` suffix on the primary monitor.
pub fn display_label(display: &Display) -> String {
    let mut parts = Vec::new();
    if !display.name.is_empty() {
        parts.push(display.name.clone());
    }
    parts.push(format!("{} × {}", display.width, display.height));
    if (display.x, display.y) != (0, 0) {
        parts.push(format!("({}, {})", display.x, display.y));
    }
    if display.primary {
        parts.push("(primary)".to_string());
    }
    parts.join(" · ")
}

/// The Monitor list: the primary default first, then every monitor keyed by its SDL index. A gap
/// the probe left stays a gap, never renumbered.
pub fn monitor_options(displays: &[Display]) -> Vec<(String, String)> {
    std::iter::once((String::new(), "Primary (default)".to_string()))
        .chain(
            displays
                .iter()
                .map(|display| (display.index.clone(), display_label(display))),
        )
        .collect()
}

/// The value the Monitor list takes: `saved` while it is still one of the options, the primary
/// default otherwise. The index is followed, never the position it used to sit at.
pub fn monitor_selection(displays: &[Display], saved: &str) -> String {
    if saved.is_empty() || displays.iter().any(|display| display.index == saved) {
        saved.to_string()
    } else {
        String::new()
    }
}

/// The environment to run `spartanport-probe` with, or `None` when it cannot start safely. As
/// `run.py`: a development build loads the MSYS2 CLANG64 DLLs (SDL3, FFmpeg); without them Windows
/// would show a missing-DLL dialog, so the probe is not run at all.
pub fn probe_env(probe: &Path, env: Lookup) -> Option<Option<IndexMap<String, String>>> {
    if !cfg!(windows) || probe.parent()?.join("SDL3.dll").is_file() {
        return Some(None);
    }
    let root = env("MSYS2_ROOT").unwrap_or_else(|| r"C:\msys64".to_string());
    let clang64 = std::path::Path::new(&root).join("clang64").join("bin");
    if !clang64.join("SDL3.dll").is_file() {
        return None;
    }
    let mut dirs = vec![clang64];
    if let Some(path) = env("PATH") {
        dirs.extend(std::env::split_paths(&path));
    }
    let path = std::env::join_paths(dirs).ok()?.into_string().ok()?;
    Some(Some(inherited_env_with(&[("PATH", path)])))
}

/// The monitors the game can open on, in the order `spartanport-probe --displays` numbers them, or
/// empty when the tool cannot tell. On Linux the probe runs under the SDL video driver the game will
/// use (`auto` lets SDL choose).
pub fn list_displays(tools: &dyn Tools, paths: &Paths, backend: &str, env: Lookup) -> Vec<Display> {
    let Some(tool) = paths.find_tool("spartanport-probe") else {
        return Vec::new();
    };
    let Some(probe_env) = probe_env(&tool, env) else {
        return Vec::new();
    };
    let mut run_env = probe_env;
    if cfg!(not(windows)) && matches!(backend, "wayland" | "x11") {
        let mut vars = run_env.unwrap_or_default();
        vars.insert("SDL_VIDEO_DRIVER".to_string(), backend.to_string());
        vars.insert("SDL_VIDEODRIVER".to_string(), backend.to_string());
        run_env = Some(vars);
    }
    run(
        tools,
        tool,
        &["--displays"],
        run_env.as_ref(),
        Duration::from_secs(5),
        true,
    )
    .map(|out| parse_display_list(&out))
    .unwrap_or_default()
}

/// Whether `spartanport-probe` sees a gamepad now; `None` when the probe is missing or fails.
pub fn connected_gamepad(tools: &dyn Tools, paths: &Paths, env: Lookup) -> Option<bool> {
    let probe = paths.find_tool("spartanport-probe")?;
    let probe_env = probe_env(&probe, env)?;
    let out = run(
        tools,
        probe,
        &["--gamepad"],
        probe_env.as_ref(),
        Duration::from_secs(15),
        false,
    )?;
    Some(out.contains("Gamepad: type="))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::core::platform::tool::FakeTools;

    const TWO_DISPLAYS: &str =
        "0\tMonitor A\t1\t0\t0\t1920\t1080\n1\tMonitor B\t0\t1920\t0\t2560\t1440\n";

    fn display(index: &str, name: &str, primary: bool, x: i64, y: i64, w: i64, h: i64) -> Display {
        Display {
            index: index.into(),
            name: name.into(),
            primary,
            x,
            y,
            width: w,
            height: h,
        }
    }

    fn gpu(index: &str, name: &str, kind: &str, msaa: Option<&str>, chosen: bool) -> GpuRow {
        GpuRow {
            index: index.into(),
            name: name.into(),
            kind: kind.into(),
            msaa: msaa.map(String::from),
            chosen,
        }
    }

    fn labels(options: &[(String, String)]) -> Vec<&str> {
        options.iter().map(|(value, _)| value.as_str()).collect()
    }

    #[test]
    fn parse_gpu_list_reads_rows() {
        let text = "0\tAMD Radeon(TM) Graphics\tintegrated\nbroken line\n1\tNVIDIA GeForce RTX 2070 SUPER\tdiscrete\n";
        assert_eq!(
            parse_gpu_list(text),
            vec![
                gpu("0", "AMD Radeon(TM) Graphics", "integrated", None, false),
                gpu(
                    "1",
                    "NVIDIA GeForce RTX 2070 SUPER",
                    "discrete",
                    None,
                    false
                ),
            ]
        );
        let text =
            "0\tIntel(R) Arc(TM) A750 Graphics\tdiscrete\toff\t1\n1\tllvmpipe\tother\ton\t0\n";
        assert_eq!(
            parse_gpu_list(text),
            vec![
                gpu(
                    "0",
                    "Intel(R) Arc(TM) A750 Graphics",
                    "discrete",
                    Some("off"),
                    true
                ),
                gpu("1", "llvmpipe", "other", Some("on"), false),
            ]
        );
        assert_eq!(parse_gpu_list(""), vec![]);
    }

    #[test]
    fn chosen_gpu_follows_the_setting_then_the_tools_pick() {
        let rows = parse_gpu_list("0\tA\tdiscrete\toff\t0\n1\tB\tother\ton\t1\n");
        assert_eq!(chosen_gpu(&rows, "0").map(|r| r.name.as_str()), Some("A"));
        assert_eq!(chosen_gpu(&rows, "").map(|r| r.name.as_str()), Some("B"));
        assert_eq!(chosen_gpu(&rows, "9").map(|r| r.name.as_str()), Some("B"));
        let plain = parse_gpu_list("0\tA\tdiscrete\n");
        assert_eq!(chosen_gpu(&plain, ""), None);
    }

    #[test]
    fn no_msaa_decision_names_the_chosen_card() {
        let rows = parse_gpu_list("0\tA\tdiscrete\toff\t1\n1\tB\tother\ton\t0\n2\tC\tother\n");
        assert_eq!(
            no_msaa_decision(&rows, ""),
            MsaaDecision::Known {
                name: "A".into(),
                off: true
            }
        );
        assert_eq!(
            no_msaa_decision(&rows, "1"),
            MsaaDecision::Known {
                name: "B".into(),
                off: false
            }
        );
        assert_eq!(no_msaa_decision(&rows, "2"), MsaaDecision::Unknown);
        assert_eq!(no_msaa_decision(&[], ""), MsaaDecision::Unknown);
        let odd = parse_gpu_list("0\tA\tdiscrete\tmaybe\t1\n");
        assert_eq!(no_msaa_decision(&odd, ""), MsaaDecision::Unknown);
    }

    #[test]
    fn seven_columns_are_read() {
        assert_eq!(
            parse_display_list(TWO_DISPLAYS),
            vec![
                display("0", "Monitor A", true, 0, 0, 1920, 1080),
                display("1", "Monitor B", false, 1920, 0, 2560, 1440),
            ]
        );
    }

    #[test]
    fn names_keep_spaces_quotes_and_utf8() {
        let name = "Dell U2720Q «à esquerda» , 4K";
        let rows = parse_display_list(&format!("3\t{name}\t0\t-1920\t0\t3840\t2160\n"));
        assert_eq!(rows, vec![display("3", name, false, -1920, 0, 3840, 2160)]);
    }

    #[test]
    fn a_name_with_a_straight_quote_is_preserved() {
        let name = "Monitor \"Pro\"";
        let rows = parse_display_list(&format!("3\t{name}\t0\t-1920\t0\t3840\t2160\n"));
        assert_eq!(rows, vec![display("3", name, false, -1920, 0, 3840, 2160)]);
    }

    #[test]
    fn a_gap_and_an_empty_name_are_kept() {
        let rows = parse_display_list("0\tA\t1\t0\t0\t1920\t1080\n2\t\t0\t0\t0\t1280\t720\n");
        assert_eq!(
            rows,
            vec![
                display("0", "A", true, 0, 0, 1920, 1080),
                display("2", "", false, 0, 0, 1280, 720),
            ]
        );
    }

    #[test]
    fn a_list_without_a_primary_is_not_a_failure() {
        let rows = parse_display_list("0\tA\t0\t0\t0\t1920\t1080\n1\tB\t0\t1920\t0\t1920\t1080\n");
        assert_eq!(
            rows.iter().map(|r| r.primary).collect::<Vec<_>>(),
            [false, false]
        );
    }

    #[test]
    fn malformed_lines_are_skipped() {
        let text = "\n\
                    0\tA\t1\t0\t0\t1920\n\
                    0\tA\t1\t0\t0\t1920\t1080\tx\n\
                    x\tA\t1\t0\t0\t1920\t1080\n\
                    -1\tA\t1\t0\t0\t1920\t1080\n\
                    \u{b2}\tA\t1\t0\t0\t1920\t1080\n\
                    0\tA\t2\t0\t0\t1920\t1080\n\
                    0\tA\t1\t0\t0\t1920\t1080\n";
        assert_eq!(
            parse_display_list(text),
            vec![display("0", "A", true, 0, 0, 1920, 1080)]
        );
    }

    #[test]
    fn geometry_must_be_integers_with_positive_sizes() {
        for bad in [
            "1\tA\t1\t0\t0\t0\t1080",
            "1\tA\t1\t0\t0\t1920\t-1",
            "1\tA\t1\t0\t0\twide\t1080",
            "1\tA\t1\t0\t0\t1_0\t1080",
            "1\tA\t1\t0\t0\t 5\t1080",
        ] {
            assert_eq!(parse_display_list(bad), vec![], "{bad:?}");
        }
        assert_eq!(
            parse_display_list("1\tA\t0\t-1920\t-1080\t1920\t1080\n"),
            vec![display("1", "A", false, -1920, -1080, 1920, 1080)]
        );
    }

    #[test]
    fn empty_output_is_no_monitors() {
        assert_eq!(parse_display_list(""), vec![]);
    }

    #[test]
    fn a_record_ends_on_a_line_feed_or_a_crlf() {
        let text = "0\tA\t1\t0\t0\t1920\t1080\r\n1\tB\t0\t1920\t0\t1280\t720\n";
        assert_eq!(
            parse_display_list(text),
            vec![
                display("0", "A", true, 0, 0, 1920, 1080),
                display("1", "B", false, 1920, 0, 1280, 720),
            ]
        );
    }

    #[test]
    fn a_unicode_line_separator_in_a_name_is_not_a_record_boundary() {
        for sep in ['\u{2028}', '\u{2029}', '\u{85}', '\u{b}', '\u{c}'] {
            let name = format!("Panel{sep}A");
            let rows = parse_display_list(&format!("0\t{name}\t1\t0\t0\t1920\t1080\n"));
            assert_eq!(
                rows,
                vec![display("0", &name, true, 0, 0, 1920, 1080)],
                "{sep:?}"
            );
        }
    }

    #[test]
    fn the_default_is_first_and_the_rest_come_by_index() {
        let options = monitor_options(&parse_display_list(TWO_DISPLAYS));
        assert_eq!(labels(&options), ["", "0", "1"]);
        assert_eq!(
            options[0],
            ("".to_string(), "Primary (default)".to_string())
        );
        assert_eq!(options[1].1, "Monitor A · 1920 × 1080 · (primary)");
        assert_eq!(options[2].1, "Monitor B · 2560 × 1440 · (1920, 0)");
    }

    #[test]
    fn a_gap_in_the_indices_is_kept() {
        let options = monitor_options(&parse_display_list(
            "0\tA\t1\t0\t0\t1920\t1080\n2\tB\t0\t0\t0\t1280\t720\n",
        ));
        assert_eq!(labels(&options), ["", "0", "2"]);
    }

    #[test]
    fn no_monitors_leaves_only_the_default() {
        assert_eq!(
            monitor_options(&[]),
            vec![("".to_string(), "Primary (default)".to_string())]
        );
    }

    #[test]
    fn primary_at_nonzero_index() {
        let displays =
            parse_display_list("0\tA\t0\t0\t0\t1920\t1080\n1\tB\t1\t1920\t0\t2560\t1440\n");
        let options = monitor_options(&displays);
        assert_eq!(labels(&options), ["", "0", "1"]);
        assert_eq!(
            options[0],
            ("".to_string(), "Primary (default)".to_string())
        );
        assert_eq!(options[1].1, "A · 1920 × 1080");
        assert_eq!(options[2].1, "B · 2560 × 1440 · (1920, 0) · (primary)");
        assert_eq!(monitor_selection(&displays, "0"), "0");
        assert_eq!(monitor_selection(&displays, "1"), "1");
        assert_eq!(monitor_selection(&displays, ""), "");
        assert_eq!(monitor_selection(&displays, "7"), "");
    }

    #[test]
    fn negative_coordinates() {
        let displays = parse_display_list("0\tLeft\t1\t-1920\t0\t1920\t1080\n");
        assert_eq!(
            displays,
            vec![display("0", "Left", true, -1920, 0, 1920, 1080)]
        );
        assert_eq!(
            display_label(&displays[0]),
            "Left · 1920 × 1080 · (-1920, 0) · (primary)"
        );
    }

    #[test]
    fn a_name_with_a_tab_or_cr_never_panics() {
        // A tab in a name adds a column: the GPU row keeps its first three, the monitor row is dropped.
        let gpus = parse_gpu_list("0\tName\twith tab\tdiscrete\nr\tx\ty\n1\tNa\rme\tdiscrete\r\n");
        assert_eq!(gpus.len(), 2);
        assert_eq!(gpus[0].index, "0");
        assert_eq!(gpus[1], gpu("1", "Na\rme", "discrete", None, false));
        assert_eq!(parse_display_list("0\tA\tB\t1\t0\t0\t1920\t1080\n"), vec![]);
        assert_eq!(
            parse_display_list("0\tA\rB\t1\t0\t0\t1920\t1080\r\n"),
            vec![display("0", "A\rB", true, 0, 0, 1920, 1080)]
        );
        assert_eq!(parse_display_list("\r\n\r\r\n\t\t\t\t\t\t\r"), vec![]);
    }

    #[test]
    fn a_missing_tool_lists_nothing() {
        let dir = tempfile::tempdir().unwrap();
        let paths = Paths::from_env(dir.path(), &|_| None);
        let tools = FakeTools::printing(0, "0\tx\ty\n");
        assert_eq!(list_gpus(&tools, &paths), vec![]);
        assert_eq!(list_displays(&tools, &paths, "auto", &|_| None), vec![]);
        assert_eq!(connected_gamepad(&tools, &paths, &|_| None), None);
        assert!(tools.calls().is_empty());
    }

    /// A port folder with `bin/<name>` for each of `names`.
    fn port_with(names: &[&str]) -> (tempfile::TempDir, Paths) {
        let dir = tempfile::tempdir().unwrap();
        std::fs::create_dir_all(dir.path().join("bin")).unwrap();
        for name in names {
            let file = if cfg!(windows) {
                format!("{name}.exe")
            } else {
                name.to_string()
            };
            std::fs::write(dir.path().join("bin").join(file), b"").unwrap();
        }
        let paths = Paths::from_env(dir.path(), &|_| None);
        (dir, paths)
    }

    #[test]
    fn gpus_come_from_the_tool_rows() {
        let (_dir, paths) = port_with(&["spartanport-gpu-capabilities"]);
        let tools = FakeTools::printing(
            0,
            "0\tAMD\tintegrated\toff\t1\n1\tNVIDIA\tdiscrete\ton\t0\n",
        );
        assert_eq!(
            list_gpus(&tools, &paths),
            vec![
                gpu("0", "AMD", "integrated", Some("off"), true),
                gpu("1", "NVIDIA", "discrete", Some("on"), false),
            ]
        );
        let calls = tools.calls();
        assert_eq!(calls.len(), 1);
        assert_eq!(calls[0].args, ["--list"]);
        assert_eq!(calls[0].limit, Some(Duration::from_secs(5)));
        assert_eq!(calls[0].env, None);
    }

    #[test]
    fn a_failing_gpu_tool_lists_nothing() {
        let (_dir, paths) = port_with(&["spartanport-gpu-capabilities"]);
        let failed = FakeTools::printing(1, "0\tAMD\tintegrated\n");
        assert_eq!(list_gpus(&failed, &paths), vec![]);
        let hung = FakeTools::timing_out();
        assert_eq!(list_gpus(&hung, &paths), vec![]);
    }

    #[test]
    fn displays_run_under_the_chosen_backend() {
        let (dir, paths) = port_with(&["spartanport-probe"]);
        // The probe finds its libraries next to it, so a development build's MSYS2 is not needed.
        std::fs::write(dir.path().join("bin").join("SDL3.dll"), b"").unwrap();
        let tools = FakeTools::printing(0, TWO_DISPLAYS);
        let shown = list_displays(&tools, &paths, "wayland", &|_| None);
        assert_eq!(shown.len(), 2);
        list_displays(&tools, &paths, "auto", &|_| None);
        let calls = tools.calls();
        assert_eq!(calls[0].args, ["--displays"]);
        assert_eq!(calls[0].limit, Some(Duration::from_secs(5)));
        let driver = |call: &crate::core::platform::tool::Called| {
            call.env
                .as_ref()
                .and_then(|env| env.get("SDL_VIDEO_DRIVER").cloned())
        };
        if cfg!(windows) {
            assert_eq!(driver(&calls[0]), None);
        } else {
            assert_eq!(driver(&calls[0]).as_deref(), Some("wayland"));
            assert_eq!(
                calls[0]
                    .env
                    .as_ref()
                    .unwrap()
                    .get("SDL_VIDEODRIVER")
                    .map(String::as_str),
                Some("wayland")
            );
        }
        assert_eq!(driver(&calls[1]), None);
    }

    #[test]
    fn a_gamepad_line_means_connected() {
        let (dir, paths) = port_with(&["spartanport-probe"]);
        // The probe finds its libraries next to it, so a development build's MSYS2 is not needed.
        std::fs::write(dir.path().join("bin").join("SDL3.dll"), b"").unwrap();
        let yes = FakeTools::printing(
            0,
            "Gamepad: type=xbox name=Pad
",
        );
        assert_eq!(connected_gamepad(&yes, &paths, &|_| None), Some(true));
        let none = FakeTools::printing(
            0,
            "No gamepad
",
        );
        assert_eq!(connected_gamepad(&none, &paths, &|_| None), Some(false));
        // A failed probe is read by its output too, as before.
        let failed = FakeTools::printing(
            3,
            "Gamepad: type=ps5
",
        );
        assert_eq!(connected_gamepad(&failed, &paths, &|_| None), Some(true));
        assert_eq!(yes.calls()[0].args, ["--gamepad"]);
        assert_eq!(yes.calls()[0].limit, Some(Duration::from_secs(15)));
        assert_eq!(
            connected_gamepad(&FakeTools::timing_out(), &paths, &|_| None),
            None
        );
    }

    #[test]
    fn displays_without_sdl3_runs_nothing_on_windows_dev_build() {
        let (_dir, paths) = port_with(&["spartanport-probe"]);
        let tools = FakeTools::printing(0, TWO_DISPLAYS);
        let env = |key: &str| (key == "MSYS2_ROOT").then(|| r"C:\nonexistent_msys64".to_string());
        if cfg!(windows) {
            let shown = list_displays(&tools, &paths, "auto", &env);
            assert_eq!(shown, vec![]);
            assert!(tools.calls().is_empty());
        }
    }
}
