/* DualSense trigger effect encoding and the SPARTAN_DUALSENSE switch (src/dualsense.h). */
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/dualsense.h"

#ifdef _WIN32
#define setenv(name, value, overwrite) _putenv_s(name, value)
#endif

/* Checks the trigger effect encodings and the output report block, the extras and speaker gain
 * parsing, the QTE colours and the headset jack bit of an input report. */
int main(void) {
    const DsTrigger off=ds_trigger_off();
    for (int i=0;i<DS_TRIGGER_SIZE;++i) assert(off.bytes[i]==0);

    /* Weapon: zones 4 and 6 set, strength 6 stored as 5. */
    DsTrigger t=ds_trigger_weapon(4,6,6);
    assert(t.bytes[0]==0x25 && t.bytes[1]==((1<<4)|(1<<6)) && t.bytes[2]==0 && t.bytes[3]==5);
    t=ds_trigger_weapon(7,8,1); /* zone 8 lands in the second byte */
    assert(t.bytes[1]==(1<<7) && t.bytes[2]==1 && t.bytes[3]==0);
    assert(ds_trigger_weapon(1,6,4).bytes[0]==0 && ds_trigger_weapon(4,4,4).bytes[0]==0 &&
           ds_trigger_weapon(4,9,4).bytes[0]==0 && ds_trigger_weapon(4,6,0).bytes[0]==0);

    /* Feedback from zone 8: zones 8 and 9 active, strength 3 (stored 2) in 3 bits each. */
    t=ds_trigger_feedback(8,3);
    assert(t.bytes[0]==0x21 && t.bytes[1]==0 && t.bytes[2]==0x03);
    const uint32_t forces=(uint32_t)t.bytes[3]|(uint32_t)t.bytes[4]<<8|(uint32_t)t.bytes[5]<<16|(uint32_t)t.bytes[6]<<24;
    assert(forces==((2u<<24)|(2u<<27)));
    assert(ds_trigger_feedback(0,9).bytes[3]==0xff); /* strength clamps to 8: seven in every field */
    assert(ds_trigger_feedback(10,3).bytes[0]==0 && ds_trigger_feedback(2,0).bytes[0]==0);

    /* Vibration: same layout as feedback, frequency in byte 9. */
    t=ds_trigger_vibration(5,4,40);
    assert(t.bytes[0]==0x26 && t.bytes[1]==0xe0 && t.bytes[2]==0x03 && t.bytes[9]==40);
    assert(ds_trigger_vibration(5,4,0).bytes[0]==0);

    /* The effects block: enable bits for both triggers, each effect at its offset, nothing else. */
    uint8_t block[DS_EFFECTS_SIZE];
    const DsTrigger right=ds_trigger_weapon(4,6,6), left=ds_trigger_feedback(3,2);
    ds_build_effects(block,&right,&left);
    assert(block[0]==(DS_ENABLE_RIGHT_TRIGGER|DS_ENABLE_LEFT_TRIGGER) && block[1]==0);
    assert(!memcmp(block+10,right.bytes,DS_TRIGGER_SIZE) && !memcmp(block+21,left.bytes,DS_TRIGGER_SIZE));
    for (int i=2;i<10;++i) assert(block[i]==0);            /* rumble, audio: untouched */
    for (int i=32;i<DS_EFFECTS_SIZE;++i) assert(block[i]==0); /* LED: untouched */

    assert(!ds_is_dualsense(NULL));
    assert(!ds_set_triggers(NULL,&right,&left));

    uint8_t rgb[3];
    assert(ds_qte_color(0,rgb) && rgb[1]==255 && rgb[0]==0);   /* triangle: green */
    assert(ds_qte_color(1,rgb) && rgb[0]==255 && rgb[2]==0);   /* circle: red */
    assert(ds_qte_color(2,rgb) && rgb[2]==255 && rgb[0]==0);   /* cross: blue */
    assert(ds_qte_color(3,rgb) && rgb[0]==255 && rgb[2]>0);    /* square: pink */
    for (unsigned t=4;t<10;++t) assert(ds_qte_color(t,rgb) && rgb[0]==255 && rgb[1]==255 && rgb[2]==255);
    assert(!ds_qte_color(10,rgb));

    assert(ds_parse_features("")==0);
    assert(ds_parse_features("triggers, qte")==(DS_TRIGGERS|DS_QTE));
    assert(ds_parse_features("hitflash\r\nmotion\n")==(DS_HITFLASH|DS_MOTION));
    assert(ds_parse_features("gyroaim")==(DS_GYROAIM|DS_MOTION));
    assert(ds_parse_features("ALL")==(DS_TRIGGERS|DS_HITFLASH|DS_MOTION|DS_GYROAIM|DS_QTE|DS_SPEAKER));
    assert(ds_parse_features("bogus,,speaker")==DS_SPEAKER);
    assert(ds_parse_features("speaker,speakergain=6")==DS_SPEAKER);
    assert(ds_parse_speaker_gain("speaker,speakergain=6")==6);
    assert(ds_parse_speaker_gain("speaker")==DS_SPEAKER_GAIN_DEFAULT);
    assert(ds_parse_speaker_gain("SpeakerGain=9")==7);
    assert(ds_parse_speaker_gain(NULL)==DS_SPEAKER_GAIN_DEFAULT);

    uint8_t report[64]={0x01};
    report[54]=0x08; /* USB link, jack empty (read from a pad) */
    assert(ds_report_jack(report,64)==0);
    report[54]=0x09;
    assert(ds_report_jack(report,64)==1);
    assert(ds_report_jack(report,40)==-1);
    report[0]=0x31; /* a Bluetooth report */
    assert(ds_report_jack(report,64)==-1);

    assert(ds_features()==0);
    puts("dualsense: ok");
    return 0;
}
