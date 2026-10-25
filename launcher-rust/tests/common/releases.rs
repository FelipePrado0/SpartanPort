//! A release host that never reaches GitHub or the browser: it reports each call on a channel and
//! answers from a script, so the UI tests see what the window asked of the host.
#![allow(dead_code)]

use std::sync::mpsc::{self, Receiver, Sender};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use spartanport_launcher::core::release::{Release, ReleaseId};
use spartanport_launcher::core::update::{InstallTarget, ReleaseHost};
use spartanport_launcher::ui::app::{self, Message, State};
use spartanport_launcher::ui::update::{self, UpdateMessage};

/// What the window asked of the host.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Call {
    List,
    /// The package URL of the release.
    Install(String),
    OpenPage(String),
}

pub struct FakeReleases {
    installed: ReleaseId,
    target: InstallTarget,
    list: Result<Vec<Release>, String>,
    install: Result<(), String>,
    progress: u8,
    self_update: bool,
    calls: Mutex<Sender<Call>>,
}

impl FakeReleases {
    /// A packaged Windows install with no releases listed, and the channel its calls arrive on.
    pub fn new() -> (FakeReleases, Receiver<Call>) {
        let (sender, receiver) = mpsc::channel();
        let fake = FakeReleases {
            installed: ReleaseId::fallback(),
            target: InstallTarget::WindowsPackage,
            list: Ok(Vec::new()),
            install: Ok(()),
            progress: 100,
            self_update: true,
            calls: Mutex::new(sender),
        };
        (fake, receiver)
    }

    pub fn installed(mut self, id: ReleaseId) -> Self {
        self.installed = id;
        self
    }

    pub fn target(mut self, target: InstallTarget) -> Self {
        self.target = target;
        self
    }

    pub fn list(mut self, answer: Result<Vec<Release>, String>) -> Self {
        self.list = answer;
        self
    }

    pub fn install(mut self, answer: Result<(), String>) -> Self {
        self.install = answer;
        self
    }

    /// The percent the install reports.
    pub fn progress(mut self, percent: u8) -> Self {
        self.progress = percent;
        self
    }

    /// Whether the install can replace itself; true by default.
    pub fn self_update(mut self, answer: bool) -> Self {
        self.self_update = answer;
        self
    }

    pub fn host(self) -> Arc<dyn ReleaseHost> {
        Arc::new(self)
    }

    fn report(&self, call: Call) {
        let _ = self.calls.lock().unwrap().send(call);
    }
}

impl ReleaseHost for FakeReleases {
    fn installed(&self) -> ReleaseId {
        self.installed.clone()
    }

    fn target(&self) -> InstallTarget {
        self.target.clone()
    }

    fn self_update(&self) -> bool {
        self.self_update
    }

    fn list(&self) -> Result<Vec<Release>, String> {
        self.report(Call::List);
        self.list.clone()
    }

    fn install(&self, release: &Release, progress: &mut dyn FnMut(u8)) -> Result<(), String> {
        // The percent goes first, so a test that saw the call may drain it at once.
        progress(self.progress);
        self.report(Call::Install(release.url.clone()));
        self.install.clone()
    }

    fn open_page(&self, url: &str) {
        self.report(Call::OpenPage(url.to_string()));
    }
}

/// The next call, or a panic after two seconds: the host's calls run on worker threads.
pub fn next_call(calls: &Receiver<Call>) -> Call {
    calls
        .recv_timeout(Duration::from_secs(2))
        .expect("the host was not called")
}

/// A state whose host says `id` is the installed release.
pub fn state_installed(dir: &std::path::Path, id: ReleaseId) -> State {
    super::state_with(dir, FakeReleases::new().0.installed(id).host())
}

/// Feeds `releases` to `state` as the answer of a silent check.
pub fn ready(state: &mut State, releases: Vec<Release>) {
    let result = Ok(releases);
    let _ = app::update(
        state,
        Message::Update(UpdateMessage::Checked {
            manual: false,
            result,
        }),
    );
}

/// A state installed as `installed` that lists `[release]` and downloads it, with `percent` shown.
pub fn downloading_state(
    dir: &std::path::Path,
    installed: ReleaseId,
    release: Release,
    percent: u8,
) -> State {
    let (fake, calls) = FakeReleases::new();
    let fake = fake.installed(installed).progress(percent);
    let mut state = super::state_with(dir, fake.host());
    ready(&mut state, vec![release.clone()]);
    let _ = app::update(
        &mut state,
        Message::Update(UpdateMessage::Confirmed(release.clone())),
    );
    // The fake sent its percent before it reported the call.
    assert_eq!(next_call(&calls), Call::Install(release.url));
    update::drain(&mut state);
    state
}
