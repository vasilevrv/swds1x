#include "button_engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct { uint8_t channel; button_event_t event; } events[32];
static unsigned count;
static void emit(uint8_t ch, button_event_t event) { assert(count < 32); events[count].channel=ch; events[count++].event=event; }
static void poll(button_state_t *s, uint8_t ch, bool pressed, uint32_t t, bool enabled) {
    button_poll(s,ch,pressed,t,enabled,false,emit);
}
static void edge(button_state_t *s, uint8_t ch, bool pressed, uint32_t t, bool enabled) {
    poll(s,ch,pressed,t,enabled); poll(s,ch,pressed,t+10,enabled);
}
static void fresh(button_state_t *s, bool enabled) { count=0; button_init(s,false,0,enabled); }
int main(void) {
    button_state_t a,b;
    fresh(&a,false);
    poll(&a,0,true,100,false); poll(&a,0,false,105,false); poll(&a,0,false,200,false);
    assert(count==0); /* Five-ms spike rejected. */
    edge(&a,0,true,300,false); edge(&a,0,false,400,false);
    assert(count==1 && events[0].event==BUTTON_SINGLE);
    edge(&a,0,true,500,false); edge(&a,0,false,600,false);
    assert(count==2); /* No double-click waiting when disabled. */

    fresh(&a,true); edge(&a,0,true,100,true); edge(&a,0,false,200,true);
    poll(&a,0,false,599,true); assert(!count);
    poll(&a,0,false,600,true); assert(count==1 && events[0].event==BUTTON_SINGLE);
    fresh(&a,true); edge(&a,0,true,100,true); edge(&a,0,false,200,true);
    edge(&a,0,true,600,true); assert(!count); /* Inclusive 400-ms boundary, with debounce. */
    edge(&a,0,false,1000,true); assert(count==1 && events[0].event==BUTTON_DOUBLE);
    poll(&a,0,false,2000,true); assert(count==1);

    fresh(&a,true); edge(&a,0,true,100,true); edge(&a,0,false,200,true);
    edge(&a,0,true,601,true); assert(count==1 && events[0].event==BUTTON_SINGLE);
    edge(&a,0,false,700,true); poll(&a,0,false,1100,true); assert(count==2);

    fresh(&a,false); edge(&a,0,true,100,false); poll(&a,0,true,1100,false); assert(!count);
    poll(&a,0,true,1101,false); assert(count==1 && events[0].event==BUTTON_HOLD);
    poll(&a,0,true,10000,false); edge(&a,0,false,10001,false); assert(count==1);
    fresh(&a,false); edge(&a,0,true,100,false); edge(&a,0,false,1100,false);
    assert(count==1 && events[0].event==BUTTON_SINGLE); /* Exactly one second is short. */
    fresh(&a,false); edge(&a,0,true,100,false); edge(&a,0,false,1101,false);
    assert(count==1 && events[0].event==BUTTON_HOLD); /* Release just after threshold. */

    fresh(&a,true); edge(&a,0,true,100,true); edge(&a,0,false,200,true);
    edge(&a,0,true,300,true); poll(&a,0,true,1301,true);
    assert(count==2 && events[0].event==BUTTON_SINGLE && events[1].event==BUTTON_HOLD);
    edge(&a,0,false,1500,true); assert(count==2); /* Short click followed by a hold. */

    fresh(&a,true); edge(&a,0,true,100,true); edge(&a,0,false,200,true);
    poll(&a,0,false,250,false); assert(count==1);
    poll(&a,0,false,1000,false); assert(count==1); /* Config change completes pending click once. */
    button_init(&a,true,0,false); count=0; poll(&a,0,true,10000,false); assert(!count);
    edge(&a,0,false,10001,false); assert(!count); edge(&a,0,true,10100,false);
    edge(&a,0,false,10200,false); assert(count==1);

    fresh(&a,false); button_init(&b,false,0,true);
    edge(&a,0,true,100,false); edge(&b,1,true,100,true);
    edge(&a,0,false,200,false); edge(&b,1,false,200,true);
    poll(&b,1,false,600,true);
    assert(count==2 && events[0].channel==0 && events[1].channel==1);

    count=0; uint32_t start=UINT32_MAX-1000;
    button_init(&a,false,start,false); edge(&a,0,true,start+100,false);
    poll(&a,0,true,start+1101,false); assert(count==1 && events[0].event==BUTTON_HOLD);
    count=0; button_init(&a,false,UINT32_MAX-300,true);
    edge(&a,0,true,UINT32_MAX-200,true); edge(&a,0,false,UINT32_MAX-100,true);
    edge(&a,0,true,100,true); edge(&a,0,false,200,true);
    assert(count==1 && events[0].event==BUTTON_DOUBLE);

    fresh(&a,false);
    button_poll(&a,2,true,100,false,true,emit); button_poll(&a,2,true,110,false,true,emit);
    button_poll(&a,2,true,10099,false,true,emit); assert(!count);
    button_poll(&a,2,true,10100,false,true,emit); assert(count==1 && events[0].event==BUTTON_RESET);
    button_poll(&a,2,false,10200,false,true,emit); button_poll(&a,2,false,10210,false,true,emit); assert(count==1);
    puts("PASS: button debounce, immediate/delayed single, double boundaries, hold boundaries, config change, concurrent keys, boot-held keys, time wrap, service reset");
}
