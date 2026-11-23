#![allow(clippy::unwrap_used, clippy::expect_used)]

use indexmap::IndexMap;
use spartanport_launcher::core::launch::{
    run_command, spawn_game, Exit, GameEvent, GameHandle, Group, Launch,
};
use spartanport_launcher::core::log::{ExitHint, RunLog};
use std::ffi::OsString;
use std::path::Path;
use std::sync::mpsc::RecvTimeoutError;
use std::time::{Duration, Instant};

const BOUND: Duration = Duration::from_secs(10);

fn start_in(args: &[&str], log: RunLog, group: Group) -> GameHandle {
    let mut command = vec![env!("CARGO_BIN_EXE_fake-game").to_string()];
    command.extend(args.iter().map(|a| a.to_string()));
    let mut env = IndexMap::new();
    if let Some(root) = std::env::var_os("SystemRoot") {
        env.insert(
            "SystemRoot".to_string(),
            root.to_string_lossy().into_owned(),
        );
    }
    spawn_game(Launch {
        command: &command,
        cwd: Path::new("."),
        env: &env,
        run_log: log,
        lang: "en",
        group,
    })
    .unwrap()
}

fn start(args: &[&str], log: RunLog) -> GameHandle {
    start_in(args, log, Group::Own)
}

/// Every event until `Exited`, each with the time it arrived.
fn collect(handle: &GameHandle) -> Vec<(GameEvent, Instant)> {
    let mut events = Vec::new();
    loop {
        match handle.events.recv_timeout(BOUND) {
            Ok(event) => {
                let done = matches!(event, GameEvent::Exited(_));
                events.push((event, Instant::now()));
                if done {
                    return events;
                }
            }
            Err(RecvTimeoutError::Timeout) => panic!("no Exited event in time"),
            Err(RecvTimeoutError::Disconnected) => panic!("channel closed before Exited"),
        }
    }
}

fn texts(events: &[(GameEvent, Instant)]) -> Vec<String> {
    events
        .iter()
        .filter_map(|(e, _)| match e {
            GameEvent::Output(t) => Some(t.replace("\r\n", "\n")),
            GameEvent::Exited(_) => None,
        })
        .collect()
}

#[test]
fn every_line_arrives_then_the_exit_code() {
    let handle = start(
        &["--lines", "5", "--delay-ms", "0", "--exit", "3"],
        RunLog::open(Path::new("")),
    );
    let events = collect(&handle);
    let expected: Vec<String> = (1..=5).map(|k| format!("line {k}\n")).collect();
    assert_eq!(texts(&events), expected);
    assert!(matches!(
        &events.last().unwrap().0,
        GameEvent::Exited(Exit { code: 3, .. })
    ));
}

#[test]
fn exit_arrives_after_stdout_closed_early() {
    let handle = start(
        &[
            "--lines",
            "2",
            "--silent-after",
            "2",
            "--exit",
            "4",
            "--delay-ms",
            "0",
        ],
        RunLog::open(Path::new("")),
    );
    let events = collect(&handle);
    let (last, exited_at) = events.last().unwrap();
    assert!(matches!(last, GameEvent::Exited(Exit { code: 4, .. })));
    let second_line_at = events
        .iter()
        .filter(|(e, _)| matches!(e, GameEvent::Output(_)))
        .map(|(_, t)| *t)
        .next_back()
        .unwrap();
    assert!(exited_at.duration_since(second_line_at) >= Duration::from_millis(400));
}

#[test]
fn terminate_ends_a_running_child_and_reports_exit() {
    let mut handle = start(
        &["--lines", "1000", "--delay-ms", "50", "--exit", "0"],
        RunLog::open(Path::new("")),
    );
    match handle.events.recv_timeout(BOUND).unwrap() {
        GameEvent::Output(_) => {}
        GameEvent::Exited(exit) => panic!("exited early with {}", exit.code),
    }
    handle.terminate();
    let events = collect(&handle);
    match &events.last().unwrap().0 {
        GameEvent::Exited(exit) => assert_eq!(exit.code, if cfg!(windows) { 1 } else { -15 }),
        GameEvent::Output(_) => unreachable!(),
    }
    handle.close();
}

#[cfg(unix)]
#[test]
fn a_signal_exit_is_minus_the_signal() {
    let command = ["sh", "-c", "kill -9 $$"].map(String::from);
    let handle = spawn_game(Launch {
        command: &command,
        cwd: Path::new("."),
        env: &IndexMap::new(),
        run_log: RunLog::open(Path::new("")),
        lang: "en",
        group: Group::Own,
    })
    .unwrap();
    match collect(&handle).last().unwrap().0 {
        GameEvent::Exited(exit) => assert_eq!(exit.code, -9),
        GameEvent::Output(_) => unreachable!(),
    }
}

#[test]
fn run_log_holds_every_line() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("last_run.log");
    let handle = start(
        &["--lines", "5", "--delay-ms", "0", "--exit", "0"],
        RunLog::open(&path),
    );
    collect(&handle);
    let text = std::fs::read_to_string(&path)
        .unwrap()
        .replace("\r\n", "\n");
    let expected: String = (1..=5).map(|k| format!("line {k}\n")).collect();
    assert_eq!(text, expected);
}

const MEMORY_LINE: &str = "mmap failed of guest memory (error 1455)";

/// The exit of a run that said `line` and ended with `code`, and what `last_run.log` holds.
fn say_and_exit(line: &str, code: &str) -> (Exit, String) {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("last_run.log");
    let handle = start(&["--say", line, "--exit", code], RunLog::open(&path));
    let exit = match collect(&handle).pop().unwrap().0 {
        GameEvent::Exited(exit) => exit,
        GameEvent::Output(_) => unreachable!(),
    };
    let log = std::fs::read_to_string(&path)
        .unwrap()
        .replace("\r\n", "\n");
    (exit, log)
}

fn advice() -> &'static str {
    if cfg!(windows) {
        ExitHint::GuestMemory.text_en()
    } else {
        ExitHint::LinuxMemory.text_en()
    }
}

#[test]
fn the_last_hint_line_reaches_the_exit() {
    let (exit, _) = say_and_exit(MEMORY_LINE, "1");
    assert_eq!(exit.code, 1);
    let expected = if cfg!(windows) {
        ExitHint::GuestMemory
    } else {
        ExitHint::LinuxMemory
    };
    assert_eq!(exit.hint, Some(expected));
}

#[test]
fn a_failed_exit_writes_the_advice_to_the_run_log() {
    let (_, log) = say_and_exit(MEMORY_LINE, "1");
    assert_eq!(log, format!("{MEMORY_LINE}\n\n{}\n", advice()));
}

#[test]
fn a_clean_exit_writes_no_advice() {
    let (exit, log) = say_and_exit(MEMORY_LINE, "0");
    assert_eq!(
        exit.hint,
        Some(if cfg!(windows) {
            ExitHint::GuestMemory
        } else {
            ExitHint::LinuxMemory
        })
    );
    assert_eq!(log, format!("{MEMORY_LINE}\n"));
}

/// The records of the `last_run.jsonl` next to `log_path`.
fn journal_records(log_path: &Path) -> Vec<serde_json::Value> {
    std::fs::read_to_string(log_path.with_extension("jsonl"))
        .unwrap()
        .lines()
        .map(|line| serde_json::from_str(line).unwrap())
        .collect()
}

/// Runs the fake game with `args`, then returns `last_run.log` and the records of `last_run.jsonl`.
fn run_and_read_both(args: &[&str]) -> (String, Vec<serde_json::Value>) {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("last_run.log");
    let handle = start(args, RunLog::open(&path));
    collect(&handle);
    let log = std::fs::read_to_string(&path).unwrap();
    (log, journal_records(&path))
}

/// The `n` and `text` of every `line` record.
fn numbered_lines(records: &[serde_json::Value]) -> Vec<(u64, String)> {
    records
        .iter()
        .filter(|record| record["type"] == "line")
        .map(|record| {
            (
                record["n"].as_u64().unwrap(),
                record["text"].as_str().unwrap().to_string(),
            )
        })
        .collect()
}

#[test]
fn the_journal_reproduces_the_run_log_line_by_line() {
    let (log, records) = run_and_read_both(&[
        "--say",
        "Texture cache: memory pressure, evicted 3 images",
        "--lines",
        "3",
        "--delay-ms",
        "0",
        "--exit",
        "0",
    ]);
    let expected: Vec<(u64, String)> = log
        .lines()
        .enumerate()
        .map(|(index, line)| (index as u64 + 1, line.to_string()))
        .collect();
    assert_eq!(expected.len(), 4, "{log}");
    assert_eq!(numbered_lines(&records), expected);
    assert_eq!(records.first().unwrap()["type"], "start");
    let exit = records.last().unwrap();
    assert_eq!(exit["type"], "exit");
    assert_eq!(exit["code"], 0);
    assert_eq!(exit["advice"], serde_json::Value::Null);
    assert_eq!(records[1]["area"], "Texture cache");
    assert_eq!(records[1]["source"], "game");
}

#[test]
fn a_failed_run_puts_its_advice_and_code_in_the_journal() {
    let (log, records) = run_and_read_both(&["--say", MEMORY_LINE, "--exit", "1"]);
    let expected: Vec<(u64, String)> = log
        .lines()
        .enumerate()
        .map(|(index, line)| (index as u64 + 1, line.to_string()))
        .collect();
    assert_eq!(numbered_lines(&records), expected);
    let launcher_lines: Vec<&str> = records
        .iter()
        .filter(|record| record["source"] == "launcher")
        .map(|record| record["text"].as_str().unwrap())
        .collect();
    assert_eq!(launcher_lines, ["", advice()]);
    let exit = records.last().unwrap();
    assert_eq!(exit["type"], "exit");
    assert_eq!(exit["code"], 1);
    assert_eq!(exit["advice"], advice());
}

/// The process group of `pid`, from `ps`.
#[cfg(unix)]
fn group_of(pid: u32) -> u32 {
    let out = std::process::Command::new("ps")
        .args(["-o", "pgid=", "-p", &pid.to_string()])
        .output()
        .unwrap();
    String::from_utf8_lossy(&out.stdout).trim().parse().unwrap()
}

#[cfg(unix)]
#[test]
fn caller_group_keeps_the_process_group() {
    let mut handle = start_in(
        &["--lines", "1000", "--delay-ms", "50", "--exit", "0"],
        RunLog::open(Path::new("")),
        Group::Caller,
    );
    assert_eq!(group_of(handle.pid), group_of(std::process::id()));
    // No group of its own: stopping it is the caller's business.
    handle.terminate();
    let _ = std::process::Command::new("kill")
        .arg(handle.pid.to_string())
        .status();
    collect(&handle);
}

#[cfg(unix)]
#[test]
fn own_group_has_its_own() {
    let mut handle = start(
        &["--lines", "1000", "--delay-ms", "50", "--exit", "0"],
        RunLog::open(Path::new("")),
    );
    assert_eq!(group_of(handle.pid), handle.pid);
    handle.terminate();
    collect(&handle);
}

#[test]
fn the_game_command_is_the_exe_with_run() {
    let exe = Path::new("port").join("spartanport-launcher");
    assert_eq!(
        run_command(&exe, &[]),
        vec![exe.to_string_lossy().into_owned(), "--run".to_string()]
    );
    let args = [OsString::from("--fullscreen"), OsString::from("two words")];
    assert_eq!(
        run_command(&exe, &args),
        vec![
            exe.to_string_lossy().into_owned(),
            "--run".to_string(),
            "--fullscreen".to_string(),
            "two words".to_string(),
        ]
    );
}
