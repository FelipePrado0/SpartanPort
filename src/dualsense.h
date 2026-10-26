/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SPARTAN_DUALSENSE_H
#define SPARTAN_DUALSENSE_H
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
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
static struct { DsTrigger right, left; SDL_Gamepad *sent_to; } ds_sent;
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
static inline void ds_forget(void) { ds_sent.sent_to=NULL; }

/* The light bar colour for a QTE prompt type (qte_hook.h): the PlayStation colour of its button
 * (0 triangle green, 1 circle red, 2 cross blue, 3 square pink); a stick rotation (4..9) is white.
 * False for a type the game does not have. */
static inline bool ds_qte_color(unsigned type, uint8_t rgb[3]) {
    static const uint8_t colors[5][3]={{0,255,100},{255,0,0},{0,100,255},{255,60,200},{255,255,255}};
    if (type>9) return false;
    memcpy(rgb,colors[type<4 ? type : 4],3);
    return true;
}

/* SPARTAN_DUALSENSE="triggers,hitflash,motion,gyroaim,qte" (or "all"): the extras that go beyond
 * what the game asks the pad for. All off by default; gyroaim includes motion. */
enum { DS_TRIGGERS=1, DS_HITFLASH=2, DS_MOTION=4, DS_GYROAIM=8, DS_QTE=16 };
static inline unsigned ds_features(void) {
    static unsigned features; static bool loaded;
    if (loaded) return features;
    loaded=true;
    const char *text=getenv("SPARTAN_DUALSENSE");
    if (!text) return features;
    char copy[128];
    if (strlen(text)>=sizeof(copy)) return features;
    strcpy(copy,text);
    for (char *name=copy,*next;name;name=next) {
        next=strchr(name,',');
        if (next) *next++=0;
        while (*name==' ') ++name;
        if (!SDL_strcasecmp(name,"all")) features|=DS_TRIGGERS|DS_HITFLASH|DS_MOTION|DS_GYROAIM|DS_QTE;
        else if (!SDL_strcasecmp(name,"qte")) features|=DS_QTE;
        else if (!SDL_strcasecmp(name,"triggers")) features|=DS_TRIGGERS;
        else if (!SDL_strcasecmp(name,"hitflash")) features|=DS_HITFLASH;
        else if (!SDL_strcasecmp(name,"motion")) features|=DS_MOTION;
        else if (!SDL_strcasecmp(name,"gyroaim")) features|=DS_GYROAIM|DS_MOTION;
    }
    return features;
}
#endif
