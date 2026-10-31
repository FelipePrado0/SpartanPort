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
static int camera_stick;
static float camera_x, camera_y;
static int camera_steers, camera_recenters;
int spartanport_camera_stick_enabled(void) { return camera_stick; }
static int camera_on=1, camera_toggles;
int spartanport_camera_on(void) { return camera_on; }
static float camera_yaw;
float spartanport_camera_yaw(void) { return camera_yaw; }
void spartanport_camera_toggle(void) { camera_on=!camera_on; ++camera_toggles; }
void spartanport_camera_steer(float x, float y, float seconds, int recenter) {
    (void)seconds;
    if (recenter) { ++camera_recenters; return; }
    camera_x=x; camera_y=y; ++camera_steers;
}
static unsigned camera_writes;
unsigned spartanport_camera_writes(void) { return camera_writes; }
static int camera_live=1;
int spartanport_camera_live(void) { return camera_live; }
static int mouse_locked, camera_looks;
static float camera_look_x, camera_look_y;
int spartanportgpu_mouse_locked(void) { return mouse_locked; }
void spartanport_camera_look(float right_degrees, float down_degrees) {
    camera_look_x=right_degrees; camera_look_y=down_degrees; ++camera_looks;
}

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

int main(void) {
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

    /* Free camera: the right stick turns the camera and the game sees it centred, except with L1
     * (the game's evade) or L2 (the items' aim) held. Off, the game sees the stick as it is. */
    inject(path,"rx=255 ry=0");
    data=read_pad();
    assert(data.right_x==255 && data.right_y==0 && camera_steers==0);
    camera_stick=1;
    data=read_pad();
    assert(data.right_x==128 && data.right_y==128 && camera_x==1 && camera_y==-1);
    inject(path,"rx=140");  /* inside the dead zone */
    data=read_pad();
    assert(data.right_x==128 && camera_x==0);
    inject(path,"l1 rx=255");
    const int steers=camera_steers;
    data=read_pad();
    assert(data.right_x==255 && (data.buttons & BTN_L1) && camera_steers==steers);
    inject(path,"l2 rx=0");
    data=read_pad();
    assert(data.right_x==0 && camera_steers==steers);
    /* A QTE prompt (a button one) on screen: the camera goes back to the game's, the stick reaches
     * the game; once the prompt is gone the stick turns the camera again. */
    static uint8_t prompt[0x20];
    spartanport_qte.block=(uint64_t)(uintptr_t)prompt;
    ++spartanport_qte.heartbeat;
    inject(path,"rx=255");
    data=read_pad();
    assert(data.right_x==255 && camera_recenters==1 && camera_steers==steers);
    usleep(200000);  /* past QTE_ACTIVE_US without a frame */
    data=read_pad();
    assert(data.right_x==128 && camera_steers==steers+1);
    spartanport_qte.block=0;
    /* The keyboard's camera key (G) turns it off: the game's camera, and the stick is the game's
     * again; the key itself never reaches the game. Again, on. */
    inject(path,"camera rx=255");
    data=read_pad();
    assert(camera_toggles==1 && !camera_on && data.right_x==255 && !(data.buttons & CAMERA_TOGGLE));
    data=read_pad();  /* still held: no second toggle */
    assert(camera_toggles==1);
    inject(path,"rx=255");
    data=read_pad();
    inject(path,"camera rx=128");  /* a size of its own: on Windows the file is told apart by mtime and size */
    data=read_pad();
    assert(camera_toggles==2 && camera_on);
    /* The mouse turns the camera by an angle (0.12° a px at 100 %) while the game has the mouse,
     * not with L2 held (the items' aim keeps it) nor once the window lets go of it. */
    inject(path,"ry=128");
    mouse_locked=1;
    SDL_SetAtomicInt(&mouse_motion[0],100*16);
    SDL_SetAtomicInt(&mouse_motion[1],-50*16);
    data=read_pad();
    assert(camera_looks==1 && fabsf(camera_look_x-12.0f)<1e-4f && fabsf(camera_look_y+6.0f)<1e-4f);
    assert(SDL_GetAtomicInt(&mouse_motion[0])==0 && data.right_x==128);
    SDL_SetAtomicInt(&mouse_motion[0],100*16);
    data=read_pad();
    assert(camera_looks==2);
    data=read_pad();  /* no movement since: no turn either */
    assert(camera_looks==2);
    inject(path,"l2");
    SDL_SetAtomicInt(&mouse_motion[0],100*16);
    data=read_pad();
    assert(camera_looks==2);
    inject(path,"ry=128");
    mouse_locked=0;
    SDL_SetAtomicInt(&mouse_motion[0],100*16);
    data=read_pad();
    assert(camera_looks==2);
    /* Turned camera: the left stick (and the evade with L1) turns with it, so that up goes ahead of
     * the camera. Behind Kratos (half a turn) up is the game's down; a quarter turn left, its left. */
    camera_yaw=3.14159265f;
    inject(path,"ly=0");
    data=read_pad();
    assert(data.left_y==255 && data.left_x>=127 && data.left_x<=129);
    camera_yaw=1.5707963f;
    inject(path,"ly=0 lx=128");
    data=read_pad();
    assert(data.left_x==0 && data.left_y>=127 && data.left_y<=129);
    inject(path,"l1 rx=255 ry=128");
    data=read_pad();
    assert(data.right_y<=1 && data.right_x>=127 && data.right_x<=129);  /* right turns into the game's up */
    /* The pause menu (the touchpad) takes WASD and the right stick as they are, until the game's
     * controller writes the camera again once the settle time is over. */
    const int menu_steers=camera_steers;
    inject(path,"touchpad ly=0 lx=128 rx=255");
    data=read_pad();
    assert(data.left_y==0 && data.left_x==128 && data.right_x==255 && camera_steers==menu_steers);
    ++camera_writes;  /* the frame in flight when it opened */
    inject(path,"ly=0 lx=128 rx=255");
    data=read_pad();
    assert(data.left_y==0 && data.left_x==128 && data.right_x==255);
    usleep(320000);
    data=read_pad();
    assert(data.left_y==0 && data.left_x==128 && data.right_x==255);
    ++camera_writes;  /* play again */
    data=read_pad();
    assert(data.left_x==0 && data.right_x==128 && camera_steers==menu_steers+1);
    /* The title screen (no Kratos): the same, the sticks as they are. */
    camera_live=0;
    data=read_pad();
    assert(data.left_y==0 && data.left_x==128 && data.right_x==255 && camera_steers==menu_steers+1);
    camera_live=1;
    camera_yaw=0;
    camera_stick=0;
    inject(path,"");
    data=read_pad();

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
    puts("PASS: pad ABI, staged left/right clicks, L1 chord, engine frame rate combo, free camera stick, SDL touch coordinates, overlay capture");
}
