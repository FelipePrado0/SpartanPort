/* SPARTAN_PAD_MAP / SPARTAN_KEY_MAP parsing (src/runtime_pad.c). */
#define _GNU_SOURCE
#include <assert.h>
#include "../src/runtime_pad.c"

int spartanportgpu_overlay_captures_input(void) { return 0; }
int spartanportgpu_vibration_percent(void) { return 100; }
int spartanportgpu_mouse_prompt_distance(void) { return 48; }
static int pause_keys;
void spartanportgpu_mouse_pause_key(void) { ++pause_keys; }
static int aim_style;
int spartanportgpu_mouse_aim_style(void) { return aim_style; }
static int aim_sensitivity=100;
int spartanportgpu_mouse_aim_sensitivity(void) { return aim_sensitivity; }
void spartanportgpu_engine_fps_cycle(void) {}
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *name) {
    (void)table; (void)count; (void)name;
    return 0;
}

/* PS4 buttons a host source presses in `map`. */
static uint32_t pressed_by(const ButtonMap *map, int source) {
    uint32_t ps=0;
    for (size_t i=0;i<map->count;++i) if (map->entries[i].source==source) ps|=map->entries[i].ps;
    return ps;
}

int main(void) {
    ButtonMap directions=key_map;
    assert(apply_button_map(&directions,"move_up=T,move_down=G,move_left=F,move_right=H,"
                                       "look_up=Keypad 8,look_down=Keypad 2,"
                                       "look_left=Keypad 4,look_right=None",true)==8);

    ButtonMap keys=key_map;
    assert(apply_button_map(&keys,NULL,true)==0 && apply_button_map(&keys,"",true)==0);
    assert(keys.count==key_map.count);

    /* Swap: Q presses Cross, Space presses Triangle; Space no longer presses Cross. */
    assert(apply_button_map(&keys," cross = Q , triangle=Space",true)==2);
    assert(pressed_by(&keys,SDL_SCANCODE_Q)==BTN_CROSS);
    assert(pressed_by(&keys,SDL_SCANCODE_SPACE)==BTN_TRIANGLE);
    assert(pressed_by(&keys,SDL_SCANCODE_E)==BTN_SQUARE);

    /* Names with spaces, any case; the last entry for a button wins. */
    ButtonMap again=key_map;
    assert(apply_button_map(&again,"CIRCLE=left shift,circle=Left Ctrl",true)==2);
    assert(pressed_by(&again,SDL_SCANCODE_LCTRL)==BTN_CIRCLE);
    assert(pressed_by(&again,SDL_SCANCODE_LSHIFT)==0);

    /* Bad entries are skipped and leave the defaults alone. */
    ButtonMap bad=key_map;
    assert(apply_button_map(&bad,"bogus=Q,cross=NotAKey,circle,=Q,square=,,",true)==0);
    assert(bad.count==key_map.count && pressed_by(&bad,SDL_SCANCODE_SPACE)==BTN_CROSS);

    /* Gamepad: Xbox layout swap; 'touchpad' is the left click, Back stays the right click and
     * the real touchpad click keeps its fingers. */
    ButtonMap pad=pad_map;
    assert(apply_button_map(&pad,"cross=b,circle=a,touchpad=misc1",false)==3);
    assert(pressed_by(&pad,SDL_GAMEPAD_BUTTON_EAST)==BTN_CROSS);
    assert(pressed_by(&pad,SDL_GAMEPAD_BUTTON_SOUTH)==BTN_CIRCLE);
    assert(pressed_by(&pad,SDL_GAMEPAD_BUTTON_MISC1)==CLICK_LEFT);
    assert(pressed_by(&pad,SDL_GAMEPAD_BUTTON_BACK)==CLICK_RIGHT);
    assert(pressed_by(&pad,SDL_GAMEPAD_BUTTON_TOUCHPAD)==BTN_TOUCHPAD);
    assert(apply_button_map(&pad,"touchpad_right=guide,touchpad_left=paddle1",false)==2);
    assert(pressed_by(&pad,SDL_GAMEPAD_BUTTON_BACK)==0 && pressed_by(&pad,SDL_GAMEPAD_BUTTON_GUIDE)==CLICK_RIGHT);
    assert(pressed_by(&pad,SDL_GAMEPAD_BUTTON_MISC1)==0 && pressed_by(&pad,SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1)==CLICK_LEFT);
    assert(apply_button_map(&pad,"cross=Space",false)==0); /* a key name is no gamepad button */
    /* Keyboard: each side has a key of its own. */
    assert(pressed_by(&key_map,SDL_SCANCODE_TAB)==CLICK_LEFT && pressed_by(&key_map,SDL_SCANCODE_BACKSPACE)==CLICK_RIGHT);
    ButtonMap sides=key_map;
    assert(apply_button_map(&sides,"touchpad_right=Delete",true)==1);
    assert(pressed_by(&sides,SDL_SCANCODE_BACKSPACE)==0 && pressed_by(&sides,SDL_SCANCODE_DELETE)==CLICK_RIGHT);
    /* Comma and Equals stand for the two separator keys; Escape is an ordinary key. */
    ButtonMap marks=key_map;
    assert(apply_button_map(&marks,"cross=Comma,circle=equals,square=Escape",true)==3);
    assert(pressed_by(&marks,SDL_SCANCODE_COMMA)==BTN_CROSS && pressed_by(&marks,SDL_SCANCODE_EQUALS)==BTN_CIRCLE);
    assert(pressed_by(&marks,SDL_SCANCODE_ESCAPE)==BTN_SQUARE);

    /* Mouse names on the keyboard map (any case); they take the button from its default key. */
    ButtonMap mouse=key_map;
    assert(apply_button_map(&mouse,"square=Mouse Left,triangle=mouse right,circle=Mouse Middle,"
                                   "l1=Wheel Up,r1=wheel down,l2=Mouse Back,r2=Mouse Forward",true)==7);
    assert(pressed_by(&mouse,MOUSE_LEFT)==BTN_SQUARE && pressed_by(&mouse,SDL_SCANCODE_E)==0);
    assert(pressed_by(&mouse,MOUSE_RIGHT)==BTN_TRIANGLE && pressed_by(&mouse,MOUSE_MIDDLE)==BTN_CIRCLE);
    assert(pressed_by(&mouse,WHEEL_UP)==BTN_L1 && pressed_by(&mouse,WHEEL_DOWN)==BTN_R1);
    assert(pressed_by(&mouse,MOUSE_BACK)==BTN_L2 && pressed_by(&mouse,MOUSE_FORWARD)==BTN_R2);
    assert(apply_button_map(&pad_map,"cross=Mouse Left",false)==0); /* the gamepad map has no mouse */

    /* Sources: mouse masks, wheel pulses, and keys only inside the scancode range. */
    bool held[SDL_SCANCODE_COUNT]={0};
    held[SDL_SCANCODE_E]=true;
    const bool no_wheel[2]={false,false}, up[2]={true,false};
    assert(source_down(SDL_SCANCODE_E,held,0,no_wheel) && !source_down(SDL_SCANCODE_Q,held,0,no_wheel));
    assert(source_down(MOUSE_LEFT,held,SDL_BUTTON_LMASK,no_wheel));
    assert(!source_down(MOUSE_RIGHT,held,SDL_BUTTON_LMASK,no_wheel));
    assert(source_down(MOUSE_BACK,held,SDL_BUTTON_X1MASK,no_wheel));
    assert(source_down(MOUSE_FORWARD,held,SDL_BUTTON_X2MASK,no_wheel));
    assert(source_down(WHEEL_UP,held,0,up) && !source_down(WHEEL_DOWN,held,0,up));

    /* Two notches up: press, gap, press, then idle; the other direction stays up. */
    SDL_SetAtomicInt(&wheel_ticks[0],2);
    const uint64_t t0=1000;
    assert(wheel_held(0,t0) && wheel_held(0,t0+WHEEL_PRESS_US-1));
    assert(!wheel_held(0,t0+WHEEL_PRESS_US));
    const uint64_t t1=t0+WHEEL_PRESS_US+WHEEL_GAP_US;
    assert(wheel_held(0,t1) && !wheel_held(0,t1+WHEEL_PRESS_US));
    assert(!wheel_held(0,t1+WHEEL_PRESS_US+WHEEL_GAP_US));
    assert(!wheel_held(1,t1));
    /* Notches beyond the limit are dropped, and the menu drops the rest. */
    SDL_Event spin={0};
    spin.type=SDL_EVENT_MOUSE_WHEEL; spin.wheel.integer_y=-10;
    mouse_watch(NULL,&spin);
    assert(SDL_GetAtomicInt(&wheel_ticks[1])==WHEEL_MAX_PENDING);
    drop_wheel();
    assert(SDL_GetAtomicInt(&wheel_ticks[1])==0 && !wheel_held(1,t1));
    /* Rage: one source for L3 + R3; "None" clears L3 and R3 so their keys press nothing. */
    ButtonMap rage=key_map;
    assert(apply_button_map(&rage,"l3=None,r3=none,rage=V",true)==3);
    assert(pressed_by(&rage,SDL_SCANCODE_Z)==0 && pressed_by(&rage,SDL_SCANCODE_C)==0);
    assert(pressed_by(&rage,SDL_SCANCODE_V)==RAGE);
    PadData sparta={0};
    sparta.buttons=RAGE|BTN_SQUARE;
    apply_combos(&sparta);
    assert(sparta.buttons==(BTN_L3|BTN_R3|BTN_SQUARE));
    sparta.buttons=BTN_L3;
    apply_combos(&sparta);
    assert(sparta.buttons==BTN_L3);

    /* The pause key tells the window once per press; L1 + touchpad right (photo mode) does not. */
    PadData menu={0};
    menu.buttons=CLICK_RIGHT; note_pause_key(&menu); note_pause_key(&menu);
    assert(pause_keys==1);
    menu.buttons=0; note_pause_key(&menu);
    menu.buttons=CLICK_RIGHT|BTN_L1; note_pause_key(&menu);
    assert(pause_keys==1);
    menu.buttons=0; note_pause_key(&menu);
    menu.buttons=CLICK_RIGHT; note_pause_key(&menu);
    assert(pause_keys==2);

    /* L2 aim: the right stick follows the mouse's movement while L2 is held, then lets go. */
    SDL_Event glide={0};
    glide.type=SDL_EVENT_MOUSE_MOTION;
    PadData item={0};
    item.right_x=item.right_y=128; item.timestamp=50000000;
    glide.motion.xrel=500.0f; mouse_watch(NULL,&glide);
    apply_mouse_aim(&item);                                   /* no L2: untouched, motion dropped */
    assert(item.right_x==128 && item.right_y==128);
    item.buttons=BTN_L2; item.timestamp+=16000;
    glide.motion.xrel=10.0f; glide.motion.yrel=-5.0f; mouse_watch(NULL,&glide);
    apply_mouse_aim(&item);                                   /* right and up, partly tilted */
    assert(item.right_x>128 && item.right_x<255 && item.right_y<128 && item.right_y>0);
    const uint8_t tilt=item.right_x;
    item.right_x=item.right_y=128; item.timestamp+=16000;     /* a gap between mouse reports */
    apply_mouse_aim(&item);
    assert(item.right_x==tilt);
    item.right_x=item.right_y=128; item.timestamp+=AIM_HOLD_US; /* stopped: the stick is let go */
    apply_mouse_aim(&item);
    assert(item.right_x==128 && item.right_y==128);
    glide.motion.xrel=-400.0f; glide.motion.yrel=0; mouse_watch(NULL,&glide);
    item.timestamp+=16000;
    apply_mouse_aim(&item);                                   /* fast to the left: full tilt */
    assert(item.right_x==0 && item.right_y==128);

    /* Absolute: the offset from where the mouse was at the L2 press, kept while the mouse rests. */
    aim_style=1;
    item.buttons=0; item.timestamp+=16000;
    apply_mouse_aim(&item);                                   /* L2 released: forget the centre */
    item.buttons=BTN_L2; item.right_x=item.right_y=128;
    glide.motion.xrel=100.0f; glide.motion.yrel=0; mouse_watch(NULL,&glide);
    item.timestamp+=16000;
    apply_mouse_aim(&item);                                   /* half way to full tilt */
    assert(item.right_x==128+63 && item.right_y==128);
    item.right_x=item.right_y=128; item.timestamp+=AIM_HOLD_US*4;
    apply_mouse_aim(&item);                                   /* resting: still pointing there */
    assert(item.right_x==128+63);
    glide.motion.xrel=1000.0f; mouse_watch(NULL,&glide);
    apply_mouse_aim(&item);                                   /* far past the edge: full tilt */
    assert(item.right_x==255);
    item.right_x=128;
    glide.motion.xrel=-200.0f; mouse_watch(NULL,&glide);
    apply_mouse_aim(&item);                                   /* back by the radius: centred, let go */
    assert(item.right_x==128 && item.right_y==128);
    aim_sensitivity=50;                                       /* half: twice the mouse for the same stick */
    glide.motion.xrel=100.0f; mouse_watch(NULL,&glide);
    apply_mouse_aim(&item);
    assert(item.right_x==128+31);
    aim_sensitivity=100;
    aim_style=0;

    /* Grab: L1 + Circle from one source, alongside rage. */
    ButtonMap grab=key_map;
    assert(apply_button_map(&grab,"r3=X,grab=C,rage=V",true)==3); /* C was R3's default key */
    assert(pressed_by(&grab,SDL_SCANCODE_C)==GRAB && pressed_by(&grab,SDL_SCANCODE_X)==BTN_R3);
    sparta.buttons=GRAB|RAGE;
    apply_combos(&sparta);
    assert(sparta.buttons==(BTN_L1|BTN_CIRCLE|BTN_L3|BTN_R3));

    /* Dodge: a host-only source; the right stick follows the left one, or goes back when still. */
    ButtonMap evade=key_map;
    assert(apply_button_map(&evade,"dodge=Left Alt",true)==1 && pressed_by(&evade,SDL_SCANCODE_LALT)==DODGE);
    PadData p={0};
    p.left_x=p.left_y=p.right_x=p.right_y=128; p.timestamp=1000000;
    p.buttons=DODGE|BTN_CROSS; p.left_x=0;                      /* A held: dodge left */
    apply_dodge(&p);
    assert(p.right_x==0 && p.right_y==128 && p.buttons==BTN_CROSS);
    p.buttons=DODGE; p.left_x=255; p.left_y=0; p.timestamp+=10000; /* W+D: up and right */
    apply_dodge(&p);
    assert(p.right_x==255 && p.right_y==0);
    p.buttons=0; p.right_x=p.right_y=128; p.timestamp+=10000;  /* released early: held to the minimum */
    apply_dodge(&p);
    assert(p.right_x==255 && p.right_y==0);
    p.right_x=p.right_y=128; p.timestamp=1000000+DODGE_MIN_US;
    apply_dodge(&p);
    assert(p.right_x==128 && p.right_y==128);
    p.buttons=DODGE; p.left_x=p.left_y=140; p.timestamp+=1000000; /* standing still: back */
    apply_dodge(&p);
    assert(p.right_x==128 && p.right_y==255);
    reset_dodge();
    p.buttons=0; p.right_x=p.right_y=128; p.timestamp+=1000;
    apply_dodge(&p);
    assert(p.right_x==128 && p.right_y==128);

    /* Weapon cycle: the wheel is a host-only action that walks the d-pad weapons clockwise in
     * play, and moves a list up and down on the game's menus. */
    ButtonMap wheel=key_map;
    assert(apply_button_map(&wheel,"weapon_next=Wheel Up,weapon_prev=Wheel Down",true)==2);
    assert(pressed_by(&wheel,WHEEL_UP)==WEAPON_NEXT && pressed_by(&wheel,WHEEL_DOWN)==WEAPON_PREV);
    PadData w={0};
    w.left_x=w.left_y=128;
    uint64_t wheel_time=5000000;
    const uint32_t clockwise[]={BTN_UP,BTN_RIGHT,BTN_DOWN,BTN_LEFT};
    /* One read in sample()'s order: the game's menus first, then the cycle. */
    #define WEAPON_READ(bits) (w.buttons=(bits), w.timestamp=(wheel_time+=10000), note_game_menu(&w), \
                               apply_weapon_cycle(&w), w.buttons)
    #define WEAPON_RELEASE() do { WEAPON_READ(0); wheel_time+=WEAPON_MIN_US; WEAPON_READ(0); } while (0)
    #define MENU_SCROLLS() do { \
        for (int n=0;n<6;++n) { assert(WEAPON_READ(WEAPON_NEXT)==BTN_UP); WEAPON_RELEASE(); } \
        for (int n=0;n<6;++n) { assert(WEAPON_READ(WEAPON_PREV)==BTN_DOWN); WEAPON_RELEASE(); } \
    } while (0)

    /* The title menu, where the game starts: the wheel only ever moves the list up, or down. It
     * used to walk the weapons there too (up, right, down, left): the list went up, then back
     * down while the player kept scrolling up. */
    MENU_SCROLLS();
    /* A dodge is play: the wheel walks the weapons from then on. */
    WEAPON_READ(DODGE);
    assert(WEAPON_READ(WEAPON_NEXT)==BTN_RIGHT);               /* starts at UP: next is RIGHT */
    WEAPON_RELEASE();
    for (int i=2;i<6;++i) {                                    /* RIGHT, DOWN, LEFT, UP, RIGHT */
        assert(WEAPON_READ(WEAPON_NEXT)==clockwise[i%4]);
        WEAPON_RELEASE();
    }
    assert(WEAPON_READ(WEAPON_PREV)==BTN_UP);                  /* back from RIGHT */
    WEAPON_RELEASE();
    assert(WEAPON_READ(WEAPON_PREV)==BTN_LEFT);                /* wraps below UP */
    WEAPON_RELEASE();
    assert(WEAPON_READ(WEAPON_PREV)==BTN_DOWN);
    WEAPON_RELEASE();

    /* A pulse lasts several reads but is one step. */
    assert(WEAPON_READ(WEAPON_NEXT)==BTN_LEFT);
    for (int i=0;i<10;++i) assert(WEAPON_READ(WEAPON_NEXT)==BTN_LEFT);
    WEAPON_RELEASE();
    assert(WEAPON_READ(0)==0);

    /* Pressed from another source, the d-pad button becomes the current weapon. */
    assert(WEAPON_READ(BTN_DOWN)==BTN_DOWN);                   /* key 3, say */
    assert(WEAPON_READ(0)==0);
    assert(WEAPON_READ(WEAPON_NEXT)==BTN_LEFT);
    WEAPON_RELEASE();
    assert(WEAPON_READ(BTN_RIGHT)==BTN_RIGHT);
    assert(WEAPON_READ(0)==0);
    assert(WEAPON_READ(WEAPON_PREV)==BTN_UP);
    WEAPON_RELEASE();
    assert(WEAPON_READ(BTN_UP|BTN_DOWN)==(BTN_UP|BTN_DOWN));   /* two at once: no resync */
    assert(WEAPON_READ(0)==0);
    assert(WEAPON_READ(WEAPON_NEXT)==BTN_RIGHT);               /* still from UP */
    WEAPON_RELEASE();

    /* The host-only bits never reach the game, and other buttons pass through. */
    w.buttons=WEAPON_NEXT|WEAPON_PREV|BTN_CROSS; w.timestamp=(wheel_time+=10000);
    apply_weapon_cycle(&w);
    assert(!(w.buttons & (WEAPON_NEXT|WEAPON_PREV)) && (w.buttons & BTN_CROSS));
    WEAPON_RELEASE();

    /* A quick tap is held to the minimum; the button goes once it has passed. */
    const uint64_t tap_start=wheel_time+10000;
    assert(WEAPON_READ(WEAPON_NEXT)!=0 && WEAPON_READ(0)!=0);
    w.buttons=0; w.timestamp=tap_start+WEAPON_MIN_US-1; apply_weapon_cycle(&w);
    assert(w.buttons!=0);
    w.buttons=0; w.timestamp=tap_start+WEAPON_MIN_US; apply_weapon_cycle(&w);
    assert(w.buttons==0);
    wheel_time=w.timestamp;

    /* A real d-pad press during the hold cancels ours instead of pressing two buttons. */
    assert(WEAPON_READ(WEAPON_NEXT)!=0);
    assert(WEAPON_READ(BTN_UP)==BTN_UP);
    assert(WEAPON_READ(0)==0);
    wheel_time+=WEAPON_MIN_US;

    /* The menu opening drops the edges and the hold but keeps the weapon. */
    assert(WEAPON_READ(BTN_RIGHT)==BTN_RIGHT);
    assert(WEAPON_READ(WEAPON_NEXT)==BTN_DOWN);
    reset_weapon_cycle();
    assert(WEAPON_READ(0)==0);                                 /* hold gone */
    assert(WEAPON_READ(WEAPON_NEXT)==BTN_LEFT);                /* a held action counts as a new press */
    WEAPON_RELEASE();

    /* The pause menu (the touchpad's right click): the wheel moves the selection up and down
     * and keeps the weapon, even after attacks, Cross, Circle, L1 and R1 (the menus take some of
     * them, and the mouse is free there: a click is no play). */
    assert(WEAPON_READ(CLICK_RIGHT)==CLICK_RIGHT && WEAPON_READ(0)==0);
    MENU_SCROLLS();
    assert(WEAPON_READ(BTN_SQUARE|BTN_TRIANGLE|BTN_CROSS|BTN_CIRCLE|BTN_L1|BTN_R1|BTN_R2|BTN_L2)!=0);
    assert(WEAPON_READ(0)==0);
    MENU_SCROLLS();
    assert(WEAPON_READ(BTN_RIGHT)==BTN_RIGHT);                 /* a d-pad press in a menu is no weapon */
    assert(WEAPON_READ(0)==0);
    /* Rage of Sparta is play too, and the cycle goes on from the weapon it had (LEFT). */
    WEAPON_READ(RAGE);
    assert(WEAPON_READ(WEAPON_NEXT)==BTN_UP);
    WEAPON_RELEASE();
    assert(WEAPON_READ(WEAPON_NEXT)==BTN_RIGHT);               /* a menu would give UP again */
    WEAPON_RELEASE();

    /* The Power Up menu (Options), photo mode (the left click) and a real touchpad click are menus
     * too; a grab is play. */
    static const uint32_t menu_buttons[]={BTN_OPTIONS,CLICK_LEFT,BTN_TOUCHPAD};
    for (size_t i=0;i<sizeof(menu_buttons)/sizeof(*menu_buttons);++i) {
        WEAPON_READ(menu_buttons[i]);
        MENU_SCROLLS();
        WEAPON_READ(GRAB);
        assert(WEAPON_READ(WEAPON_NEXT)==clockwise[(2+2*i)%4]);
        WEAPON_RELEASE();
        assert(WEAPON_READ(WEAPON_NEXT)==clockwise[(3+2*i)%4]);
        WEAPON_RELEASE();
    }

    /* Walking is play: the left stick held MENU_WALK_US. Steps through a list (the stick let
     * go in between) and a small tilt are not. */
    WEAPON_READ(BTN_OPTIONS);
    for (int step=0;step<10;++step) {
        const uint64_t start=wheel_time;
        w.left_y=0;
        while (wheel_time-start<MENU_WALK_US-20000) WEAPON_READ(0);
        w.left_y=128;
        WEAPON_READ(0);
    }
    w.left_x=128+MENU_WALK_TILT-1;
    for (int n=0;n<300;++n) WEAPON_READ(0);
    w.left_x=128;
    MENU_SCROLLS();
    const uint64_t walk=wheel_time;
    w.left_x=255;
    while (wheel_time-walk<=MENU_WALK_US) WEAPON_READ(0);
    w.left_x=128;
    assert(WEAPON_READ(BTN_RIGHT)==BTN_RIGHT && WEAPON_READ(0)==0); /* in play: the weapon is RIGHT */
    assert(WEAPON_READ(WEAPON_NEXT)==BTN_DOWN);                /* a menu would give UP */
    WEAPON_RELEASE();

    /* Two notches in a row reach the game as two presses with the wheel's gap between them: the
     * hold lasts as long as a notch's press, so the game sees the release in between. */
    drop_wheel();
    SDL_SetAtomicInt(&wheel_ticks[0],2);
    int presses=0;
    uint64_t released=0, gap=UINT64_MAX;
    bool down=false;
    for (int n=0;n<100;++n) {
        w.timestamp=(wheel_time+=5000);
        w.buttons=wheel_held(0,w.timestamp) ? WEAPON_NEXT : 0;
        note_game_menu(&w);
        apply_weapon_cycle(&w);
        const bool now_down=(w.buttons & (BTN_UP|BTN_RIGHT|BTN_DOWN|BTN_LEFT))!=0;
        if (now_down && !down) {
            ++presses;
            if (released && w.timestamp-released<gap) gap=w.timestamp-released;
        }
        if (!now_down && down) released=w.timestamp;
        down=now_down;
    }
    assert(presses==2 && gap>=WHEEL_GAP_US);
    #undef WEAPON_READ
    #undef WEAPON_RELEASE
    #undef MENU_SCROLLS

    /* Keyboard rotation minigame: a prompt is up while the update hook counts frames. */
    _putenv("SPARTAN_STICK_PROMPTS=keyboard");
    _putenv("SPARTAN_QTE_SEQUENCES=360ccw=Q>E, up180cw = D > S > A ,bogus=W,down180cw=D>Mouse Left");
    static uint8_t prompt[0x20];
    spartanport_qte.block=(uint64_t)(uintptr_t)prompt;
    static bool keyboard[SDL_SCANCODE_COUNT];
    uint64_t now=10000000;
    prompt[SPARTAN_QTE_TYPE_OFFSET]=4;                            /* Up180CW: D S A */
    assert(!qte_play(keyboard,now));                           /* no frames: no prompt */
    assert(qte_sequences[2].length==3 && qte_sequences[2].keys[1]==SDL_SCANCODE_S); /* kept */
    assert(qte_sequences[5].length==2 && qte_sequences[5].keys[0]==SDL_SCANCODE_Q);
    keyboard[SDL_SCANCODE_D]=true;                             /* held while walking in */
    ++spartanport_qte.heartbeat;
    assert(qte_play(keyboard,now) && qte.step==0);
    keyboard[SDL_SCANCODE_D]=false;
    now+=16000; ++spartanport_qte.heartbeat;
    assert(qte_play(keyboard,now) && qte.step==0);
    const int wrong[]={SDL_SCANCODE_D,SDL_SCANCODE_S,SDL_SCANCODE_D,SDL_SCANCODE_S,SDL_SCANCODE_A};
    const unsigned steps[]={1,2,1,2,3};                        /* D out of turn: back to step 1 */
    for (unsigned i=0;i<5;++i) {
        keyboard[wrong[i]]=true; now+=16000; ++spartanport_qte.heartbeat;
        assert(qte_play(keyboard,now) && qte.step==steps[i]);
        keyboard[wrong[i]]=false; now+=16000; ++spartanport_qte.heartbeat;
        assert(qte_play(keyboard,now));
    }
    assert(qte.passed && spartanport_qte.pass==1);
    spartanport_qte.pass=0;                                           /* the rotation stub answered */
    keyboard[SDL_SCANCODE_D]=true; now+=16000; ++spartanport_qte.heartbeat;
    assert(qte_play(keyboard,now) && spartanport_qte.pass==0);         /* once per prompt */
    keyboard[SDL_SCANCODE_D]=false;
    now+=QTE_ACTIVE_US+1;                                      /* the prompt went away */
    assert(!qte_play(keyboard,now) && !qte.passed && qte.step==0);
    prompt[SPARTAN_QTE_TYPE_OFFSET]=1;                            /* a circle prompt: WASD stay */
    ++spartanport_qte.heartbeat;
    assert(!qte_play(keyboard,now));
    prompt[SPARTAN_QTE_TYPE_OFFSET]=9;                            /* 360CCW, custom Q E */
    now+=QTE_ACTIVE_US+1; ++spartanport_qte.heartbeat;
    assert(qte_play(keyboard,now));
    const int custom[]={SDL_SCANCODE_Q,SDL_SCANCODE_E};
    for (unsigned i=0;i<2;++i) {
        keyboard[custom[i]]=true; now+=16000; ++spartanport_qte.heartbeat;
        assert(qte_play(keyboard,now));
        keyboard[custom[i]]=false;
    }
    assert(spartanport_qte.pass==1);
    int parsed[QTE_KEYS_MAX];
    assert(qte_parse_sequence("Left Shift > Space",parsed)==2 && parsed[0]==SDL_SCANCODE_LSHIFT);
    assert(!qte_parse_sequence("D>Mouse Left",parsed) && !qte_parse_sequence("A>B>C>D>E>F>G>H>I",parsed));
    /* Adaptive mouse: the same sequence drawn, one step per mouse_step px in a new direction. */
    qte.mode=STICK_PROMPTS_MOUSE;
    prompt[SPARTAN_QTE_TYPE_OFFSET]=6;                            /* Down180CW: D S A */
    SDL_Event move={0};
    move.type=SDL_EVENT_MOUSE_MOTION;
    move.motion.xrel=500.0f; mouse_watch(NULL,&move);          /* before the prompt: forgotten */
    now+=QTE_ACTIVE_US+1;
    assert(!qte_play(keyboard,now));                           /* no prompt between the two */
    ++spartanport_qte.heartbeat;
    assert(qte_play(keyboard,now) && qte.step==0);
    const float path[][2]={{20,0},{30,5},{40,0},{0,30},{5,50},{-60,0},{-30,0}};
    const unsigned drawn[]={0,1,1,1,2,3,3};                    /* right, down, left; once each */
    for (unsigned i=0;i<7;++i) {
        move.motion.xrel=path[i][0]; move.motion.yrel=path[i][1];
        mouse_watch(NULL,&move);
        now+=16000; ++spartanport_qte.heartbeat;
        assert(qte_play(keyboard,now) && qte.step==drawn[i]);
    }
    assert(spartanport_qte.pass==1);
    move.motion.xrel=0; move.motion.yrel=-3; mouse_watch(NULL,&move);  /* jitter: nothing */
    now+=QTE_ACTIVE_US+1;
    assert(!qte_play(keyboard,now));
    ++spartanport_qte.heartbeat;
    assert(qte_play(keyboard,now) && qte.step==0 && spartanport_qte.pass==0);
    spartanport_qte.block=0;

    /* Direction sources drive both sticks, release their old keys, and never reach PS4 buttons. */
    const ButtonMap original_keys=key_map;
    key_map=directions;
    memset(keyboard,0,sizeof(keyboard));
    keyboard[SDL_SCANCODE_W]=keyboard[SDL_SCANCODE_UP]=keyboard[SDL_SCANCODE_RIGHT]=true;
    PadData remapped={.left_x=128,.left_y=128,.right_x=128,.right_y=128,.timestamp=now};
    sample_keyboard(&remapped,keyboard,0,no_wheel);
    assert(remapped.left_y==128 && remapped.right_x==128 && remapped.right_y==128);
    keyboard[SDL_SCANCODE_T]=keyboard[SDL_SCANCODE_H]=keyboard[SDL_SCANCODE_KP_4]=true;
    sample_keyboard(&remapped,keyboard,0,no_wheel);
    assert(remapped.left_x==255 && remapped.left_y==0 && remapped.right_x==0);
    assert(!(remapped.buttons & STICK_DIRECTIONS));
    remapped.buttons=DODGE;
    reset_dodge();
    apply_dodge(&remapped);
    assert(remapped.right_x==255 && remapped.right_y==0);
    assert(apply_button_map(&key_map,"move_up=None,move_right=None,look_left=None",true)==3);
    remapped=(PadData){.left_x=128,.left_y=128,.right_x=128,.right_y=128,.timestamp=now};
    sample_keyboard(&remapped,keyboard,0,no_wheel);
    assert(remapped.left_x==128 && remapped.left_y==128 && remapped.right_x==128);
    ButtonMap untouched_pad=pad_map;
    assert(apply_button_map(&untouched_pad,"move_up=a,look_right=b",false)==0);
    assert(!memcmp(&untouched_pad,&pad_map,sizeof(pad_map)));

    /* Default rotation prompts follow the remapped movement inputs, not their original letters. */
    key_map=directions;
    qte.mode=STICK_PROMPTS_KEYBOARD;
    qte_reset();
    memset(qte.held,0,sizeof(qte.held));
    memset(keyboard,0,sizeof(keyboard));
    spartanport_qte.block=(uint64_t)(uintptr_t)prompt;
    prompt[SPARTAN_QTE_TYPE_OFFSET]=6; /* Down180CW: right, down, left. */
    ++spartanport_qte.heartbeat;
    assert(qte_play(keyboard,now));
    const int old_turn[]={SDL_SCANCODE_D,SDL_SCANCODE_S,SDL_SCANCODE_A};
    const int new_turn[]={SDL_SCANCODE_H,SDL_SCANCODE_G,SDL_SCANCODE_F};
    for (unsigned i=0;i<3;++i) {
        keyboard[old_turn[i]]=true; now+=16000; ++spartanport_qte.heartbeat;
        assert(qte_play(keyboard,now) && qte.step==0);
        keyboard[old_turn[i]]=false;
    }
    for (unsigned i=0;i<3;++i) {
        keyboard[new_turn[i]]=true; now+=16000; ++spartanport_qte.heartbeat;
        assert(qte_play(keyboard,now) && qte.step==i+1);
        keyboard[new_turn[i]]=false; now+=16000; ++spartanport_qte.heartbeat;
        assert(qte_play(keyboard,now));
    }
    assert(spartanport_qte.pass==1);
    key_map=original_keys;
    spartanport_qte.block=0;

    puts("PASS: pad and key maps");
}
