/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SPARTAN_DUALSENSE_H
#define SPARTAN_DUALSENSE_H
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DualSense extras for runtime_pad.c: adaptive trigger effects and the SPARTAN_DUALSENSE switch.
 *
 * The PS4 game never asks for trigger effects (scePadSetTriggerEffect is a PS5 call), so every
 * effect here is chosen by the port. SDL has no trigger API; SDL_SendGamepadEffect hands SDL's
 * PS5 driver the output report's effects block as it is (DS5EffectsState_t, 47 bytes, no
 * merging with its own rumble and LED state), so only the trigger enable bits are set and the
 * rumble and LED fields are left alone. The pad keeps a trigger effect until the next one.
 * The effect encodings are the ones of the DualSense output report; builders below. */

enum { DS_EFFECTS_SIZE=47, DS_RIGHT_TRIGGER_OFFSET=10, DS_LEFT_TRIGGER_OFFSET=21, DS_TRIGGER_SIZE=11,
       DS_ENABLE_RIGHT_TRIGGER=0x04, DS_ENABLE_LEFT_TRIGGER=0x08, DS_ZONES=10 };
typedef struct { uint8_t bytes[DS_TRIGGER_SIZE]; } DsTrigger;

/* Mode 0: the trigger moves freely. */
static inline DsTrigger ds_trigger_off(void) { DsTrigger t; memset(&t,0,sizeof(t)); return t; }

/* Resistance from `position` (0..9, the zone where it starts) to the end of the travel;
 * strength 1..8 (0 is off). Mode 0x21. */
static inline DsTrigger ds_trigger_feedback(unsigned position, unsigned strength) {
    DsTrigger t=ds_trigger_off();
    if (position>=DS_ZONES || strength==0) return t;
    if (strength>8) strength=8;
    uint32_t zones=0, forces=0;
    for (unsigned i=position;i<DS_ZONES;++i) { zones|=1u<<i; forces|=(uint32_t)(strength-1)<<(3*i); }
    t.bytes[0]=0x21;
    t.bytes[1]=(uint8_t)zones; t.bytes[2]=(uint8_t)(zones>>8);
    for (unsigned i=0;i<4;++i) t.bytes[3+i]=(uint8_t)(forces>>(8*i));
    return t;
}

/* A click: a wall that gives way between zones `start` (2..7) and `end` (start+1..8), then
 * releases, like a gun trigger or a latch. strength 1..8. Mode 0x25. */
static inline DsTrigger ds_trigger_weapon(unsigned start, unsigned end, unsigned strength) {
    DsTrigger t=ds_trigger_off();
    if (start<2 || start>7 || end<=start || end>8 || strength==0) return t;
    if (strength>8) strength=8;
    const uint16_t zones=(uint16_t)((1u<<start)|(1u<<end));
    t.bytes[0]=0x25;
    t.bytes[1]=(uint8_t)zones; t.bytes[2]=(uint8_t)(zones>>8);
    t.bytes[3]=(uint8_t)(strength-1);
    return t;
}

/* Vibration from `position` (0..9) to the end of the travel; amplitude 1..8, frequency in Hz
 * (1..255). Mode 0x26. */
static inline DsTrigger ds_trigger_vibration(unsigned position, unsigned amplitude, unsigned frequency) {
    DsTrigger t=ds_trigger_off();
    if (position>=DS_ZONES || amplitude==0 || frequency==0) return t;
    if (amplitude>8) amplitude=8;
    if (frequency>255) frequency=255;
    uint32_t zones=0, forces=0;
    for (unsigned i=position;i<DS_ZONES;++i) { zones|=1u<<i; forces|=(uint32_t)(amplitude-1)<<(3*i); }
    t.bytes[0]=0x26;
    t.bytes[1]=(uint8_t)zones; t.bytes[2]=(uint8_t)(zones>>8);
    for (unsigned i=0;i<4;++i) t.bytes[3+i]=(uint8_t)(forces>>(8*i));
    t.bytes[9]=(uint8_t)frequency;
    return t;
}

static inline bool ds_is_dualsense(SDL_Gamepad *g) { return g && SDL_GetGamepadType(g)==SDL_GAMEPAD_TYPE_PS5; }

/* The output report's effects block for both triggers. */
static inline void ds_build_effects(uint8_t out[DS_EFFECTS_SIZE], const DsTrigger *right, const DsTrigger *left) {
    memset(out,0,DS_EFFECTS_SIZE);
    out[0]=DS_ENABLE_RIGHT_TRIGGER|DS_ENABLE_LEFT_TRIGGER;
    memcpy(out+DS_RIGHT_TRIGGER_OFFSET,right->bytes,DS_TRIGGER_SIZE);
    memcpy(out+DS_LEFT_TRIGGER_OFFSET,left->bytes,DS_TRIGGER_SIZE);
}

/* What was last sent, so an effect is written once and again after a reconnect. */
static struct { DsTrigger right, left; SDL_Gamepad *sent_to; } ds_sent __attribute__((unused));
static inline bool ds_set_triggers(SDL_Gamepad *g, const DsTrigger *right, const DsTrigger *left) {
    if (!ds_is_dualsense(g)) return false;
    if (ds_sent.sent_to==g && !memcmp(&ds_sent.right,right,sizeof(*right)) && !memcmp(&ds_sent.left,left,sizeof(*left)))
        return true;
    uint8_t effects[DS_EFFECTS_SIZE];
    ds_build_effects(effects,right,left);
    if (!SDL_SendGamepadEffect(g,effects,sizeof(effects))) return false;
    ds_sent.right=*right; ds_sent.left=*left; ds_sent.sent_to=g;
    return true;
}
static inline void ds_forget(void);

/* The light bar colour for a QTE prompt type (qte_hook.h): the PlayStation colour of its button
 * (0 triangle green, 1 circle red, 2 cross blue, 3 square pink); a stick rotation (4..9) is white.
 * False for a type the game does not have. */
static inline bool ds_qte_color(unsigned type, uint8_t rgb[3]) {
    static const uint8_t colors[5][3]={{0,255,100},{255,0,0},{0,100,255},{255,60,200},{255,255,255}};
    if (type>9) return false;
    memcpy(rgb,colors[type<4 ? type : 4],3);
    return true;
}

/* The playback device of a pad's speaker, or 0. Over USB, Windows lists a DualSense's speaker as
 * an output of its own, named after the pad; over Bluetooth there is none. */
static inline SDL_AudioDeviceID ds_speaker_device(void) {
    int count=0;
    SDL_AudioDeviceID *devices=SDL_GetAudioPlaybackDevices(&count), found=0;
    for (int i=0;devices && i<count && !found;++i) {
        const char *name=SDL_GetAudioDeviceName(devices[i]);
        if (name && (SDL_strcasestr(name,"DualSense") || SDL_strcasestr(name,"Wireless Controller"))) found=devices[i];
    }
    SDL_free(devices);
    return found;
}

/* The pad's own speaker, which Windows does not switch on: the pad starts with its audio routed to
 * the headset jack only, so nothing played on its output reaches the speaker. Set the output path
 * to the speaker (byte 7 bits 4-5 = 3: the speaker plays the right channel, the jack is off), the
 * speaker volume (byte 5, the top of the range PS5 games use) and the speaker preamp gain (byte 37
 * bits 0-2, 0..7), each applied only with its enable bit (byte 0 bits 7 and 5, byte 1 bit 7). */
enum { DS_SPEAKER_VOLUME=0x64, DS_SPEAKER_GAIN_DEFAULT=4, DS_PATH_HEADSET=0x00, DS_PATH_SPEAKER=0x30 };
/* With a headset in the pad's jack, its audio stays on the headset (path 0, the pad's own choice). */
static struct { int gain, jack; SDL_Gamepad *sent_to; } ds_speaker_sent __attribute__((unused));
static inline bool ds_set_speaker(SDL_Gamepad *g, int gain, bool jack) {
    if (!ds_is_dualsense(g)) return false;
    if (ds_speaker_sent.sent_to==g && ds_speaker_sent.gain==gain && ds_speaker_sent.jack==jack) return true;
    uint8_t effects[DS_EFFECTS_SIZE];
    memset(effects,0,sizeof(effects));
    effects[0]=0x80|0x20; effects[5]=DS_SPEAKER_VOLUME; effects[7]=jack ? DS_PATH_HEADSET : DS_PATH_SPEAKER;
    effects[1]=0x80; effects[37]=(uint8_t)(gain & 7);
    if (!SDL_SendGamepadEffect(g,effects,sizeof(effects))) return false;
    ds_speaker_sent.gain=gain; ds_speaker_sent.jack=jack; ds_speaker_sent.sent_to=g;
    printf("Runtime: pad audio on its %s (speaker volume 0x%x, preamp gain %d)\n",jack ? "headset jack" : "speaker",
           DS_SPEAKER_VOLUME,gain);
    return true;
}

/* The headset jack bit of a DualSense USB input report (report id 1, status byte 54 bit 0;
 * bit 3 there is the USB data link), or -1 for another report. */
static inline int ds_report_jack(const uint8_t *report, int size) {
    if (size<56 || report[0]!=0x01) return -1;
    return report[54] & 1;
}
/* SDL keeps the pad's input reports to itself, so a second HID handle (Windows gives each handle
 * its own copy) reads the jack. Without one the jack reads as empty: the speaker plays. */
static struct { SDL_hid_device *device; bool tried, jack; } ds_hid __attribute__((unused));
static inline bool ds_jack_plugged(void) {
    if (!ds_hid.device && !ds_hid.tried) {
        ds_hid.tried=true;
        static const unsigned short products[]={0x0ce6,0x0df2}; /* DualSense, DualSense Edge */
        for (size_t i=0;i<2 && !ds_hid.device;++i) ds_hid.device=SDL_hid_open(0x054c,products[i],NULL);
        if (ds_hid.device) SDL_hid_set_nonblocking(ds_hid.device,1);
    }
    uint8_t report[128];
    int size;
    for (int i=0;ds_hid.device && i<64 && (size=SDL_hid_read(ds_hid.device,report,sizeof(report)))>0;++i) {
        const int jack=ds_report_jack(report,size);
        if (jack>=0) ds_hid.jack=jack;
    }
    return ds_hid.jack;
}
static inline void ds_forget(void) {
    ds_sent.sent_to=NULL; ds_speaker_sent.sent_to=NULL;
    if (ds_hid.device) SDL_hid_close(ds_hid.device);
    ds_hid.device=NULL; ds_hid.tried=false; ds_hid.jack=false;
}
/* "speakergain=N" (0..7) among the extras' names, or the default. */
static inline int ds_parse_speaker_gain(const char *text) {
    const char *at=text ? SDL_strcasestr(text,"speakergain=") : NULL;
    if (!at) return DS_SPEAKER_GAIN_DEFAULT;
    const int gain=atoi(at+12);
    return gain<0 ? 0 : gain>7 ? 7 : gain;
}

/* SPARTAN_DUALSENSE="triggers,hitflash,motion,gyroaim,qte,speaker" (or "all"): the extras that go
 * beyond what the game asks the pad for. All off by default; gyroaim includes motion. */
enum { DS_TRIGGERS=1, DS_HITFLASH=2, DS_MOTION=4, DS_GYROAIM=8, DS_QTE=16, DS_SPEAKER=32 };
/* Names separated by commas, spaces or new lines; unknown names are ignored. */
static inline unsigned ds_parse_features(const char *text) {
    unsigned features=0;
    char name[32];
    size_t n=0;
    for (const char *p=text;;++p) {
        const char c=*p;
        if (c && c!=',' && c!=' ' && c!='\t' && c!='\r' && c!='\n') { if (n<sizeof(name)-1) name[n++]=c; continue; }
        name[n]=0;
        if (!SDL_strcasecmp(name,"all")) features|=DS_TRIGGERS|DS_HITFLASH|DS_MOTION|DS_GYROAIM|DS_QTE|DS_SPEAKER;
        else if (!SDL_strcasecmp(name,"triggers")) features|=DS_TRIGGERS;
        else if (!SDL_strcasecmp(name,"hitflash")) features|=DS_HITFLASH;
        else if (!SDL_strcasecmp(name,"motion")) features|=DS_MOTION;
        else if (!SDL_strcasecmp(name,"gyroaim")) features|=DS_GYROAIM|DS_MOTION;
        else if (!SDL_strcasecmp(name,"qte")) features|=DS_QTE;
        else if (!SDL_strcasecmp(name,"speaker")) features|=DS_SPEAKER;
        n=0;
        if (!c) return features;
    }
}

/* The file dualsense.txt in SPARTAN_DATA_DIR (or SPARTAN_DUALSENSE_FILE) overrides the variable
 * while it exists, and is read again every second: the extras can be switched while the game runs.
 * speaker is the exception, taken when the game opens its audio ports at start. */
static int ds_gain_read __attribute__((unused))=DS_SPEAKER_GAIN_DEFAULT;
static inline unsigned ds_features(void) {
    static unsigned features, logged;
    static uint64_t checked;
    static bool loaded;
    const uint64_t now=SDL_GetTicks();
    if (loaded && now-checked<1000) return features;
    loaded=true; checked=now;
    char path[1024];
    const char *file=getenv("SPARTAN_DUALSENSE_FILE"), *dir=getenv("SPARTAN_DATA_DIR");
    if (!file) { snprintf(path,sizeof(path),"%s%sdualsense.txt",dir ? dir : "",dir ? "/" : ""); file=path; }
    FILE *f=fopen(file,"rb");
    if (f) {
        char text[256];
        const size_t n=fread(text,1,sizeof(text)-1,f);
        fclose(f);
        text[n]=0;
        features=ds_parse_features(text);
        ds_gain_read=ds_parse_speaker_gain(text);
    } else {
        const char *text=getenv("SPARTAN_DUALSENSE");
        features=text ? ds_parse_features(text) : 0;
        ds_gain_read=ds_parse_speaker_gain(text);
    }
    if (features!=logged) { logged=features; printf("Runtime: DualSense extras 0x%x\n",features); }
    return features;
}
/* The speaker preamp gain from the same place as the extras. */
static inline int ds_speaker_gain(void) { ds_features(); return ds_gain_read; }
#endif
