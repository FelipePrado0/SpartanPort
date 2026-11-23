//! The Weapon Switching tutorials for the switch attacks (`SPARTAN_SWITCH_ATTACKS=1`,
//! `src/switch_hook.h`; Python `scripts/switch_text.py`).
//!
//! With the option, L1 + Cross no longer cycles through the weapons: it repeats the switch attack
//! of the weapon in Kratos' hands, and L1 with a d-pad direction switches to that weapon with its
//! switch attack. The tutorial cards (texts 190, 283 and 286: a title, "Press <direction> for
//! <weapon>" lines, then the L1 + Cross sentence) say so instead of the old sentence. [`build`]
//! writes patched copies of the language WADs and returns the guest paths the runner mounts them
//! on; the game folder is never written.
//!
//! The text is the `MSGS_TXT` entry of each `R_LANG_<code>.WAD`, UTF-8: CRLF lines, `*<id>*` (with
//! a T for a tutorial) before each text. In each card the lines from the first one with
//! `[L1Button]` to the end of the card are replaced, so the patch does not depend on how a language
//! worded or wrapped the old sentence. The entry grows (its size field, its padding to 16, and
//! everything after it moves). A language is left as it is when its text lacks one of the new
//! characters: each language's font only has the characters its own text uses.

use std::collections::HashSet;
use std::io;
use std::path::{Path, PathBuf};

use indexmap::IndexMap;
use serde_json::{json, Value};

use crate::core::fs::atomic_write;
use crate::core::python::python_json;
use crate::runner::glyphs::entries;

/// Of the text: a change rewrites the cached copies.
pub const VERSION: u32 = 2;
pub const FOLDER: &str = "EXEC/WAD/PS4_LE";
/// The Weapon Switching cards.
pub const CARDS: [u32; 3] = [190, 283, 286];

pub const ENGLISH: [&str; 2] = [
    "Hold [L1Button] to attack when switching weapons.",
    "Use [L1Button] and [XButton] to repeat switch attacks.",
];
const SPANISH: [&str; 2] = [
    "Mantén [L1Button] para atacar al cambiar de arma.",
    "Usa [L1Button] y [XButton] para repetir el ataque de cambio.",
];

/// `R_LANG_<code>.WAD` -> the lines that end the cards, in the Python's order.
pub const TEXT: [(&str, [&str; 2]); 16] = [
    ("EN", ENGLISH),
    ("UK", ENGLISH),
    (
        "DE",
        [
            "Halte [L1Button] gedrückt, um beim Waffenwechsel anzugreifen.",
            "Drücke [L1Button] und [XButton], um Wechselangriffe zu wiederholen.",
        ],
    ),
    (
        "FR",
        [
            "Maintenez [L1Button] pour attaquer en changeant d'arme.",
            "Utilisez [L1Button] et [XButton] pour répéter l'attaque de changement.",
        ],
    ),
    (
        "IT",
        [
            "Tieni premuto [L1Button] per attaccare mentre cambi arma.",
            "Usa [L1Button] e [XButton] per ripetere l'attacco di cambio.",
        ],
    ),
    ("ES", SPANISH),
    ("LA", SPANISH),
    (
        "PT",
        [
            "Mantém [L1Button] premido para atacar ao trocar de arma.",
            "Usa [L1Button] e [XButton] para repetir o ataque de troca.",
        ],
    ),
    (
        "BR",
        [
            "Segure [L1Button] para atacar ao trocar de arma.",
            "Use [L1Button] e [XButton] para repetir o ataque de troca.",
        ],
    ),
    (
        "NL",
        [
            "Houd [L1Button] vast om aan te vallen bij een wapenwissel.",
            "Gebruik [L1Button] en [XButton] om wisselaanvallen te herhalen.",
        ],
    ),
    (
        "PL",
        [
            "Przytrzymaj [L1Button], aby zaatakować przy zmianie broni.",
            "Użyj [L1Button] i [XButton], aby powtórzyć atak zmiany broni.",
        ],
    ),
    (
        "RU",
        [
            "Удерживайте [L1Button], чтобы атаковать при смене оружия.",
            "Нажмите [L1Button] и [XButton], чтобы повторить атаку смены оружия.",
        ],
    ),
    (
        "TR",
        [
            "Silah değiştirirken saldırmak için [L1Button] tuşunu basılı tut.",
            "Değiştirme saldırısını tekrarlamak için [L1Button] ve [XButton] kullan.",
        ],
    ),
    (
        "JA",
        [
            "[L1Button]を押しながら武器を切り替えると攻撃します。",
            "[L1Button]と[XButton]で切り替え攻撃を繰り返します。",
        ],
    ),
    (
        "KO",
        [
            "[L1Button] 버튼을 누른 채 무기를 바꾸면 공격합니다.",
            "[L1Button] 버튼과 [XButton] 버튼으로 전환 공격을 반복합니다.",
        ],
    ),
    (
        "ZH",
        [
            "按住[L1Button]切換武器時會發動攻擊。",
            "使用[L1Button]和[XButton]重複切換攻擊。",
        ],
    ),
];

/// The lines of the language `code`, if the game has it.
pub fn lines_of(code: &str) -> Option<[&'static str; 2]> {
    TEXT.iter()
        .find(|(name, _)| *name == code)
        .map(|(_, lines)| *lines)
}

/// A card mark of the text (Python `CARD`, `\r\n\*(\d+)\*T?\r\n`): its start, its end and its id
/// (`None` for an id too long to be one of [`CARDS`]).
struct Mark {
    start: usize,
    end: usize,
    id: Option<u32>,
}

/// The mark that matches at byte `at` of `text`, if one does.
fn mark_at(text: &str, at: usize) -> Option<Mark> {
    let rest = text[at..].strip_prefix("\r\n*")?;
    let digits = rest.bytes().take_while(u8::is_ascii_digit).count();
    if digits == 0 {
        return None;
    }
    let after = rest[digits..].strip_prefix('*')?;
    let after = after.strip_prefix('T').unwrap_or(after);
    let after = after.strip_prefix("\r\n")?;
    Some(Mark {
        start: at,
        end: text.len() - after.len(),
        id: rest[..digits].parse().ok(),
    })
}

/// Every mark of `text`, as `re.finditer` finds them: left to right, none overlapping.
fn marks(text: &str) -> Vec<Mark> {
    let mut found = Vec::new();
    let mut at = 0;
    while let Some(next) = text[at..].find("\r\n*") {
        let start = at + next;
        match mark_at(text, start) {
            Some(mark) => {
                at = mark.end;
                found.push(mark);
            }
            None => at = start + 1,
        }
    }
    found
}

/// The `MSGS_TXT` text with the cards ending in `lines`, and how many cards changed; (text, 0)
/// when no card has an `[L1Button]` line, or the language's text lacks a character of `lines`.
pub fn rewrite(text: &str, lines: &[&str]) -> (String, usize) {
    let used: HashSet<char> = text.chars().collect();
    if lines
        .iter()
        .flat_map(|line| line.chars())
        .any(|c| !used.contains(&c))
    {
        return (text.to_string(), 0);
    }
    let marks = marks(text);
    let mut out = String::with_capacity(text.len() + 256);
    let (mut at, mut count) = (0, 0);
    for (index, mark) in marks.iter().enumerate() {
        if !mark.id.is_some_and(|id| CARDS.contains(&id)) {
            continue;
        }
        let end = marks.get(index + 1).map_or(text.len(), |next| next.start);
        let body: Vec<&str> = text[mark.end..end].split("\r\n").collect();
        let Some(first) = body.iter().position(|line| line.contains("[L1Button]")) else {
            continue;
        };
        out.push_str(&text[at..mark.end]);
        let kept = body[..first].iter().chain(lines.iter());
        out.push_str(&kept.copied().collect::<Vec<_>>().join("\r\n"));
        at = end;
        count += 1;
    }
    out.push_str(&text[at..]);
    (out, count)
}

/// Ends the Weapon Switching cards of the WAD in `data` with `lines`, growing its `MSGS_TXT`
/// entry; returns how many cards changed.
pub fn patch(data: &mut Vec<u8>, lines: &[&str]) -> usize {
    let Some(entry) = entries(data).into_iter().find(|e| e.name == "MSGS_TXT") else {
        return 0;
    };
    let (start, size) = (entry.offset + 0x40, entry.size);
    let end = start + size;
    let Ok(text) = std::str::from_utf8(&data[start..end]) else {
        return 0;
    };
    let (text, count) = rewrite(text, lines);
    if count > 0 {
        let raw = text.into_bytes();
        let padded = (end + (16 - size % 16) % 16).min(data.len());
        let fill = (16 - raw.len() % 16) % 16;
        let length = raw.len();
        data.splice(
            start..padded,
            raw.into_iter().chain(std::iter::repeat_n(0, fill)),
        );
        // The size field is a u32: a text past 4 GB is no WAD's.
        let length = u32::try_from(length).unwrap_or(u32::MAX);
        data[entry.offset + 4..entry.offset + 8].copy_from_slice(&length.to_le_bytes());
    }
    count
}

fn mtime_ns(meta: &std::fs::Metadata) -> io::Result<i64> {
    let modified = meta.modified()?;
    match modified.duration_since(std::time::UNIX_EPOCH) {
        Ok(after) => i64::try_from(after.as_nanos()),
        Err(before) => i64::try_from(before.duration().as_nanos()).map(|nanos| -nanos),
    }
    .map_err(io::Error::other)
}

/// `{guest path: patched copy}` for the runner to mount. The copies in `out/switch_text` are kept
/// until the game's language WADs change; `stamp.json` records them as the Python's `json.dumps`
/// does, so a copy either launcher made is used by the other.
pub fn build(
    game: &Path,
    out: &Path,
    log: &mut dyn FnMut(String),
) -> io::Result<IndexMap<String, PathBuf>> {
    let target = out.join("switch_text");
    let sources: Vec<(String, &str, PathBuf)> = TEXT
        .iter()
        .map(|(code, _)| {
            let file = format!("R_LANG_{code}.WAD");
            (
                format!("{FOLDER}/{file}"),
                *code,
                game.join(FOLDER).join(&file),
            )
        })
        .filter(|(_, _, path)| path.is_file())
        .collect();
    let mut listed = Vec::new();
    for (name, _, path) in &sources {
        let meta = std::fs::metadata(path)?;
        listed.push(json!([name, meta.len(), mtime_ns(&meta)?]));
    }
    let stamp = python_json(&json!([VERSION, Value::Array(listed)]))?;
    let stamp_file = target.join("stamp.json");
    let file_name = |name: &str| name.rsplit('/').next().unwrap_or(name).to_string();
    let mut made: Vec<String> = std::fs::read_dir(&target)
        .map(|dir| {
            dir.filter_map(Result::ok)
                .map(|e| e.file_name().to_string_lossy().into_owned())
                .filter(|n| n.ends_with(".WAD"))
                .collect()
        })
        .unwrap_or_default();
    if std::fs::read_to_string(&stamp_file).ok().as_deref() != Some(stamp.as_str()) {
        let _ = std::fs::remove_dir_all(&target);
        // As the Python's mkdir(parents=True): a folder that could not be removed is an error.
        if let Some(parent) = target.parent() {
            std::fs::create_dir_all(parent)?;
        }
        std::fs::create_dir(&target)?;
        made.clear();
        for (name, code, path) in &sources {
            let mut data = std::fs::read(path)?;
            let lines = lines_of(code).unwrap_or(ENGLISH);
            let file = file_name(name);
            if patch(&mut data, &lines) > 0 {
                atomic_write(&target.join(&file), &data)?;
                made.push(file);
            } else {
                log(format!("Switch attacks: {file} keeps its tutorial text"));
            }
        }
        atomic_write(&stamp_file, stamp.as_bytes())?; // last: an interrupted build is redone
    }
    Ok(sources
        .iter()
        .filter(|(name, _, _)| made.contains(&file_name(name)))
        .map(|(name, _, _)| (format!("/app0/{name}"), target.join(file_name(name))))
        .collect())
}

#[cfg(test)]
pub(crate) mod tests {
    //! `tests/test_switch_text.py` on a synthetic WAD; the runner's step is in `runner/mod.rs`.
    use super::*;

    fn entry(name: &str, payload: &[u8], tag: u16) -> Vec<u8> {
        let mut out = Vec::new();
        out.extend_from_slice(&tag.to_le_bytes());
        out.extend_from_slice(&0u16.to_le_bytes());
        out.extend_from_slice(&u32::try_from(payload.len()).unwrap().to_le_bytes());
        let mut field = name.as_bytes().to_vec();
        field.resize(56, 0);
        out.extend_from_slice(&field);
        out.extend_from_slice(payload);
        out.resize(out.len() + (16 - payload.len() % 16) % 16, 0);
        out
    }

    pub(crate) const OLD: &str =
        "Use [L1Button] and [XButton] to rapidly\r\nswitch weapons while attacking.";

    /// Every letter of the new English lines is in it.
    pub(crate) fn text() -> String {
        format!(
            "5384 7373\r\n*190*T\r\n[*1]Weapon Switching\r\nPress [RightButton] for Claws.\r\n{OLD}\
             \r\n*191*\r\nPress [L1Button] to block.\r\n*283*T\r\n[*1]Weapon Switching\r\n{OLD}\
             \r\n*284*\r\nOnyx\r\n*286*T\r\n[*1]Weapon Switching\r\nPress [LeftButton] for Whip.\r\n\
             *5000*\r\nHold to repeat attacks when using weapons.\r\n\0"
        )
    }

    pub(crate) fn wad_of(text: &str) -> Vec<u8> {
        [
            entry("WAD_R_Lang", &[0; 8], 0x14),
            entry("WAD_R_Lang", &[], 0x12),
            entry("MSGS_TXT", text.as_bytes(), 7),
            entry("TXR_GoWIII_en", &[0xee; 100], 1),
            entry("SMF_0", &[b"SMF2".as_slice(), &[0x11; 20]].concat(), 7),
        ]
        .concat()
    }

    pub(crate) fn wad() -> Vec<u8> {
        wad_of(&text())
    }

    #[test]
    fn cards_end_with_the_new_lines_and_what_follows_moves() {
        let mut data = wad();
        assert_eq!(patch(&mut data, &ENGLISH), 2); // 286 has no L1 line here
        let found: IndexMap<String, (usize, usize)> = entries(&data)
            .into_iter()
            .map(|e| (e.name, (e.offset, e.size)))
            .collect();
        assert_eq!(
            found.keys().collect::<Vec<_>>(),
            ["WAD_R_Lang", "MSGS_TXT", "TXR_GoWIII_en", "SMF_0"]
        );
        let (offset, size) = found["MSGS_TXT"];
        let text = std::str::from_utf8(&data[offset + 0x40..offset + 0x40 + size]).unwrap();
        assert_eq!(text, self::text().replace(OLD, &ENGLISH.join("\r\n")));
        assert!(text.contains("*191*\r\nPress [L1Button] to block.")); // not a Weapon Switching card
        assert_eq!(
            text.matches("\r\n").count(),
            self::text().matches("\r\n").count()
        ); // the same lines
        assert_eq!(found["TXR_GoWIII_en"].0 % 16, 0);
        let tail = found["SMF_0"].0;
        assert_eq!(&data[tail + 0x40..tail + 0x44], b"SMF2");
    }

    #[test]
    fn any_wording_of_the_old_sentence() {
        let text = "1 1\r\n*1*\r\nHalte gedrückt, um bei Angriffen Wechselangriffe zu wiederholen.\r\n*283*T\r\n[*1]Waffenwechsel\r\nDrücke [DownButton] für den Cestus.\r\nMit [L1Button] und [XButton]\r\nwechselst du schnell\r\nim Angriff.\r\n*284*\r\nOnyx\r\n";
        let german = lines_of("DE").unwrap();
        let (new, count) = rewrite(text, &german);
        assert_eq!(count, 1);
        assert_eq!(
            new,
            format!(
                "1 1\r\n*1*\r\nHalte gedrückt, um bei Angriffen Wechselangriffe zu wiederholen.\r\n*283*T\r\n[*1]Waffenwechsel\r\nDrücke [DownButton] für den Cestus.\r\n{}\r\n*284*\r\nOnyx\r\n",
                german.join("\r\n")
            )
        );
    }

    #[test]
    fn a_language_whose_font_lacks_a_new_character_keeps_its_text() {
        let text = self::text().replace("rapidly", "rapidement"); // no ç, é or ' in this "French"
        assert_eq!(rewrite(&text, &lines_of("FR").unwrap()), (text.clone(), 0));
        let empty = "5384 7373\r\n*1*\r\nEmpty\r\n\0";
        let mut data = wad_of(empty);
        assert_eq!(patch(&mut data, &ENGLISH), 0);
        assert_eq!(data, wad_of(empty));
    }

    #[test]
    fn marks_are_found_as_the_pythons_regex_finds_them() {
        // `*12x*` and `*7*` without its CRLF are no marks; a T is optional; ids parse as numbers.
        let text = "a\r\n*12x*\r\nb\r\n*7*c\r\n*0190*T\r\nd\r\n*283*\r\n";
        let found: Vec<_> = marks(text).iter().map(|m| (m.start, m.end, m.id)).collect();
        assert_eq!(found, [(17, 28, Some(190)), (29, 38, Some(283))]);
        assert_eq!(&text[17..28], "\r\n*0190*T\r\n");
    }

    #[test]
    fn build_mounts_copies_and_never_writes_the_game() {
        let temp = tempfile::tempdir().unwrap();
        let (game, out) = (temp.path().join("game"), temp.path().join("out"));
        let folder = game.join(FOLDER);
        std::fs::create_dir_all(&folder).unwrap();
        std::fs::write(folder.join("R_LANG_EN.WAD"), wad()).unwrap();
        std::fs::write(folder.join("R_LANG_JA.WAD"), wad()).unwrap(); // no Japanese characters: kept
        std::fs::write(folder.join("R_LANG_XX.WAD"), wad()).unwrap(); // not a language of the game
        let mut log = Vec::new();
        let mounts = build(&game, &out, &mut |line| log.push(line)).unwrap();
        let guest = "/app0/EXEC/WAD/PS4_LE/R_LANG_EN.WAD";
        assert_eq!(mounts.keys().collect::<Vec<_>>(), [guest]);
        let copy = std::fs::read(&mounts[guest]).unwrap();
        assert!(copy
            .windows(ENGLISH[1].len())
            .any(|window| window == ENGLISH[1].as_bytes()));
        assert_eq!(std::fs::read(folder.join("R_LANG_EN.WAD")).unwrap(), wad());
        assert_eq!(
            log,
            ["Switch attacks: R_LANG_JA.WAD keeps its tutorial text"]
        );
        // Cached: nothing is patched or said again.
        let mut again = Vec::new();
        assert_eq!(
            build(&game, &out, &mut |line| again.push(line)).unwrap(),
            mounts
        );
        assert!(again.is_empty());
    }

    #[test]
    fn the_stamp_is_the_pythons_json() {
        let temp = tempfile::tempdir().unwrap();
        let (game, out) = (temp.path().join("game"), temp.path().join("out"));
        let folder = game.join(FOLDER);
        std::fs::create_dir_all(&folder).unwrap();
        std::fs::write(folder.join("R_LANG_DE.WAD"), wad()).unwrap();
        build(&game, &out, &mut |_| {}).unwrap();
        let meta = std::fs::metadata(folder.join("R_LANG_DE.WAD")).unwrap();
        let expected = format!(
            "[2, [[\"EXEC/WAD/PS4_LE/R_LANG_DE.WAD\", {}, {}]]]",
            meta.len(),
            mtime_ns(&meta).unwrap()
        );
        assert_eq!(
            std::fs::read_to_string(out.join("switch_text/stamp.json")).unwrap(),
            expected
        );
    }
}
