#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../platforms/win32/WG_PCM_QUEUE_TRACE.h"
int main(void)
{
    wg_pcm_queue_trace_t t;
    memset(&t,0,sizeof(t));
    WG_PCMQueueTraceObserve(&t,0,100);assert(!t.episodes); /* Startup excluded. */
    t.started=1;
    WG_PCMQueueTraceObserve(&t,1,200);
    WG_PCMQueueTraceObserve(&t,0,300);
    WG_PCMQueueTraceObserve(&t,0,400);assert(t.episodes==1);
    WG_PCMQueueTraceObserve(&t,1,500);
    assert(t.completed==1 && t.total==200 && t.longest==200 && !t.empty);
    WG_PCMQueueTraceObserve(&t,0,600);
    WG_PCMQueueTraceObserve(&t,1,650);
    assert(t.episodes==2 && t.completed==2 && t.total==250 && t.longest==200);
    WG_PCMQueueTraceObserve(&t,0,700);
    assert(t.empty && t.total==250); /* Open shutdown tail not counted. */
    WG_PCMQueueTraceObserve(&t,1,699);assert(t.total==250); /* Counter reversal. */
    memset(&t,0,sizeof(t));assert(!t.started && !t.total && !t.empty);
    puts("PCM queue trace: startup, transitions, durations, open tail and reset passed");
    return 0;
}
