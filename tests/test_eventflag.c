#include "runtime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef void *EventFlag;
typedef int32_t (ABI *CreateFn)(EventFlag*,const char*,uint32_t,uint64_t,const void*);
typedef int32_t (ABI *DeleteFn)(EventFlag);
typedef int32_t (ABI *SetFn)(EventFlag,uint64_t);
typedef int32_t (ABI *WaitFn)(EventFlag,uint64_t,uint32_t,uint64_t*,uint32_t*);

#define ETIMEDOUT_GUEST ((int32_t)0x8002003c)

int main(void) {
    CreateFn create=(CreateFn)runtime_eventflag_resolve("BpFoboUJoZU#libkernel");
    DeleteFn destroy=(DeleteFn)runtime_eventflag_resolve("8mql9OcQnd4#libkernel");
    SetFn set=(SetFn)runtime_eventflag_resolve("IOnSvHzqu6A#libkernel");
    WaitFn wait=(WaitFn)runtime_eventflag_resolve("JTvBflhYazQ#libkernel");
    assert(create && destroy && set && wait);

    EventFlag ef=NULL;
    assert(create(&ef,"firstlight-test",0x20,0x3,NULL)==0);

    uint64_t result=0;
    assert(wait(ef,0x3,0x01,&result,NULL)==0); /* AND */
    assert(result==0x3);

    /* CLEAR_BITS removes only the waited pattern. */
    assert(wait(ef,0x1,0x21,&result,NULL)==0);
    assert(result==0x3);

    uint32_t timeout=0;
    result=UINT64_MAX;
    assert(wait(ef,0x1,0x01,&result,&timeout)==ETIMEDOUT_GUEST);
    assert(timeout==0);
    assert(result==0x2);

    assert(set(ef,0x4)==0);
    assert(wait(ef,0x6,0x02,&result,NULL)==0); /* OR */
    assert((result&0x6)!=0);

    assert(destroy(ef)==0);
    puts("eventflag-test: ok");
    return 0;
}
