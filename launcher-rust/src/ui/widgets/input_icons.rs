//! Generated from Kenney's Input Prompts (`assets/pads/icons`, see `assets/pads/LICENSE.txt`):
//! the icon of each keyboard, mouse and controller input.

/// An input's icon: white, and red for a pressed input. A mouse icon carries its own red.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Icon {
    pub plain: &'static [u8],
    pub lit: &'static [u8],
}

/// By SDL key or mouse input name.
pub static INPUT_ICONS: &[(&str, Icon)] = &[
    (
        "A",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_a.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_a_lit.png"),
        },
    ),
    (
        "B",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_b.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_b_lit.png"),
        },
    ),
    (
        "C",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_c.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_c_lit.png"),
        },
    ),
    (
        "D",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_d.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_d_lit.png"),
        },
    ),
    (
        "E",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_e.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_e_lit.png"),
        },
    ),
    (
        "F",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_f.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_f_lit.png"),
        },
    ),
    (
        "G",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_g.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_g_lit.png"),
        },
    ),
    (
        "H",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_h.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_h_lit.png"),
        },
    ),
    (
        "I",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_i.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_i_lit.png"),
        },
    ),
    (
        "J",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_j.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_j_lit.png"),
        },
    ),
    (
        "K",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_k.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_k_lit.png"),
        },
    ),
    (
        "L",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_l.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_l_lit.png"),
        },
    ),
    (
        "M",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_m.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_m_lit.png"),
        },
    ),
    (
        "N",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_n.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_n_lit.png"),
        },
    ),
    (
        "O",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_o.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_o_lit.png"),
        },
    ),
    (
        "P",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_p.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_p_lit.png"),
        },
    ),
    (
        "Q",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_q.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_q_lit.png"),
        },
    ),
    (
        "R",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_r.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_r_lit.png"),
        },
    ),
    (
        "S",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_s.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_s_lit.png"),
        },
    ),
    (
        "T",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_t.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_t_lit.png"),
        },
    ),
    (
        "U",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_u.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_u_lit.png"),
        },
    ),
    (
        "V",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_v.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_v_lit.png"),
        },
    ),
    (
        "W",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_w.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_w_lit.png"),
        },
    ),
    (
        "X",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_x.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_x_lit.png"),
        },
    ),
    (
        "Y",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_y.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_y_lit.png"),
        },
    ),
    (
        "Z",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_z.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_z_lit.png"),
        },
    ),
    (
        "0",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_0.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_0_lit.png"),
        },
    ),
    (
        "1",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_1.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_1_lit.png"),
        },
    ),
    (
        "2",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_2.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_2_lit.png"),
        },
    ),
    (
        "3",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_3.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_3_lit.png"),
        },
    ),
    (
        "4",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_4.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_4_lit.png"),
        },
    ),
    (
        "5",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_5.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_5_lit.png"),
        },
    ),
    (
        "6",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_6.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_6_lit.png"),
        },
    ),
    (
        "7",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_7.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_7_lit.png"),
        },
    ),
    (
        "8",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_8.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_8_lit.png"),
        },
    ),
    (
        "9",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_9.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_9_lit.png"),
        },
    ),
    (
        "F1",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_f1.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_f1_lit.png"),
        },
    ),
    (
        "F2",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_f2.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_f2_lit.png"),
        },
    ),
    (
        "F3",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_f3.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_f3_lit.png"),
        },
    ),
    (
        "F4",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_f4.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_f4_lit.png"),
        },
    ),
    (
        "F5",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_f5.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_f5_lit.png"),
        },
    ),
    (
        "F6",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_f6.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_f6_lit.png"),
        },
    ),
    (
        "F7",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_f7.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_f7_lit.png"),
        },
    ),
    (
        "F8",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_f8.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_f8_lit.png"),
        },
    ),
    (
        "F9",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_f9.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_f9_lit.png"),
        },
    ),
    (
        "F10",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_f10.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_f10_lit.png"),
        },
    ),
    (
        "F11",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_f11.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_f11_lit.png"),
        },
    ),
    (
        "F12",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_f12.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_f12_lit.png"),
        },
    ),
    (
        "Escape",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_escape.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_escape_lit.png"),
        },
    ),
    (
        "Tab",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_tab.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_tab_lit.png"),
        },
    ),
    (
        "CapsLock",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_capslock.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_capslock_lit.png"),
        },
    ),
    (
        "Left Shift",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_shift.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_shift_lit.png"),
        },
    ),
    (
        "Right Shift",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_shift.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_shift_lit.png"),
        },
    ),
    (
        "Left Ctrl",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_ctrl.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_ctrl_lit.png"),
        },
    ),
    (
        "Right Ctrl",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_ctrl.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_ctrl_lit.png"),
        },
    ),
    (
        "Left Alt",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_alt.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_alt_lit.png"),
        },
    ),
    (
        "Right Alt",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_alt.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_alt_lit.png"),
        },
    ),
    (
        "Left GUI",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_win.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_win_lit.png"),
        },
    ),
    (
        "Space",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_space.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_space_lit.png"),
        },
    ),
    (
        "Return",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_return.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_return_lit.png"),
        },
    ),
    (
        "Backspace",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_backspace.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_backspace_lit.png"),
        },
    ),
    (
        "Delete",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_delete.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_delete_lit.png"),
        },
    ),
    (
        "Insert",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_insert.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_insert_lit.png"),
        },
    ),
    (
        "Home",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_home.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_home_lit.png"),
        },
    ),
    (
        "End",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_end.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_end_lit.png"),
        },
    ),
    (
        "PageUp",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_page_up.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_page_up_lit.png"),
        },
    ),
    (
        "PageDown",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_page_down.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_page_down_lit.png"),
        },
    ),
    (
        "Up",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_arrow_up.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_arrow_up_lit.png"),
        },
    ),
    (
        "Down",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_arrow_down.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_arrow_down_lit.png"),
        },
    ),
    (
        "Left",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_arrow_left.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_arrow_left_lit.png"),
        },
    ),
    (
        "Right",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_arrow_right.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_arrow_right_lit.png"),
        },
    ),
    (
        "-",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_minus.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_minus_lit.png"),
        },
    ),
    (
        "Equals",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_equals.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_equals_lit.png"),
        },
    ),
    (
        "[",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_bracket_open.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_bracket_open_lit.png"),
        },
    ),
    (
        "]",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_bracket_close.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_bracket_close_lit.png"),
        },
    ),
    (
        "\\",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_slash_back.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_slash_back_lit.png"),
        },
    ),
    (
        "/",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_slash_forward.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_slash_forward_lit.png"),
        },
    ),
    (
        ";",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_semicolon.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_semicolon_lit.png"),
        },
    ),
    (
        "'",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_apostrophe.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_apostrophe_lit.png"),
        },
    ),
    (
        "Comma",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_comma.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_comma_lit.png"),
        },
    ),
    (
        ".",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_period.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_period_lit.png"),
        },
    ),
    (
        "`",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_tilde.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_tilde_lit.png"),
        },
    ),
    (
        "Numlock",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_numlock.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_numlock_lit.png"),
        },
    ),
    (
        "ScrollLock",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_scroll_lock.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_scroll_lock_lit.png"),
        },
    ),
    (
        "Pause",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_pause.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_pause_lit.png"),
        },
    ),
    (
        "PrintScreen",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_printscreen.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_printscreen_lit.png"),
        },
    ),
    (
        "Keypad Enter",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_numpad_enter.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_numpad_enter_lit.png"),
        },
    ),
    (
        "Keypad +",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/keys/keyboard_numpad_plus.png"),
            lit: include_bytes!("../../../assets/pads/icons/keys/keyboard_numpad_plus_lit.png"),
        },
    ),
    (
        "Mouse Left",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/mouse/mouse_left.png"),
            lit: include_bytes!("../../../assets/pads/icons/mouse/mouse_left.png"),
        },
    ),
    (
        "Mouse Right",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/mouse/mouse_right.png"),
            lit: include_bytes!("../../../assets/pads/icons/mouse/mouse_right.png"),
        },
    ),
    (
        "Mouse Middle",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/mouse/mouse_scroll.png"),
            lit: include_bytes!("../../../assets/pads/icons/mouse/mouse_scroll.png"),
        },
    ),
    (
        "Wheel Up",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/mouse/mouse_scroll_up.png"),
            lit: include_bytes!("../../../assets/pads/icons/mouse/mouse_scroll_up.png"),
        },
    ),
    (
        "Wheel Down",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/mouse/mouse_scroll_down.png"),
            lit: include_bytes!("../../../assets/pads/icons/mouse/mouse_scroll_down.png"),
        },
    ),
    (
        "Mouse Back",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/mouse/mouse_side_back.png"),
            lit: include_bytes!("../../../assets/pads/icons/mouse/mouse_side_back.png"),
        },
    ),
    (
        "Mouse Forward",
        Icon {
            plain: include_bytes!("../../../assets/pads/icons/mouse/mouse_side_forward.png"),
            lit: include_bytes!("../../../assets/pads/icons/mouse/mouse_side_forward.png"),
        },
    ),
];

pub static MOUSE_TOP: &[u8] = include_bytes!("../../../assets/pads/icons/mouse/mouse.png");
pub static MOUSE_SIDE: &[u8] = include_bytes!("../../../assets/pads/icons/mouse/mouse_side.png");

/// By SDL gamepad input name.
pub static PLAYSTATION5: &[(&str, &[u8])] = &[
    (
        "a",
        include_bytes!("../../../assets/pads/icons/playstation/playstation_button_color_cross.png"),
    ),
    (
        "b",
        include_bytes!(
            "../../../assets/pads/icons/playstation/playstation_button_color_circle.png"
        ),
    ),
    (
        "x",
        include_bytes!(
            "../../../assets/pads/icons/playstation/playstation_button_color_square.png"
        ),
    ),
    (
        "y",
        include_bytes!(
            "../../../assets/pads/icons/playstation/playstation_button_color_triangle.png"
        ),
    ),
    (
        "leftshoulder",
        include_bytes!("../../../assets/pads/icons/playstation/playstation_trigger_l1.png"),
    ),
    (
        "rightshoulder",
        include_bytes!("../../../assets/pads/icons/playstation/playstation_trigger_r1.png"),
    ),
    (
        "lefttrigger",
        include_bytes!("../../../assets/pads/icons/playstation/playstation_trigger_l2.png"),
    ),
    (
        "righttrigger",
        include_bytes!("../../../assets/pads/icons/playstation/playstation_trigger_r2.png"),
    ),
    (
        "leftstick",
        include_bytes!("../../../assets/pads/icons/playstation/playstation_button_l3.png"),
    ),
    (
        "rightstick",
        include_bytes!("../../../assets/pads/icons/playstation/playstation_button_r3.png"),
    ),
    (
        "dpup",
        include_bytes!("../../../assets/pads/icons/playstation/playstation_dpad_up.png"),
    ),
    (
        "dpdown",
        include_bytes!("../../../assets/pads/icons/playstation/playstation_dpad_down.png"),
    ),
    (
        "dpleft",
        include_bytes!("../../../assets/pads/icons/playstation/playstation_dpad_left.png"),
    ),
    (
        "dpright",
        include_bytes!("../../../assets/pads/icons/playstation/playstation_dpad_right.png"),
    ),
    (
        "back",
        include_bytes!("../../../assets/pads/icons/playstation/playstation5_button_create.png"),
    ),
    (
        "start",
        include_bytes!("../../../assets/pads/icons/playstation/playstation5_button_options.png"),
    ),
    (
        "touchpad",
        include_bytes!("../../../assets/pads/icons/playstation/playstation5_touchpad_press.png"),
    ),
    (
        "misc1",
        include_bytes!("../../../assets/pads/icons/playstation/playstation5_button_mute.png"),
    ),
];

/// By SDL gamepad input name.
pub static PLAYSTATION4: &[(&str, &[u8])] = &[
    (
        "back",
        include_bytes!("../../../assets/pads/icons/playstation/playstation4_button_share.png"),
    ),
    (
        "start",
        include_bytes!("../../../assets/pads/icons/playstation/playstation4_button_options.png"),
    ),
    (
        "touchpad",
        include_bytes!("../../../assets/pads/icons/playstation/playstation4_touchpad_press.png"),
    ),
];

/// By SDL gamepad input name.
pub static XBOX: &[(&str, &[u8])] = &[
    (
        "a",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_button_color_a.png"),
    ),
    (
        "b",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_button_color_b.png"),
    ),
    (
        "x",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_button_color_x.png"),
    ),
    (
        "y",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_button_color_y.png"),
    ),
    (
        "leftshoulder",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_lb.png"),
    ),
    (
        "rightshoulder",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_rb.png"),
    ),
    (
        "lefttrigger",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_lt.png"),
    ),
    (
        "righttrigger",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_rt.png"),
    ),
    (
        "leftstick",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_ls.png"),
    ),
    (
        "rightstick",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_rs.png"),
    ),
    (
        "dpup",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_dpad_up.png"),
    ),
    (
        "dpdown",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_dpad_down.png"),
    ),
    (
        "dpleft",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_dpad_left.png"),
    ),
    (
        "dpright",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_dpad_right.png"),
    ),
    (
        "back",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_button_view.png"),
    ),
    (
        "start",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_button_menu.png"),
    ),
    (
        "guide",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_guide.png"),
    ),
    (
        "misc1",
        include_bytes!("../../../assets/pads/icons/xbox/xbox_button_share.png"),
    ),
];

/// By SDL gamepad input name.
pub static SWITCH: &[(&str, &[u8])] = &[
    (
        "a",
        include_bytes!("../../../assets/pads/icons/switch/switch_button_b.png"),
    ),
    (
        "b",
        include_bytes!("../../../assets/pads/icons/switch/switch_button_a.png"),
    ),
    (
        "x",
        include_bytes!("../../../assets/pads/icons/switch/switch_button_y.png"),
    ),
    (
        "y",
        include_bytes!("../../../assets/pads/icons/switch/switch_button_x.png"),
    ),
    (
        "leftshoulder",
        include_bytes!("../../../assets/pads/icons/switch/switch_button_l.png"),
    ),
    (
        "rightshoulder",
        include_bytes!("../../../assets/pads/icons/switch/switch_button_r.png"),
    ),
    (
        "lefttrigger",
        include_bytes!("../../../assets/pads/icons/switch/switch_button_zl.png"),
    ),
    (
        "righttrigger",
        include_bytes!("../../../assets/pads/icons/switch/switch_button_zr.png"),
    ),
    (
        "leftstick",
        include_bytes!("../../../assets/pads/icons/switch/switch_stick_l_press.png"),
    ),
    (
        "rightstick",
        include_bytes!("../../../assets/pads/icons/switch/switch_stick_r_press.png"),
    ),
    (
        "dpup",
        include_bytes!("../../../assets/pads/icons/switch/switch_dpad_up.png"),
    ),
    (
        "dpdown",
        include_bytes!("../../../assets/pads/icons/switch/switch_dpad_down.png"),
    ),
    (
        "dpleft",
        include_bytes!("../../../assets/pads/icons/switch/switch_dpad_left.png"),
    ),
    (
        "dpright",
        include_bytes!("../../../assets/pads/icons/switch/switch_dpad_right.png"),
    ),
    (
        "back",
        include_bytes!("../../../assets/pads/icons/switch/switch_button_minus.png"),
    ),
    (
        "start",
        include_bytes!("../../../assets/pads/icons/switch/switch_button_plus.png"),
    ),
    (
        "guide",
        include_bytes!("../../../assets/pads/icons/switch/switch_button_home.png"),
    ),
    (
        "misc1",
        include_bytes!("../../../assets/pads/icons/switch/switch_button_sync.png"),
    ),
];
