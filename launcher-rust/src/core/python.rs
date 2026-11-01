//! Python spellings the launcher must match byte for byte: `repr`, `int()`, `hex` and
//! `json.dumps`.

use std::io;

use serde::Serialize;

/// `text` as Python's `repr` spells a string.
pub(crate) fn py_repr(text: &str) -> String {
    let quote = if text.contains('\'') && !text.contains('"') {
        '"'
    } else {
        '\''
    };
    let mut out = String::with_capacity(text.len() + 2);
    out.push(quote);
    for c in text.chars() {
        match c {
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if c == quote => {
                out.push('\\');
                out.push(c);
            }
            c if c < ' ' || c == '\u{7f}' => out.push_str(&format!("\\x{:02x}", u32::from(c))),
            c => out.push(c),
        }
    }
    out.push(quote);
    out
}

/// A list of strings as Python prints it: `['a', 'b']`.
pub(crate) fn py_list<S: AsRef<str>>(items: &[S]) -> String {
    let items: Vec<String> = items.iter().map(|item| py_repr(item.as_ref())).collect();
    format!("[{}]", items.join(", "))
}

/// A number as Python's `{:#x}` spells it.
pub(crate) fn py_hex(value: i128) -> String {
    if value < 0 {
        format!("-{:#x}", value.unsigned_abs())
    } else {
        format!("{value:#x}")
    }
}

/// Python's `int(text, radix)` for radix 0, 10 or 16: surrounding whitespace, a sign, a
/// `0x`/`0o`/`0b` prefix (radix 0 takes the base from it and refuses a decimal with leading
/// zeros) and single underscores between digits. `None` where Python raises.
pub(crate) fn py_int(text: &str, radix: u32) -> Option<i128> {
    let text = text.trim();
    let (negative, unsigned) = match text.strip_prefix('-') {
        Some(rest) => (true, rest),
        None => (false, text.strip_prefix('+').unwrap_or(text)),
    };
    let prefix = unsigned.get(..2).map(str::to_ascii_lowercase);
    let prefixed = match (radix, prefix.as_deref()) {
        (0 | 16, Some("0x")) => Some(16),
        (0, Some("0o")) => Some(8),
        (0, Some("0b")) => Some(2),
        _ => None,
    };
    let (base, digits) = match prefixed {
        // One underscore may follow the prefix ("0x_ff").
        Some(base) => {
            let rest = unsigned.get(2..)?;
            (base, rest.strip_prefix('_').unwrap_or(rest))
        }
        None => (if radix == 0 { 10 } else { radix }, unsigned),
    };
    if digits.is_empty()
        || digits.starts_with('_')
        || digits.ends_with('_')
        || digits.contains("__")
    {
        return None;
    }
    let digits: String = digits.chars().filter(|&c| c != '_').collect();
    if !digits.chars().all(|c| c.is_digit(base)) {
        return None;
    }
    // int('010', 0) raises: a base-0 decimal has no leading zero unless it is all zeros.
    if radix == 0 && base == 10 && digits.starts_with('0') && digits.chars().any(|c| c != '0') {
        return None;
    }
    let value = i128::from_str_radix(&digits, base).ok()?;
    Some(if negative { -value } else { value })
}

/// `json.dumps`' default layout: one line, `", "` between items and `": "` after keys.
struct PythonCompact;

impl serde_json::ser::Formatter for PythonCompact {
    fn begin_array_value<W: ?Sized + io::Write>(
        &mut self,
        writer: &mut W,
        first: bool,
    ) -> io::Result<()> {
        if first {
            Ok(())
        } else {
            writer.write_all(b", ")
        }
    }

    fn begin_object_key<W: ?Sized + io::Write>(
        &mut self,
        writer: &mut W,
        first: bool,
    ) -> io::Result<()> {
        if first {
            Ok(())
        } else {
            writer.write_all(b", ")
        }
    }

    fn begin_object_value<W: ?Sized + io::Write>(&mut self, writer: &mut W) -> io::Result<()> {
        writer.write_all(b": ")
    }
}

/// `value` as Python's `json.dumps(value)`: [`PythonCompact`] and [`ensure_ascii`].
pub(crate) fn python_json(value: &impl Serialize) -> io::Result<String> {
    let mut bytes = Vec::new();
    value
        .serialize(&mut serde_json::Serializer::with_formatter(
            &mut bytes,
            PythonCompact,
        ))
        .map_err(io::Error::other)?;
    let text = String::from_utf8(bytes).map_err(io::Error::other)?;
    Ok(ensure_ascii(&text))
}

/// `text`, JSON serde_json wrote, as Python's `json.dumps` spells it with `ensure_ascii`: every
/// character past `~` (DEL included) as `\uXXXX`, outside the BMP as a surrogate pair. Below DEL
/// serde_json already escapes what the Python does.
pub(crate) fn ensure_ascii(text: &str) -> String {
    let mut ascii = String::with_capacity(text.len());
    for c in text.chars() {
        if c < '\u{7f}' {
            ascii.push(c);
        } else {
            for unit in c.encode_utf16(&mut [0; 2]).iter() {
                ascii.push_str(&format!("\\u{unit:04x}"));
            }
        }
    }
    ascii
}
