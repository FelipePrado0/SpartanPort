//! The in-memory log ring, the run log file and the output-line classifiers.

use chrono::{DateTime, Local, SecondsFormat};
use serde_json::{json, Value};
use std::collections::VecDeque;
use std::fs::File;
use std::io::{self, Write};
use std::path::Path;
use std::time::Instant;

/// Whole lines the launcher log keeps; older ones are dropped.
pub const MAX_LOG_LINES: usize = 6000;

/// A ring of whole lines plus the partial line still waiting for its `\n`.
#[derive(Debug, Clone)]
pub struct LogBuffer {
    max_lines: usize,
    lines: VecDeque<String>,
    partial: String,
}

impl LogBuffer {
    pub fn new(max_lines: usize) -> Self {
        Self {
            max_lines,
            lines: VecDeque::new(),
            partial: String::new(),
        }
    }

    /// Appends `text`. Each `\n` completes a line (one trailing `\r` is stripped); the text after
    /// the last `\n` stays as a partial line that the next push continues.
    pub fn push_text(&mut self, text: &str) {
        let mut rest = text;
        while let Some(pos) = rest.find('\n') {
            self.partial.push_str(&rest[..pos]);
            if self.partial.ends_with('\r') {
                self.partial.pop();
            }
            self.lines.push_back(std::mem::take(&mut self.partial));
            rest = &rest[pos + 1..];
        }
        self.partial.push_str(rest);
        while self.lines.len() > self.max_lines {
            self.lines.pop_front();
        }
    }

    /// The whole lines, then the partial line when it is not empty.
    pub fn lines(&self) -> impl Iterator<Item = &str> {
        self.lines
            .iter()
            .map(String::as_str)
            .chain((!self.partial.is_empty()).then_some(self.partial.as_str()))
    }

    /// The lines joined with `\n`.
    pub fn text(&self) -> String {
        let mut out = String::new();
        for (i, line) in self.lines().enumerate() {
            if i > 0 {
                out.push('\n');
            }
            out.push_str(line);
        }
        out
    }

    pub fn clear(&mut self) {
        self.lines.clear();
        self.partial.clear();
    }

    /// Whole lines, plus one when a non-empty partial line exists.
    pub fn len(&self) -> usize {
        self.lines.len() + usize::from(!self.partial.is_empty())
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }
}

/// The run log file, and next to it the timestamped journal of the same lines. After any failure a
/// file stops being written and never fails again, so the game still starts and its output is
/// still drained; the two fail apart, and the journal never changes what `last_run.log` holds.
#[derive(Debug)]
pub struct RunLog {
    file: Option<File>,
    journal: Option<RunJournal>,
}

impl RunLog {
    /// Creates the folder and truncates the file, and does the same for `last_run.jsonl` beside
    /// it; a file that fails is left closed.
    pub fn open(path: &Path) -> RunLog {
        let journal = create_truncated(&path.with_extension("jsonl"))
            .and_then(|file| RunJournal::new(Box::new(file)))
            .ok();
        RunLog {
            file: create_truncated(path).ok(),
            journal,
        }
    }

    pub fn is_open(&self) -> bool {
        self.file.is_some()
    }

    /// Writes and flushes `text`; on any error the file is dropped and later writes do nothing.
    pub fn write(&mut self, text: &str) {
        self.write_from(text, Source::Game);
    }

    /// Ends the run: writes the advice for the failed exit (a blank line, then the advice) and the
    /// journal's `exit` record. `code` is `None` when the exit status could not be read.
    pub fn finish(mut self, code: Option<i32>, advice: Option<&str>) {
        if let Some(advice) = advice {
            self.write_from(&format!("\n{advice}\n"), Source::Launcher);
        }
        self.with_journal(|journal| journal.record_exit(code, advice));
    }

    fn write_from(&mut self, text: &str, source: Source) {
        if let Some(file) = &mut self.file {
            if file
                .write_all(text.as_bytes())
                .and_then(|()| file.flush())
                .is_err()
            {
                self.file = None;
            }
        }
        self.with_journal(|journal| journal.record_text(text, source));
    }

    fn with_journal(&mut self, record: impl FnOnce(&mut RunJournal) -> io::Result<()>) {
        if let Some(journal) = &mut self.journal {
            if record(journal).is_err() {
                self.journal = None;
            }
        }
    }
}

fn create_truncated(path: &Path) -> io::Result<File> {
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent)?;
    }
    File::create(path)
}

/// Who wrote a line of the run log.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Source {
    Game,
    Launcher,
}

impl Source {
    fn as_str(self) -> &'static str {
        match self {
            Source::Game => "game",
            Source::Launcher => "launcher",
        }
    }
}

/// How serious a line is.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Level {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
}

impl Level {
    /// The level named by the GPU logger (`Common::Log::Write`), in any case.
    fn from_gpu_logger(name: &str) -> Option<Level> {
        match name.to_ascii_lowercase().as_str() {
            "trace" => Some(Level::Trace),
            "debug" => Some(Level::Debug),
            "info" => Some(Level::Info),
            "warning" => Some(Level::Warning),
            "error" => Some(Level::Error),
            "critical" => Some(Level::Critical),
            _ => None,
        }
    }

    fn as_str(self) -> &'static str {
        match self {
            Level::Trace => "trace",
            Level::Debug => "debug",
            Level::Info => "info",
            Level::Warning => "warning",
            Level::Error => "error",
            Level::Critical => "critical",
        }
    }
}

/// The longest prefix of a line that still reads as an area label.
const MAX_AREA_CHARS: usize = 32;

/// The level and the area of one output line. The game prints with `printf` and an area prefix
/// (`Texture cache: ...`) and its GPU logger prints `GPU [Class] <Level> file:line func: msg`, so
/// this is a guess from the text: a prefix that only looks like a label becomes an area too.
fn classify(text: &str) -> (Level, Option<&str>) {
    if let Some(gpu_line) = classify_gpu_logger_line(text) {
        return gpu_line;
    }
    let level = if text.starts_with("STOP:") || text.starts_with("Fault:") {
        Level::Critical
    } else {
        Level::Info
    };
    (level, area_prefix(text))
}

fn classify_gpu_logger_line(text: &str) -> Option<(Level, Option<&str>)> {
    let (class, rest) = text.strip_prefix("GPU [")?.split_once("] <")?;
    let (level, _) = rest.split_once('>')?;
    Some((Level::from_gpu_logger(level)?, Some(class)))
}

fn area_prefix(text: &str) -> Option<&str> {
    let (label, _) = text.split_once(": ")?;
    let is_label = label.chars().count() <= MAX_AREA_CHARS
        && label.starts_with(char::is_alphabetic)
        && !label.contains([':', '\\']);
    is_label.then_some(label)
}

/// A line whose `\n` has not arrived yet, with when and from whom its first piece came.
#[derive(Debug)]
struct OpenLine {
    text: String,
    source: Source,
    at: Instant,
    time: DateTime<Local>,
}

impl OpenLine {
    fn begin(source: Source) -> OpenLine {
        OpenLine {
            text: String::new(),
            source,
            at: Instant::now(),
            time: Local::now(),
        }
    }
}

/// The level and area of the last line, which an indented continuation line inherits.
#[derive(Debug)]
struct LineContext {
    level: Level,
    area: Option<String>,
}

/// `last_run.jsonl`: one JSON object per line, in order. It splits the text at `\n` exactly as
/// `last_run.log` is split, so a record's `n` is that line's number in the log.
struct RunJournal {
    out: Box<dyn Write + Send>,
    opened: Instant,
    line_number: u64,
    open_line: Option<OpenLine>,
    previous: Option<LineContext>,
}

impl std::fmt::Debug for RunJournal {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("RunJournal")
            .field("line_number", &self.line_number)
            .finish_non_exhaustive()
    }
}

impl RunJournal {
    /// Writes the `start` record to `out`.
    fn new(out: Box<dyn Write + Send>) -> io::Result<RunJournal> {
        let mut journal = RunJournal {
            out,
            opened: Instant::now(),
            line_number: 0,
            open_line: None,
            previous: None,
        };
        journal.write_record(&json!({
            "type": "start",
            "schema": 1,
            "time": format_time(Local::now()),
            "launcher": env!("CARGO_PKG_VERSION"),
            "os": std::env::consts::OS,
        }))?;
        Ok(journal)
    }

    /// Takes `text` as the next piece of the log: each `\n` completes a line, and what follows
    /// the last one stays open, stamped by the piece that began it.
    fn record_text(&mut self, text: &str, source: Source) -> io::Result<()> {
        let mut rest = text;
        while let Some(end) = rest.find('\n') {
            let mut line = self
                .open_line
                .take()
                .unwrap_or_else(|| OpenLine::begin(source));
            line.text.push_str(&rest[..end]);
            self.write_line(line)?;
            rest = &rest[end + 1..];
        }
        if !rest.is_empty() {
            let line = self
                .open_line
                .get_or_insert_with(|| OpenLine::begin(source));
            line.text.push_str(rest);
        }
        Ok(())
    }

    /// Ends the journal: a line still open is written, then the `exit` record.
    fn record_exit(&mut self, code: Option<i32>, advice: Option<&str>) -> io::Result<()> {
        if let Some(line) = self.open_line.take() {
            self.write_line(line)?;
        }
        self.write_record(&json!({
            "type": "exit",
            "t": self.seconds_since_open(Instant::now()),
            "time": format_time(Local::now()),
            "code": code,
            "advice": advice,
        }))
    }

    fn write_line(&mut self, line: OpenLine) -> io::Result<()> {
        self.line_number += 1;
        let text = line.text.strip_suffix('\r').unwrap_or(&line.text);
        let continuation = self.previous.is_some() && text.starts_with(char::is_whitespace);
        let (level, area) = match (&self.previous, continuation) {
            (Some(previous), true) => (previous.level, previous.area.clone()),
            _ => {
                let (level, area) = classify(text);
                (level, area.map(str::to_owned))
            }
        };
        let mut record = json!({
            "type": "line",
            "n": self.line_number,
            "t": self.seconds_since_open(line.at),
            "time": format_time(line.time),
            "source": line.source.as_str(),
            "level": level.as_str(),
            "area": area,
        });
        if continuation {
            record["continuation"] = json!(true);
        }
        record["text"] = json!(text);
        self.previous = Some(LineContext { level, area });
        self.write_record(&record)
    }

    fn seconds_since_open(&self, at: Instant) -> f64 {
        at.saturating_duration_since(self.opened).as_millis() as f64 / 1000.0
    }

    /// One `write_all` per record, so a record is never torn by another writer.
    fn write_record(&mut self, record: &Value) -> io::Result<()> {
        let mut bytes = serde_json::to_vec(record)?;
        bytes.push(b'\n');
        self.out.write_all(&bytes)?;
        self.out.flush()
    }
}

fn format_time(time: DateTime<Local>) -> String {
    time.to_rfc3339_opts(SecondsFormat::Millis, false)
}

/// Why the game closed, when an output line says so.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ExitHint {
    /// Windows could not reserve the guest memory: the page file.
    GuestMemory,
    /// Linux could not allocate the guest memory: RAM, swap or /dev/shm.
    LinuxMemory,
    DumpIncomplete,
}

impl ExitHint {
    /// The English advice; pass it through `tr` for the UI language.
    pub fn text_en(&self) -> &'static str {
        match self {
            ExitHint::GuestMemory => {
                "Windows could not reserve the memory the game needs. Close other programs, or let the \
                 Windows page file grow (for example 16384 MB initial, 32768 MB maximum), and start again."
            }
            ExitHint::LinuxMemory => {
                "Linux could not allocate the memory the game needs. Close other programs, check available \
                 RAM, swap space, or /dev/shm, and start again."
            }
            ExitHint::DumpIncomplete => {
                "The game stopped while reading its files: the game dump may be incomplete. Extract the pkg \
                 again and check that the game folder is about 40 GB."
            }
        }
    }
}

/// The advice for an output line, for the system the launcher runs on.
pub fn exit_hint(line: &str) -> Option<ExitHint> {
    exit_hint_on(line, cfg!(windows))
}

/// `exit_hint` with the system given: a guest memory line that names the page file is Windows
/// advice anywhere, one that names /dev/shm is Linux advice anywhere, and any other follows
/// `windows` (the Python's `WINDOWS`).
pub fn exit_hint_on(line: &str, windows: bool) -> Option<ExitHint> {
    if line.contains("of guest memory") {
        let page_file = line.contains("is the page file large enough?")
            || (!line.contains("is /dev/shm or RAM large enough?") && windows);
        Some(if page_file {
            ExitHint::GuestMemory
        } else {
            ExitHint::LinuxMemory
        })
    } else if line.contains("guest raised debug exception code=0xa0020001") {
        Some(ExitHint::DumpIncomplete)
    } else {
        None
    }
}

/// What an output line says about the game's state.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LineStatus {
    Running,
    Restarting,
}

pub fn line_status(line: &str) -> Option<LineStatus> {
    if line.contains("Entering original x86-64 code") {
        Some(LineStatus::Running)
    } else if line.contains("restarting through run.py") {
        Some(LineStatus::Restarting)
    } else {
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ring_keeps_only_the_last_6000_lines() {
        let mut buf = LogBuffer::new(MAX_LOG_LINES);
        for i in 0..6500 {
            buf.push_text(&format!("line {i}\n"));
        }
        assert_eq!(buf.len(), 6000);
        assert_eq!(buf.lines().next(), Some("line 500"));
        assert_eq!(buf.lines().last(), Some("line 6499"));
        buf.clear();
        assert!(buf.is_empty());
        assert_eq!(buf.text(), "");
    }

    #[test]
    fn a_partial_line_is_completed_by_the_next_push() {
        let mut buf = LogBuffer::new(10);
        buf.push_text("ab\ncd");
        assert_eq!(buf.len(), 2);
        assert_eq!(buf.lines().collect::<Vec<_>>(), ["ab", "cd"]);
        buf.push_text("ef\r\ngh\n");
        assert_eq!(buf.lines().collect::<Vec<_>>(), ["ab", "cdef", "gh"]);
        assert_eq!(buf.text(), "ab\ncdef\ngh");
    }

    #[test]
    fn ten_megabytes_without_newline_stay_one_line() {
        let mut buf = LogBuffer::new(MAX_LOG_LINES);
        let chunk = "x".repeat(1 << 20);
        for _ in 0..10 {
            buf.push_text(&chunk);
        }
        assert_eq!(buf.len(), 1);
        assert_eq!(buf.lines().count(), 1);
        assert_eq!(buf.text().len(), 10 << 20);
        buf.push_text("\n");
        assert_eq!(buf.len(), 1);
        buf.push_text("next\n");
        assert_eq!(buf.len(), 2);
    }

    #[test]
    fn run_log_keeps_going_after_a_write_error() {
        let dir = tempfile::tempdir().unwrap();
        let blocker = dir.path().join("file");
        std::fs::write(&blocker, "x").unwrap();
        let mut log = RunLog::open(&blocker.join("last_run.log"));
        assert!(!log.is_open());
        log.write("ignored\n");
        log.write("still ignored\n");
        assert!(!log.is_open());
    }

    #[test]
    fn run_log_truncates_and_creates_the_folder() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("sub").join("last_run.log");
        std::fs::create_dir_all(path.parent().unwrap()).unwrap();
        std::fs::write(&path, "old").unwrap();
        let mut log = RunLog::open(&path);
        assert!(log.is_open());
        assert_eq!(std::fs::read_to_string(&path).unwrap(), "");
        log.write("a\n");
        log.write("b\n");
        assert_eq!(std::fs::read_to_string(&path).unwrap(), "a\nb\n");
        let deep = dir.path().join("x").join("y").join("run.log");
        assert!(RunLog::open(&deep).is_open());
    }

    #[test]
    fn guest_memory_failure_points_to_the_page_file_on_windows() {
        let line = "Runtime: cannot create 8128 MiB of guest memory (error 1450); is the page file large enough?\n";
        // The line names the page file: Windows advice wherever the launcher runs.
        for windows in [true, false] {
            let hint = exit_hint_on(line, windows).unwrap();
            assert!(hint.text_en().contains("page file"), "{windows}");
            assert!(hint.text_en().contains("Windows could not reserve"));
        }
    }

    #[test]
    fn guest_memory_failure_points_to_linux_memory_on_linux() {
        let line = "Runtime: cannot create 6080 MiB of guest memory (error 12); is /dev/shm or RAM large enough?\n";
        for windows in [true, false] {
            let hint = exit_hint_on(line, windows).unwrap();
            assert!(
                hint.text_en().contains("Linux could not allocate"),
                "{windows}"
            );
            assert!(hint.text_en().contains("/dev/shm"));
            assert!(!hint.text_en().contains("page file"));
        }
        // The Python tables translate it.
        assert_eq!(
            crate::core::i18n::tr("pt", ExitHint::LinuxMemory.text_en()),
            "O Linux não conseguiu alocar a memória que o jogo precisa. Feche outros programas, verifique a memória RAM disponível, swap ou /dev/shm, e inicie de novo."
        );
    }

    #[test]
    fn guest_memory_mapping_failure_on_linux() {
        let line = "Runtime: cannot map 6080 MiB of guest memory (error 12)\n";
        let hint = exit_hint_on(line, false).unwrap();
        assert!(hint.text_en().contains("Linux could not allocate"));
        assert!(hint.text_en().contains("RAM, swap space, or /dev/shm"));
    }

    #[test]
    fn windows_environment_uses_windows_advice_for_generic_error() {
        let line = "Runtime: cannot create 6080 MiB of guest memory (error 1450)\n";
        let hint = exit_hint_on(line, true).unwrap();
        assert!(hint.text_en().contains("Windows could not reserve"));
        assert!(hint.text_en().contains("page file"));
    }

    #[test]
    fn linux_environment_uses_linux_advice_for_generic_error() {
        let line = "Runtime: cannot create 6080 MiB of guest memory (error 12)\n";
        let hint = exit_hint_on(line, false).unwrap();
        assert!(hint.text_en().contains("Linux could not allocate"));
        assert!(hint.text_en().contains("/dev/shm"));
    }

    #[test]
    fn hints_and_statuses_match_the_python_strings() {
        assert_eq!(
            exit_hint_on("mmap failed of guest memory (error 1455)", true),
            Some(ExitHint::GuestMemory)
        );
        assert_eq!(
            exit_hint_on("mmap failed of guest memory (error 1455)", false),
            Some(ExitHint::LinuxMemory)
        );
        assert_eq!(
            exit_hint("guest raised debug exception code=0xa0020001 at rip"),
            Some(ExitHint::DumpIncomplete)
        );
        assert_eq!(exit_hint("just a normal line"), None);
        assert!(ExitHint::GuestMemory.text_en().contains("page file"));
        assert!(ExitHint::DumpIncomplete.text_en().contains("about 40 GB"));
        assert_eq!(
            line_status("Entering original x86-64 code"),
            Some(LineStatus::Running)
        );
        assert_eq!(
            line_status("restarting through run.py"),
            Some(LineStatus::Restarting)
        );
        assert_eq!(line_status("other"), None);
    }

    use serde_json::Value;
    use std::path::PathBuf;
    use std::time::Duration;

    /// A run log in a fresh folder, with the paths of its two files.
    fn open_in_tempdir() -> (tempfile::TempDir, RunLog, PathBuf, PathBuf) {
        let dir = tempfile::tempdir().unwrap();
        let log_path = dir.path().join("last_run.log");
        let journal_path = dir.path().join("last_run.jsonl");
        let log = RunLog::open(&log_path);
        (dir, log, log_path, journal_path)
    }

    fn records(journal_path: &Path) -> Vec<Value> {
        std::fs::read_to_string(journal_path)
            .unwrap()
            .lines()
            .map(|line| serde_json::from_str(line).unwrap())
            .collect()
    }

    /// The `line` records, without the start and exit ones.
    fn line_records(journal_path: &Path) -> Vec<Value> {
        records(journal_path)
            .into_iter()
            .filter(|record| record["type"] == "line")
            .collect()
    }

    /// Writes `text` as game output and returns the single line record it makes.
    fn only_line_record(text: &str) -> Value {
        let (_dir, mut log, _, journal_path) = open_in_tempdir();
        log.write(text);
        log.finish(Some(0), None);
        let mut lines = line_records(&journal_path);
        assert_eq!(lines.len(), 1, "{lines:?}");
        lines.remove(0)
    }

    fn seconds(record: &Value) -> f64 {
        record["t"].as_f64().unwrap()
    }

    #[test]
    fn journal_starts_with_a_start_record_and_truncates_the_old_one() {
        let dir = tempfile::tempdir().unwrap();
        let journal_path = dir.path().join("last_run.jsonl");
        std::fs::write(&journal_path, "old\n").unwrap();
        let log = RunLog::open(&dir.path().join("last_run.log"));
        drop(log);
        let all = records(&journal_path);
        assert_eq!(all.len(), 1, "{all:?}");
        let start = &all[0];
        assert_eq!(start["type"], "start");
        assert_eq!(start["schema"], 1);
        assert_eq!(start["launcher"], env!("CARGO_PKG_VERSION"));
        assert_eq!(start["os"], std::env::consts::OS);
        let time = start["time"].as_str().unwrap();
        assert!(chrono::DateTime::parse_from_rfc3339(time).is_ok(), "{time}");
        // Milliseconds, as in 2026-11-02T14:03:21.482-03:00.
        assert_eq!(time.find('.'), Some(19), "{time}");
        assert_eq!(time[20..].find(['+', '-', 'Z']), Some(3), "{time}");
    }

    #[test]
    fn journal_lines_keep_the_log_numbers_and_strip_the_line_ending() {
        let (_dir, mut log, log_path, journal_path) = open_in_tempdir();
        log.write("first\r\n");
        log.write("\n");
        std::thread::sleep(Duration::from_millis(20));
        log.write("third\n");
        log.finish(Some(0), None);
        let lines = line_records(&journal_path);
        let numbered: Vec<(u64, &str)> = lines
            .iter()
            .map(|r| (r["n"].as_u64().unwrap(), r["text"].as_str().unwrap()))
            .collect();
        assert_eq!(numbered, [(1, "first"), (2, ""), (3, "third")]);
        assert!(seconds(&lines[0]) <= seconds(&lines[1]));
        assert!(seconds(&lines[2]) - seconds(&lines[1]) >= 0.015);
        assert!(lines.iter().all(|r| r["source"] == "game"));
        assert!(lines
            .iter()
            .all(|r| chrono::DateTime::parse_from_rfc3339(r["time"].as_str().unwrap()).is_ok()));
        // The log itself did not change.
        assert_eq!(
            std::fs::read_to_string(log_path).unwrap(),
            "first\r\n\nthird\n"
        );
    }

    #[test]
    fn a_gpu_logger_line_gives_its_level_and_class() {
        let line = only_line_record(
            "GPU [Render.Vulkan] <Warning> vk_scheduler.cpp:120 Flush: submit took 31 ms\n",
        );
        assert_eq!(line["level"], "warning");
        assert_eq!(line["area"], "Render.Vulkan");
        for (name, level) in [
            ("Trace", "trace"),
            ("Debug", "debug"),
            ("Info", "info"),
            ("Error", "error"),
            ("Critical", "critical"),
        ] {
            let line = only_line_record(&format!("GPU [Shader] <{name}> a.cpp:1 f: m\n"));
            assert_eq!(line["level"], level);
            assert_eq!(line["area"], "Shader");
        }
    }

    #[test]
    fn stop_and_fault_lines_are_critical() {
        let stop = only_line_record("STOP: first unsupported PS4 import: sceFoo (index 7)\n");
        assert_eq!(stop["level"], "critical");
        assert_eq!(stop["area"], "STOP");
        let fault = only_line_record("Fault: access violation at 0x1234\n");
        assert_eq!(fault["level"], "critical");
        assert_eq!(fault["area"], "Fault");
    }

    #[test]
    fn a_prefixed_game_line_is_info_with_the_prefix_as_area() {
        let line = only_line_record("Texture cache: memory pressure, evicted 12 images\r\n");
        assert_eq!(line["level"], "info");
        assert_eq!(line["area"], "Texture cache");
        assert_eq!(
            line["text"],
            "Texture cache: memory pressure, evicted 12 images"
        );
    }

    #[test]
    fn free_text_has_no_area() {
        for text in [
            "line 1\n",
            "mmap failed of guest memory (error 1455)\n",
            "\n",
            " leading space: not a label\n",
            "9 lives: starts with a digit\n",
            "a sentence that is far too long to be a label: and then some text\n",
            "C:\\games\\x: path-like\n",
        ] {
            let line = only_line_record(text);
            assert_eq!(line["level"], "info", "{text}");
            assert_eq!(line["area"], Value::Null, "{text}");
        }
    }

    #[test]
    fn indented_lines_continue_the_previous_line() {
        let (_dir, mut log, _, journal_path) = open_in_tempdir();
        log.write("    leading continuation without a previous line\n");
        log.write("GPU [Shader] <Error> s.cpp:9 compile: failed\n");
        log.write("  stage: fragment\n");
        log.write("\tline 12\n");
        log.write("Runtime: next report\n");
        log.write("  detail\n");
        log.finish(Some(0), None);
        let lines = line_records(&journal_path);
        let summary: Vec<(bool, &str, Option<&str>)> = lines
            .iter()
            .map(|r| {
                (
                    r.get("continuation").is_some_and(|c| c == true),
                    r["level"].as_str().unwrap(),
                    r["area"].as_str(),
                )
            })
            .collect();
        assert_eq!(
            summary,
            [
                (false, "info", None),
                (false, "error", Some("Shader")),
                (true, "error", Some("Shader")),
                (true, "error", Some("Shader")),
                (false, "info", Some("Runtime")),
                (true, "info", Some("Runtime")),
            ]
        );
    }

    #[test]
    fn a_line_in_several_chunks_is_one_record_with_the_first_time() {
        let (_dir, mut log, log_path, journal_path) = open_in_tempdir();
        log.write("Texture cache: ");
        std::thread::sleep(Duration::from_millis(50));
        log.write("a very long ");
        log.write("report\r\n");
        log.write("next\n");
        log.finish(Some(0), None);
        let lines = line_records(&journal_path);
        assert_eq!(lines.len(), 2, "{lines:?}");
        assert_eq!(lines[0]["n"], 1);
        assert_eq!(lines[0]["text"], "Texture cache: a very long report");
        assert_eq!(lines[0]["area"], "Texture cache");
        assert_eq!(lines[1]["n"], 2);
        // The second line was stamped at least the pause after the first chunk of the first.
        assert!(
            seconds(&lines[1]) - seconds(&lines[0]) >= 0.045,
            "{lines:?}"
        );
        assert_eq!(
            std::fs::read_to_string(log_path).unwrap(),
            "Texture cache: a very long report\r\nnext\n"
        );
    }

    #[test]
    fn a_final_chunk_without_a_newline_is_flushed_at_the_end() {
        let (_dir, mut log, _, journal_path) = open_in_tempdir();
        log.write("done\n");
        log.write("last words");
        log.finish(Some(3), None);
        let all = records(&journal_path);
        let kinds: Vec<&str> = all.iter().map(|r| r["type"].as_str().unwrap()).collect();
        assert_eq!(kinds, ["start", "line", "line", "exit"]);
        assert_eq!(all[2]["n"], 2);
        assert_eq!(all[2]["text"], "last words");
    }

    #[test]
    fn the_advice_lines_are_launcher_records_numbered_as_in_the_log() {
        let (_dir, mut log, log_path, journal_path) = open_in_tempdir();
        log.write("Runtime: cannot map 6080 MiB of guest memory (error 12)\n");
        log.finish(Some(1), Some("Close other programs and start again."));
        assert_eq!(
            std::fs::read_to_string(&log_path).unwrap(),
            "Runtime: cannot map 6080 MiB of guest memory (error 12)\n\nClose other programs and start again.\n"
        );
        let lines = line_records(&journal_path);
        let summary: Vec<(u64, &str, &str)> = lines
            .iter()
            .map(|r| {
                (
                    r["n"].as_u64().unwrap(),
                    r["source"].as_str().unwrap(),
                    r["text"].as_str().unwrap(),
                )
            })
            .collect();
        assert_eq!(
            summary,
            [
                (
                    1,
                    "game",
                    "Runtime: cannot map 6080 MiB of guest memory (error 12)"
                ),
                (2, "launcher", ""),
                (3, "launcher", "Close other programs and start again."),
            ]
        );
    }

    #[test]
    fn the_advice_ends_a_line_the_game_left_open() {
        let (_dir, mut log, log_path, journal_path) = open_in_tempdir();
        log.write("cut off");
        log.finish(Some(1), Some("Advice."));
        // The advice starts with a newline, which ends the open line, as in the file.
        assert_eq!(
            std::fs::read_to_string(log_path).unwrap(),
            "cut off\nAdvice.\n"
        );
        let lines = line_records(&journal_path);
        let summary: Vec<(u64, &str, &str)> = lines
            .iter()
            .map(|r| {
                (
                    r["n"].as_u64().unwrap(),
                    r["source"].as_str().unwrap(),
                    r["text"].as_str().unwrap(),
                )
            })
            .collect();
        assert_eq!(
            summary,
            [(1, "game", "cut off"), (2, "launcher", "Advice.")]
        );
    }

    #[test]
    fn the_exit_record_has_the_code_and_the_advice() {
        let (_dir, mut log, _, journal_path) = open_in_tempdir();
        log.write("x\n");
        std::thread::sleep(Duration::from_millis(20));
        log.finish(Some(21), Some("Advice."));
        let all = records(&journal_path);
        let exit = all.last().unwrap();
        assert_eq!(exit["type"], "exit");
        assert_eq!(exit["code"], 21);
        assert_eq!(exit["advice"], "Advice.");
        assert!(seconds(exit) >= seconds(&all[1]));
        assert!(chrono::DateTime::parse_from_rfc3339(exit["time"].as_str().unwrap()).is_ok());
    }

    #[test]
    fn the_exit_record_has_null_for_an_unknown_code_and_no_advice() {
        let (_dir, log, _, journal_path) = open_in_tempdir();
        log.finish(None, None);
        let all = records(&journal_path);
        assert_eq!(all.len(), 2, "{all:?}");
        assert_eq!(all[1]["code"], Value::Null);
        assert_eq!(all[1]["advice"], Value::Null);
    }

    /// Accepts `allowed` writes, then fails every one.
    struct FailsAfter {
        allowed: usize,
    }

    impl Write for FailsAfter {
        fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
            if self.allowed == 0 {
                return Err(std::io::Error::other("disk full"));
            }
            self.allowed -= 1;
            Ok(buf.len())
        }

        fn flush(&mut self) -> std::io::Result<()> {
            Ok(())
        }
    }

    #[test]
    fn a_journal_write_error_leaves_the_run_log_intact() {
        let dir = tempfile::tempdir().unwrap();
        let log_path = dir.path().join("last_run.log");
        let journal = RunJournal::new(Box::new(FailsAfter { allowed: 1 })).unwrap();
        let mut log = RunLog {
            file: File::create(&log_path).ok(),
            journal: Some(journal),
        };
        log.write("one\n");
        assert!(
            log.journal.is_none(),
            "the failed write disables the journal"
        );
        log.write("two\n");
        log.finish(Some(1), Some("Advice."));
        assert_eq!(
            std::fs::read_to_string(log_path).unwrap(),
            "one\ntwo\n\nAdvice.\n"
        );
    }

    #[test]
    fn a_journal_that_cannot_be_created_leaves_the_run_log_working() {
        let dir = tempfile::tempdir().unwrap();
        std::fs::create_dir(dir.path().join("last_run.jsonl")).unwrap();
        let log_path = dir.path().join("last_run.log");
        let mut log = RunLog::open(&log_path);
        assert!(log.is_open());
        assert!(log.journal.is_none());
        log.write("kept\n");
        log.finish(Some(0), None);
        assert_eq!(std::fs::read_to_string(log_path).unwrap(), "kept\n");
    }

    #[test]
    fn the_journal_keeps_going_when_the_run_log_cannot_be_created() {
        let dir = tempfile::tempdir().unwrap();
        std::fs::create_dir(dir.path().join("last_run.log")).unwrap();
        let mut log = RunLog::open(&dir.path().join("last_run.log"));
        assert!(!log.is_open());
        log.write(
            "only in the journal
",
        );
        log.finish(Some(0), None);
        let lines = line_records(&dir.path().join("last_run.jsonl"));
        assert_eq!(lines.len(), 1, "{lines:?}");
        assert_eq!(lines[0]["text"], "only in the journal");
    }
}
