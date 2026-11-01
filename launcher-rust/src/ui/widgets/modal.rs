//! Info, error and yes/no dialogs over a dim layer.

use iced::widget::{button, column, container, mouse_area, row, text};
use iced::{Background, Center, Color, Element, Fill, Right};

use crate::core::i18n::tr;
use crate::ui::app::Message;
use crate::ui::theme::{self, palette, CINZEL_BOLD};

/// What a dialog asks.
// The Yes/No answers carry a `Message`, which now holds a `Release`: boxing every answer costs
// more than the bytes.
#[allow(clippy::large_enum_variant)]
#[derive(Debug, Clone)]
pub enum ModalKind {
    Info,
    Error,
    YesNo { yes: Message, no: Message },
}

/// An open dialog.
#[derive(Debug, Clone)]
pub struct Modal {
    pub kind: ModalKind,
    pub title: String,
    pub text: String,
}

impl Modal {
    fn titled(kind: ModalKind, text: String) -> Self {
        Self {
            kind,
            title: crate::APP_NAME.to_string(),
            text,
        }
    }

    /// An OK-only notice.
    pub fn info(text: String) -> Self {
        Self::titled(ModalKind::Info, text)
    }

    /// An OK-only failure notice.
    pub fn error(text: String) -> Self {
        Self::titled(ModalKind::Error, text)
    }

    /// A Yes/No question with explicit answers.
    pub fn ask(text: String, yes: Message, no: Message) -> Self {
        Self::titled(ModalKind::YesNo { yes, no }, text)
    }

    /// A Yes/No question where No just closes the dialog.
    pub fn confirm(text: String, yes: Message) -> Self {
        Self::ask(text, yes, Message::Modal(ModalMessage::Dismiss))
    }
}

/// Dialog events.
#[derive(Debug, Clone)]
pub enum ModalMessage {
    Dismiss,
}

/// Width of the card.
const CARD_WIDTH: f32 = 440.0;

pub fn modal<'a>(kind: ModalKind, title: &str, body: &str, lang: &str) -> Element<'a, Message> {
    let title_color = match kind {
        ModalKind::Error => palette::RED_HI,
        _ => palette::TEXT,
    };
    let ok = || {
        button(text(tr(lang, "OK")))
            .style(theme::button_flat)
            .on_press(Message::Modal(ModalMessage::Dismiss))
    };
    let buttons = match kind {
        ModalKind::Info | ModalKind::Error => row![ok()],
        ModalKind::YesNo { yes, no } => row![
            button(text(tr(lang, "Yes")))
                .style(theme::button_play)
                .on_press(yes),
            button(text(tr(lang, "No")))
                .style(theme::button_flat)
                .on_press(no),
        ],
    }
    .spacing(8);
    let card = container(
        column![
            text(title.to_string())
                .font(CINZEL_BOLD)
                .size(18)
                .color(title_color),
            text(body.to_string()),
            container(buttons).width(Fill).align_x(Right),
        ]
        .spacing(14),
    )
    .padding(20)
    .width(CARD_WIDTH)
    .style(theme::container_card);
    let layer = container(card)
        .width(Fill)
        .height(Fill)
        .align_x(Center)
        .align_y(Center)
        .style(|_| container::Style {
            background: Some(Background::Color(Color {
                a: 0.6,
                ..palette::BG
            })),
            ..container::Style::default()
        });
    // Swallows clicks so nothing under the dialog reacts.
    mouse_area(layer).on_press(Message::Noop).into()
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::APP_NAME;

    #[test]
    fn constructors_title_the_dialog_with_the_app_name() {
        let info = Modal::info("a".to_string());
        assert!(matches!(info.kind, ModalKind::Info));
        let error = Modal::error("b".to_string());
        assert!(matches!(error.kind, ModalKind::Error));
        let ask = Modal::ask("c".to_string(), Message::Noop, Message::Noop);
        assert!(matches!(ask.kind, ModalKind::YesNo { .. }));
        let confirm = Modal::confirm("d".to_string(), Message::Noop);
        match &confirm.kind {
            ModalKind::YesNo { yes, no } => {
                assert!(matches!(yes, Message::Noop));
                assert!(matches!(no, Message::Modal(ModalMessage::Dismiss)));
            }
            other => panic!("expected YesNo, got {other:?}"),
        }
        for (modal, text) in [(&info, "a"), (&error, "b"), (&ask, "c"), (&confirm, "d")] {
            assert_eq!(modal.title, APP_NAME);
            assert_eq!(modal.text, text);
        }
    }
}
