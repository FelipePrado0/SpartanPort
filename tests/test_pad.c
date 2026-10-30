#define _GNU_SOURCE
#include <assert.h>
#include <unistd.h>
#include "../src/runtime_pad.c"
#ifdef _WIN32
#define setenv(name, value, overwrite) _putenv_s(name, value)
#endif

static int capture;
static int vibration_percent=100;
static uint16_t rumble_low, rumble_high;
int spartanportgpu_overlay_captures_input(void) { return capture; }
int spartanportgpu_vibration_percent(void) { return vibration_percent; }
static int engine_fps_cycles;
void spartanportgpu_engine_fps_cycle(void) { ++engine_fps_cycles; }
int spartanportgpu_mouse_prompt_distance(void) { return 48; }
void spartanportgpu_mouse_pause_key(void) {}
int spartanportgpu_mouse_aim_style(void) { return 0; }
int spartanportgpu_mouse_aim_sensitivity(void) { return 100; }

static bool SDLCALL record_rumble(void *userdata, uint16_t low, uint16_t high) {
    (void)userdata;
    rumble_low=low;
    rumble_high=high;
    return true;
}
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *name) {
    (void)table; (void)count; (void)name;
    return 0;
}

static void inject(const char *path, const char *tokens) {
    FILE *f=fopen(path,"w");
    assert(f);
    fputs(tokens,f);
    fclose(f);
    usleep(25000);
}
static PadData read_pad(void) {
    PadData d;
    assert(pad_read_state(1,&d)==0);
    return d;
}
/* One finger in the middle of the left (480) or right (1440) half of the touchpad. */
static int touch_at(const PadData *d, int x) {
    return d->touch_count==1 && d->touches[0].x==x && d->touches[0].y==471;
}
static void pump(void) { SDL_UpdateJoysticks(); SDL_UpdateGamepads(); }

/* Switch attacks: move data in a buffer the scan may read, his stats, and the pad's requests. */
static uint8_t *switch_data;
int runtime_memory_is_mapped(uintptr_t address, uint64_t size) {
    const uintptr_t lo=(uintptr_t)switch_data;
    return switch_data && address>=lo && address+size<=lo+0x20000;
}
static uint8_t *switch_put(unsigned at, uint8_t weapon, unsigned target, uint16_t window_end) {
    uint8_t *b=switch_data+at;
    const int32_t rel=(int32_t)target-(int32_t)at;
    memcpy(b,&rel,4);
    b[0x10]=b[0x11]=0xaa; b[0x2c]=b[0x2d]=0xff; b[0x2e]=0x0f; b[0x2f]=weapon; b[0x30]=b[0x31]=1;
    memcpy(b+0x16,&window_end,2);
    return b;
}
static void test_switch_attack(void) {
    setenv("SPARTAN_SWITCH_ATTACKS","1",1);
    spartanport_switch_status(1);
    switch_data=aligned_alloc(0x10000,0x20000);
    memset(switch_data,0,0x20000);
    /* Switch attack moves: Cestus 0x100 (common) and 0x200, Blades 0x300 and 0x400, Claws 0x500. */
    const uint8_t *claws_to_cestus=switch_put(0x1000,0x10,0x100,0x3c00);
    const uint8_t *claws_to_blades=switch_put(0x1038,0x12,0x300,0x3c00);  /* the Claws block */
    switch_put(0x2000,0x12,0x300,0x3c00);
    const uint8_t *blades_to_claws=switch_put(0x3000,0x13,0x500,0x3c00);
    const uint8_t *blades_to_claws_short=switch_put(0x3100,0x13,0x500,0x3266);
    switch_put(0x4000,0x10,0x100,0x3266);
    const uint8_t *cestus_to_blades_short=switch_put(0x4038,0x12,0x300,0x3266);
    const uint8_t *rare_to_cestus=switch_put(0x5000,0x10,0x200,0x3c00);
    const uint8_t *rare_to_blades=switch_put(0x5038,0x12,0x400,0x3c00);
    switch_put(0x6000,0x12,0x300,0x3c00);
    switch_data[0x6000+0x31]=4;                                /* Cross held: found too */
    uint8_t other[0x38]={0};
    other[0x30]=other[0x31]=1; other[0x2f]=0x06;
    static uint8_t stats[0x300];
    stats[0x20b]=0x12;                                         /* the Blades in his hands */
    const uint32_t owned=1u<<15 | 1u<<17 | 1u<<18;             /* Cestus, Blades, Claws */
    memcpy(stats+0x1c4,&owned,4);
    spartanport_switch.stats=(uintptr_t)stats;
    spartanport_switch.caller=0x1234;

    /* No request: switch branches fail (Cross alone no longer cycles), others are the game's. */
    PadData d={0};
    d.timestamp=now_us();
    apply_switch_attack(&d);
    assert(spartanport_switch_input(blades_to_claws,0x1234)==0 && sw.scanned && sw.found==10);
    assert(spartanport_switch_input(other,0x1234)==2 && spartanport_switch_queue(other)==other);
    assert(spartanport_switch_input(blades_to_claws,0x5678)==0);     /* another caller: never (no cycling) */
    assert(spartanport_switch_input(other,0x5678)==2);
    assert(sw.attack[0][0]==switch_data+0x100 && sw.attack[0][1]==switch_data+0x200);
    assert(sw.attack[2][0]==switch_data+0x300 && sw.attack[2][1]==switch_data+0x400 && sw.attack[3][0]==switch_data+0x500);

    /* L1 + d-pad down: the Cestus. The Blades block's branch to the Claws takes it, swapped for one
     * to the Cestus's common switch attack, with the same timing. */
    d.buttons=BTN_L1; d.timestamp=now_us(); apply_switch_attack(&d);
    d.buttons=BTN_L1|BTN_DOWN; d.timestamp=now_us(); apply_switch_attack(&d);
    assert(sw.request==0x10);
    assert(spartanport_switch_input(blades_to_claws,0x1234)==1);
    assert(spartanport_switch_queue(blades_to_claws)==claws_to_cestus && !sw.request);
    assert(spartanport_switch_input(blades_to_claws,0x1234)==0);              /* taken once */

    /* L1 + Cross: the weapon in his hands, the Blades, from the Claws block's branch to the Cestus. */
    sw.last_at=0;
    d.buttons=BTN_L1; d.timestamp=now_us(); apply_switch_attack(&d);
    d.buttons=BTN_L1|BTN_CROSS; d.timestamp=now_us(); apply_switch_attack(&d);
    assert(sw.request==SWITCH_CURRENT && spartanport_switch_input(claws_to_cestus,0x1234)==1);
    assert(spartanport_switch_queue(claws_to_cestus)==claws_to_blades && !sw.request);
    /* The same weapon again within SWITCH_REPEAT_US: held back; another weapon is not. */
    d.buttons=BTN_L1; apply_switch_attack(&d);
    d.buttons=BTN_L1|BTN_CROSS; d.timestamp=now_us(); apply_switch_attack(&d);
    assert(spartanport_switch_input(claws_to_cestus,0x1234)==0 && spartanport_switch_queue(claws_to_cestus)==claws_to_cestus);
    d.buttons=BTN_L1; apply_switch_attack(&d);
    d.buttons=BTN_L1|BTN_RIGHT; d.timestamp=now_us(); apply_switch_attack(&d);
    assert(spartanport_switch_input(blades_to_claws,0x1234)==1 && spartanport_switch_queue(blades_to_claws)==blades_to_claws);
    sw.last_at=0;
    /* A branch with the short timing gets one with the short timing. */
    d.buttons=BTN_L1; apply_switch_attack(&d);
    d.buttons=BTN_L1|BTN_CROSS; d.timestamp=now_us(); apply_switch_attack(&d);
    assert(spartanport_switch_queue(blades_to_claws_short)==cestus_to_blades_short);
    /* The rare kind leads to the rare kind; a branch to the requested weapon stays. */
    sw.last_at=0;
    d.buttons=BTN_L1; apply_switch_attack(&d);
    d.buttons=BTN_L1|BTN_DOWN; d.timestamp=now_us(); apply_switch_attack(&d);
    assert(spartanport_switch_queue(rare_to_blades)==rare_to_cestus);
    sw.last_at=0;
    d.buttons=BTN_L1; apply_switch_attack(&d);
    d.buttons=BTN_L1|BTN_RIGHT; d.timestamp=now_us(); apply_switch_attack(&d);
    assert(spartanport_switch_queue(blades_to_claws)==blades_to_claws && !sw.request);

    /* Not owned (the Whip, d-pad left), or without L1: nothing. A request no move takes lapses. */
    d.buttons=BTN_L1; apply_switch_attack(&d);
    d.buttons=BTN_L1|BTN_LEFT; d.timestamp=now_us(); apply_switch_attack(&d);
    assert(sw.request==0x11 && spartanport_switch_input(blades_to_claws,0x1234)==0 && spartanport_switch_queue(blades_to_claws)==blades_to_claws);
    d.buttons=0; d.timestamp+=SWITCH_REQUEST_US+1000; apply_switch_attack(&d);
    assert(!sw.request);
    d.buttons=BTN_DOWN; d.timestamp=now_us(); apply_switch_attack(&d);
    assert(!sw.request);

    spartanport_switch.stats=0;
    free(switch_data);
    switch_data=NULL;
    setenv("SPARTAN_SWITCH_ATTACKS","0",1);
    sw.loaded=0;
}

int main(void) {
    test_switch_attack();
    const char *tmp=getenv("TMPDIR") ? getenv("TMPDIR") : getenv("TEMP") ? getenv("TEMP") : "/tmp";
    char path[1024];
    snprintf(path,sizeof(path),"%s/spartanport-pad-test-XXXXXX",tmp);
    int fd=mkstemp(path);
    assert(fd>=0);
    close(fd);
    setenv("SPARTAN_PAD_FILE",path,1);
    setenv("SDL_VIDEODRIVER","dummy",1);
    /* Only the virtual test controller is a gamepad, whatever is plugged in. */
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT,"0x1d50/0x6189");
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD));
    assert(pad_init()==0 && pad_open(1,0,0,NULL)==1);

    /* A click without a touch surface (Back, Tab, Backspace, the pad file) plays like a finger:
     * the touch comes first, the button CLICK_LEAD_US later, and the touch stays CLICK_TAIL_US
     * after the release. SpartanPort opens photo mode on the release of a left click with
     * the finger still down; a touch that appears and vanishes with the button is ignored. */
    inject(path,"cross l3 touchpad_left");
    PadData data=read_pad();
    assert((data.buttons & (BTN_CROSS|BTN_L3))==(BTN_CROSS|BTN_L3));
    assert(!(data.buttons & BTN_TOUCHPAD) && touch_at(&data,480));
    usleep(60000);
    data=read_pad();
    assert((data.buttons & BTN_TOUCHPAD) && touch_at(&data,480));
    usleep(60000);
    inject(path,"");
    data=read_pad();
    assert(data.buttons==0 && touch_at(&data,480)); /* released: the finger lingers */
    usleep(220000);
    data=read_pad();
    assert(data.buttons==0 && data.touch_count==0);

    /* A tap shorter than the lead still clicks, for CLICK_HOLD_US. */
    inject(path,"touchpad_right");
    data=read_pad();
    assert(!(data.buttons & BTN_TOUCHPAD) && touch_at(&data,1440));
    inject(path,"");
    data=read_pad();
    assert(!(data.buttons & BTN_TOUCHPAD) && touch_at(&data,1440));
    usleep(60000);
    data=read_pad();
    assert((data.buttons & BTN_TOUCHPAD) && touch_at(&data,1440));
    usleep(60000);
    data=read_pad();
    assert(!(data.buttons & BTN_TOUCHPAD) && touch_at(&data,1440));
    usleep(220000);
    data=read_pad();
    assert(data.buttons==0 && data.touch_count==0);

    /* L1 + the right click is a left click (photo mode on a pad without a touchpad). The side
     * is chosen when the click starts and kept until it ends. */
    inject(path,"l1 touchpad_right");
    data=read_pad();
    assert((data.buttons & BTN_L1) && touch_at(&data,480));
    inject(path,"touchpad_right");
    data=read_pad();
    assert(!(data.buttons & BTN_L1) && touch_at(&data,480));
    inject(path,"");
    data=read_pad(); /* the release is seen here: the tail starts */
    usleep(220000);
    data=read_pad();
    assert(data.buttons==0 && data.touch_count==0);

    /* The settings menu takes the input and ends a click; the source starts a new one after. */
    inject(path,"touchpad_right");
    usleep(60000);
    data=read_pad();
    assert(touch_at(&data,1440));
    capture=1;
    data=read_pad();
    assert(data.buttons==0 && data.touch_count==0);
    capture=0;
    data=read_pad();
    assert(!(data.buttons & BTN_TOUCHPAD) && touch_at(&data,1440));
    inject(path,"");
    data=read_pad(); /* the release is seen here: the tail starts */
    usleep(220000);
    data=read_pad();
    assert(data.buttons==0 && data.touch_count==0);

    /* R3 + L1 + R1 cycles the engine frame rate once per press; the three stay hidden from
     * the game until each one is released (a stray R1 would grab in the middle of a ledge). */
    inject(path,"r3 l1");
    data=read_pad();
    assert((data.buttons & (BTN_R3|BTN_L1))==(BTN_R3|BTN_L1) && engine_fps_cycles==0);
    inject(path,"r3 l1 r1 cross");
    data=read_pad();
    assert(engine_fps_cycles==1 && !(data.buttons & (BTN_R3|BTN_L1|BTN_R1)) && (data.buttons & BTN_CROSS));
    data=read_pad();
    assert(engine_fps_cycles==1 && !(data.buttons & (BTN_R3|BTN_L1|BTN_R1)));
    inject(path,"l1 r1");
    data=read_pad();
    assert(engine_fps_cycles==1 && data.buttons==0);
    inject(path,"");
    data=read_pad();
    inject(path,"l1");
    data=read_pad();
    assert(data.buttons==BTN_L1 && engine_fps_cycles==1);
    inject(path,"r3 l1 r1");
    data=read_pad();
    assert(engine_fps_cycles==2 && data.buttons==0);
    inject(path,"");
    data=read_pad();
    assert(data.buttons==0);

    /* R3 + R2 opens the trophy list and R3 + L2 the menu: with R3 held the trigger never reaches
     * the game (a stray R2 uses the item), until each one is released. */
    inject(path,"r2");
    data=read_pad();
    assert(data.buttons==BTN_R2 && data.r2==255);
    inject(path,"");
    data=read_pad();
    inject(path,"r3 r2");
    data=read_pad();
    assert(data.buttons==0 && data.r2==0);
    inject(path,"r2");
    data=read_pad();
    assert(data.buttons==0 && data.r2==0);
    inject(path,"r3 l2 cross");
    data=read_pad();
    assert(data.buttons==BTN_CROSS && data.l2==0);
    inject(path,"");
    data=read_pad();
    inject(path,"r3");
    data=read_pad();
    assert(data.buttons==BTN_R3);
    inject(path,"");
    data=read_pad();
    assert(data.buttons==0);

    SDL_VirtualJoystickTouchpadDesc touch={.nfingers=2};
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes=SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
    desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
    desc.name="spartanport test controller";
    desc.vendor_id=0x1d50;
    desc.product_id=0x6189;
    desc.ntouchpads=1;
    desc.touchpads=&touch;
    desc.Rumble=record_rumble;
    SDL_JoystickID id=SDL_AttachVirtualJoystick(&desc);
    assert(id!=0);
    SDL_Joystick *joystick=SDL_OpenJoystick(id);
    assert(joystick);
    /* A real touchpad: the fingers and the click go to the game as they are. */
    assert(SDL_SetJoystickVirtualTouchpad(joystick,0,0,true,0.75f,0.5f,1.0f));
    assert(SDL_SetJoystickVirtualTouchpad(joystick,0,1,true,0.25f,1.0f,1.0f));
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_TOUCHPAD,true));
    pump();
    data=read_pad();
    assert(gamepad && data.touch_count==2 && (data.buttons & BTN_TOUCHPAD));
    const uint8_t motors[]={255,128};
    assert(pad_vibration(1,motors)==0);
    assert(rumble_low==65535 && rumble_high==32896);
    vibration_percent=50;
    assert(pad_vibration(1,motors)==0);
    assert(rumble_low==32767 && rumble_high==16448);
    vibration_percent=0;
    assert(pad_vibration(1,motors)==0);
    assert(rumble_low==0 && rumble_high==0);
    vibration_percent=100;
    assert(data.touches[0].x==1439 && data.touches[0].y==471 && data.touches[0].id==0);
    assert(data.touches[1].x==480 && data.touches[1].y==942 && data.touches[1].id==1);
    capture=1;
    data=read_pad();
    assert(data.touch_count==0 && data.buttons==0);
    capture=0;
    /* The click without a finger is played as a left click. */
    assert(SDL_SetJoystickVirtualTouchpad(joystick,0,0,false,0,0,0));
    assert(SDL_SetJoystickVirtualTouchpad(joystick,0,1,false,0,0,0));
    pump();
    data=read_pad();
    assert(!(data.buttons & BTN_TOUCHPAD) && touch_at(&data,480));
    usleep(60000);
    data=read_pad();
    assert((data.buttons & BTN_TOUCHPAD) && touch_at(&data,480));
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_TOUCHPAD,false));
    pump();
    data=read_pad();
    usleep(220000);
    data=read_pad();
    assert(data.buttons==0 && data.touch_count==0);
    /* Back (Share, View) is the right click: the pause menu. With L1 it is the left click. */
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_BACK,true));
    pump();
    data=read_pad();
    assert(!(data.buttons & BTN_TOUCHPAD) && touch_at(&data,1440));
    usleep(60000);
    data=read_pad();
    assert((data.buttons & BTN_TOUCHPAD) && touch_at(&data,1440));
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_BACK,false));
    pump();
    data=read_pad();
    usleep(220000);
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,true));
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_BACK,true));
    pump();
    data=read_pad();
    assert((data.buttons & BTN_L1) && !(data.buttons & BTN_TOUCHPAD) && touch_at(&data,480));
    usleep(60000);
    data=read_pad();
    assert((data.buttons & (BTN_L1|BTN_TOUCHPAD))==(BTN_L1|BTN_TOUCHPAD) && touch_at(&data,480));
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,false));
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_BACK,false));
    pump();
    data=read_pad();
    usleep(220000);
    data=read_pad();
    assert(data.buttons==0 && data.touch_count==0);
    SDL_CloseJoystick(joystick);
    if (gamepad) SDL_CloseGamepad(gamepad);
    gamepad=NULL;
    assert(SDL_DetachVirtualJoystick(id));
    SDL_Quit();
    unlink(path);
    puts("PASS: pad ABI, staged left/right clicks, L1 chord, engine frame rate combo, SDL touch coordinates, overlay capture, switch attacks");
}
