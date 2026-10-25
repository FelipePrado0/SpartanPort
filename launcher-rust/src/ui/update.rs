//! The release list and the install of any listed release (spec 5.6).
//!
//! Progress: the download runs on a worker thread that sends percentages through an `mpsc`
//! channel kept in the downloading phase of the `Updater`; the app clock (`Message::Tick`) drains it with
//! [`drain`], as it does for the game's output.

use std::path::PathBuf;
use std::sync::mpsc::{self, Receiver};
use std::sync::Arc;

use iced::Task;

use crate::core::i18n::{tr, tr_fmt};
use crate::core::release::{newer_stable, version_tuple, Release, ReleaseId};
use crate::core::update::{InstallTarget, ReleaseHost};
use crate::ui::app::{Message, State};
use crate::ui::play::{self, WORKER_LOST};
use crate::ui::splash::{Check, SplashMessage};
use crate::ui::tasks::{blocking, blocking_or};
use crate::ui::widgets::Modal;

/// Whether the release list is on its way, failed or here.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Listing {
    /// Not asked yet, or a check is running.
    Fetching,
    Failed,
    Listed,
}

/// Where the update stands.
enum Phase {
    Idle,
    /// A check is running.
    Checking,
    /// The releases of this system, newest first.
    Ready(Vec<Release>),
    Failed(String),
    /// `release`'s package is downloading; the list stays. `progress` carries the percentages the
    /// worker sends; [`drain`] empties it on each clock tick.
    Downloading {
        releases: Vec<Release>,
        release: Release,
        percent: u8,
        progress: Receiver<u8>,
    },
}

/// The update flow: the host, the installed release and where the check or download stands.
pub struct Updater {
    host: Arc<dyn ReleaseHost>,
    installed: ReleaseId,
    self_update: bool,
    phase: Phase,
}

impl Updater {
    /// Reads the installed release and whether this install can update itself, once.
    pub fn new(host: Arc<dyn ReleaseHost>) -> Self {
        Updater {
            installed: host.installed(),
            self_update: host.self_update(),
            host,
            phase: Phase::Idle,
        }
    }

    /// The listed releases: those of `Ready` or `Downloading`, else none.
    pub fn releases(&self) -> &[Release] {
        match &self.phase {
            Phase::Ready(releases) | Phase::Downloading { releases, .. } => releases,
            _ => &[],
        }
    }

    /// The newest listed stable above the installed release.
    pub fn newer(&self) -> Option<&Release> {
        newer_stable(self.releases(), &self.installed)
    }

    /// The installed release: the stamp beside the launcher, or this version.
    pub fn installed(&self) -> &ReleaseId {
        &self.installed
    }

    /// The release being downloaded and its percent.
    pub fn downloading(&self) -> Option<(&Release, u8)> {
        match &self.phase {
            Phase::Downloading {
                release, percent, ..
            } => Some((release, *percent)),
            _ => None,
        }
    }

    pub fn listing(&self) -> Listing {
        match &self.phase {
            Phase::Idle | Phase::Checking => Listing::Fetching,
            Phase::Failed(_) => Listing::Failed,
            Phase::Ready(_) | Phase::Downloading { .. } => Listing::Listed,
        }
    }

    /// Why the last check failed, while the list is not there.
    pub fn error(&self) -> Option<&str> {
        match &self.phase {
            Phase::Failed(error) => Some(error),
            _ => None,
        }
    }

    /// Whether this install can replace itself (Windows, or an AppImage).
    pub fn self_update(&self) -> bool {
        self.self_update
    }

    /// Whether nothing was listed yet or the last check failed: opening the menu asks again.
    pub fn stale(&self) -> bool {
        matches!(self.phase, Phase::Idle | Phase::Failed(_))
    }
}

/// Update events.
#[derive(Debug, Clone)]
pub enum UpdateMessage {
    /// A check: the Check for updates button (`manual`) or a silent one.
    Check {
        manual: bool,
    },
    /// The answer of a check; `manual` is what the check was started as.
    Checked {
        manual: bool,
        result: Result<Vec<Release>, String>,
    },
    /// A listed release was picked.
    Install(Release),
    /// The dialog's Yes to a read-only AppImage folder: open this release page.
    OpenPage(String),
    /// The dialog's Yes to the install.
    Confirmed(Release),
    Progress(u8),
    Done(Result<(), String>),
}

/// What a finished check means for the user.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Decision {
    /// A newer stable is listed.
    Offer,
    /// Manual check: nothing newer.
    Latest,
    Nothing,
}

/// Keeps `releases` as the list; a running download keeps its state.
pub fn ready(state: &mut State, releases: Vec<Release>) {
    if state.updater.downloading().is_none() {
        state.updater.phase = Phase::Ready(releases);
    }
}

/// Whether `release`'s version is below the installed one.
fn is_older(release: &Release, installed: &ReleaseId) -> bool {
    version_tuple(&release.id.version) < version_tuple(&installed.version)
}

/// The install question, with a warning when `release` is older than the installed one.
fn confirm_text(lang: &str, release: &Release, installed: &ReleaseId) -> String {
    let mut text = tr_fmt(
        lang,
        "Install version {} now? The launcher closes, installs it and opens again. Saves and settings are kept.",
        &[&release.id.short()],
    );
    if is_older(release, installed) {
        text.push('\n');
        text.push_str(&tr(lang, "This version is older than the installed one."));
    }
    text
}

/// Python `check_update`'s `show`, over the list.
fn offer_decision(newer: bool, manual: bool) -> Decision {
    if newer {
        Decision::Offer
    } else if manual {
        Decision::Latest
    } else {
        Decision::Nothing
    }
}

/// What installing a release does (Python `install_update`, before the download).
#[derive(Debug, Clone, PartialEq, Eq)]
enum InstallPlan {
    Blocked,
    /// Download by hand: open the release page.
    OpenPage,
    /// The AppImage's folder cannot be written; offer the page.
    ReadOnly(PathBuf),
    /// Ask, then download and install.
    Confirm,
}

/// Decides an install from the host's target.
fn install_decision(busy: bool, target: &InstallTarget) -> InstallPlan {
    if busy {
        return InstallPlan::Blocked;
    }
    match target {
        InstallTarget::WindowsPackage => InstallPlan::Confirm,
        InstallTarget::AppImage { writable: true, .. } => InstallPlan::Confirm,
        InstallTarget::AppImage { folder, .. } => InstallPlan::ReadOnly(folder.clone()),
        InstallTarget::WindowsSource | InstallTarget::Elsewhere => InstallPlan::OpenPage,
    }
}

fn open_page(state: &State, page: String) -> Task<Message> {
    let host = state.updater.host.clone();
    blocking(move || host.open_page(&page)).map(|()| Message::Noop)
}

/// Applies an update event.
pub fn update(state: &mut State, message: UpdateMessage) -> Task<Message> {
    let lang = state.lang.clone();
    match message {
        UpdateMessage::Check { manual } => {
            if state.updater.stale() {
                state.updater.phase = Phase::Checking;
            }
            let host = state.updater.host.clone();
            blocking(move || host.list())
                .map(move |result| Message::Update(UpdateMessage::Checked { manual, result }))
        }
        UpdateMessage::Checked { manual, result } => {
            match result {
                Ok(releases) => {
                    ready(state, releases);
                    if offer_decision(state.updater.newer().is_some(), manual) == Decision::Latest {
                        let text = tr_fmt(
                            &lang,
                            "You have the latest version ({}).",
                            &[&state.updater.installed.version],
                        );
                        state.modal = Some(Modal::info(text));
                    }
                }
                Err(error) => {
                    // A list already shown and a running download stay.
                    if state.updater.listing() != Listing::Listed {
                        state.updater.phase = Phase::Failed(error.clone());
                    }
                    if manual {
                        let text = tr_fmt(&lang, "Could not check for updates: {}", &[&error]);
                        state.modal = Some(Modal::error(text));
                    }
                }
            }
            Task::none()
        }
        UpdateMessage::Install(release) => {
            if state.updater.downloading().is_some()
                || release.id.same_build(&state.updater.installed)
            {
                return Task::none();
            }
            let plan = install_decision(state.game.is_live(), &state.updater.host.target());
            match plan {
                InstallPlan::Blocked => {
                    let text = tr(&lang, "Close the game before updating.");
                    state.modal = Some(Modal::info(text));
                    Task::none()
                }
                InstallPlan::OpenPage => open_page(state, release.page),
                InstallPlan::ReadOnly(folder) => {
                    let text = tr_fmt(
                        &lang,
                        "The launcher cannot write to {}, so it cannot replace itself there. Open the releases page to download the new version by hand?",
                        &[&folder.display().to_string()],
                    );
                    let yes = Message::Update(UpdateMessage::OpenPage(release.page));
                    state.modal = Some(Modal::confirm(text, yes));
                    Task::none()
                }
                InstallPlan::Confirm => {
                    let text = confirm_text(&lang, &release, &state.updater.installed);
                    let yes = Message::Update(UpdateMessage::Confirmed(release));
                    state.modal = Some(Modal::confirm(text, yes));
                    Task::none()
                }
            }
        }
        UpdateMessage::OpenPage(page) => {
            state.modal = None;
            open_page(state, page)
        }
        UpdateMessage::Confirmed(release) => {
            state.modal = None;
            // The question may have been asked before the game started.
            if play::refuse_while_live(state, play::LiveAction::Update) {
                return Task::none();
            }
            let (sender, receiver) = mpsc::channel();
            state.updater.phase = Phase::Downloading {
                releases: state.updater.releases().to_vec(),
                release: release.clone(),
                percent: 0,
                progress: receiver,
            };
            let host = state.updater.host.clone();
            // A download that dies still ends `Downloading`, so the list comes back.
            blocking_or(
                move || {
                    host.install(&release, &mut |percent| {
                        let _ = sender.send(percent);
                    })
                },
                Err(WORKER_LOST.to_string()),
            )
            .map(|result| Message::Update(UpdateMessage::Done(result)))
        }
        UpdateMessage::Progress(percent) => {
            if let Phase::Downloading { percent: shown, .. } = &mut state.updater.phase {
                *shown = percent;
            }
            Task::none()
        }
        UpdateMessage::Done(Ok(())) => iced::exit(),
        UpdateMessage::Done(Err(error)) => {
            if state.updater.downloading().is_some() {
                // The list comes back.
                let phase = std::mem::replace(&mut state.updater.phase, Phase::Idle);
                if let Phase::Downloading { releases, .. } = phase {
                    state.updater.phase = Phase::Ready(releases);
                }
            }
            let text = tr_fmt(&lang, "Update failed: {}", &[&error]);
            state.modal = Some(Modal::error(text));
            Task::none()
        }
    }
}

/// Applies the percentages the worker sent since the last clock tick.
pub fn drain(state: &mut State) {
    let Phase::Downloading { progress, .. } = &state.updater.phase else {
        return;
    };
    let latest = progress.try_iter().last();
    if let Some(percent) = latest {
        let _ = update(state, UpdateMessage::Progress(percent));
    }
}

/// The update check of the splash: the host lists the releases when this install can update
/// itself and the setting asks for it; `Check::Update` carries the answer.
pub fn start_up_check(state: &State) -> Task<Message> {
    let done = |releases| Message::Splash(SplashMessage::CheckDone(Check::Update(releases)));
    if state.updater.self_update() && state.store.flag("check_updates") {
        let host = state.updater.host.clone();
        blocking(move || host.list().ok()).map(done)
    } else {
        Task::done(done(None))
    }
}
