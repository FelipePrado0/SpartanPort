/* DualSense trigger effect encoding and the SPARTAN_DUALSENSE switch (src/dualsense.h). */
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/dualsense.h"

#ifdef _WIN32
#define setenv(name, value, overwrite) _putenv_s(name, value)
#endif

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

    assert(ds_features()==0);
    puts("dualsense: ok");
    return 0;
}
