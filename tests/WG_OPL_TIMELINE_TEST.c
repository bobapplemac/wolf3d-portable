#include <assert.h>
#include <stdio.h>
#include "../platforms/win32/WG_OPL_TIMELINE.h"
static wg_opl_timeline_t t;
int main(void)
{
    wg_opl_event_t e;
    unsigned i;
    WG_OPLTimelineInit(&t, 11025, 1000000);
    assert(WG_OPLTimelinePush(&t, 1, 10, 0, 11025));
    assert(WG_OPLTimelinePush(&t, 2, 20, 110, 11025));
    assert(WG_OPLTimelinePush(&t, 3, 30, 110, 11025));
    assert(!WG_OPLTimelinePop(&t, 1000000, &e));
    WG_OPLTimelineSubmit(&t, 512, 1000000);
    assert(WG_OPLTimelinePop(&t, 1000000, &e) && e.reg == 1);
    assert(!WG_OPLTimelinePop(&t, 1001000, &e));
    assert(WG_OPLTimelinePop(&t, 1010000, &e) && e.reg == 2);
    assert(WG_OPLTimelinePop(&t, 1010000, &e) && e.reg == 3);
    assert(WG_OPLTimelinePush(&t, 4, 40, 600, 11025));
    assert(!WG_OPLTimelinePop(&t, 2000000, &e)); /* Starved: no future PCM. */
    WG_OPLTimelineSubmit(&t, 512, 2000000);
    assert(!WG_OPLTimelinePop(&t, 2001000, &e));
    assert(WG_OPLTimelinePop(&t, 2008000, &e) && e.reg == 4);
    WG_OPLTimelineInit(&t, 48000, 1000000);
    WG_OPLTimelineSubmit(&t, 2048, 0);
    WG_OPLTimelineObserve(&t, 480, 11000); /* Device clock corrects QPC drift. */
    assert(WG_OPLTimelineNow(&t, 12000) == 528);
    WG_OPLTimelineObserve(&t, 480, 12000); /* Coarse unchanged report: interpolate. */
    assert(WG_OPLTimelineNow(&t, 13000) == 576);
    assert(WG_OPLTimelineNow(&t, 1000000) == 2048);
    t.last_position = 0xfffffff0U; t.observed = 0xfffffff0ULL;
    t.submitted = 0x100000100ULL;
    WG_OPLTimelineObserve(&t, 16, 14000);
    assert(t.observed == 0x100000010ULL);
    WG_OPLTimelineInit(&t, 44100, 1000000);
    for (i = 0; i < WG_OPL_QUEUE_CAPACITY; ++i)
        assert(WG_OPLTimelinePush(&t, 1, 0, i, 44100));
    assert(!WG_OPLTimelinePush(&t, 1, 0, i, 44100));
    WG_OPLTimelineSubmit(&t, WG_OPL_QUEUE_CAPACITY, 0);
    assert(WG_OPLTimelinePop(&t, 0, &e));
    assert(WG_OPLTimelinePush(&t, 2, 0, i, 44100)); /* Ring wrap. */
    WG_OPLTimelineInit(&t, 11025, 1000000);
    assert(!t.count && !t.started); /* Restart discards old song. */
    assert(!WG_OPLTimelinePush(&t, 1, 0, 0, 48000));
    assert(WG_OPLTimelinePush(&t, 1, 0, 20, 11025));
    assert(!WG_OPLTimelinePush(&t, 1, 0, 19, 11025));
    puts("OPL timeline: spacing, equal-time order, underruns, drift, wrap, overflow and restart passed.");
    return 0;
}
