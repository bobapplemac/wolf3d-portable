#ifndef WG_PCM_QUEUE_TRACE_H
#define WG_PCM_QUEUE_TRACE_H
#include <stdint.h>
#include <string.h>
/* Durations start when an empty queue is observed, not at inferred playback time. */
typedef struct wg_pcm_queue_trace {
    uint64_t episodes, completed, total, longest, since;
    int started, empty;
} wg_pcm_queue_trace_t;
static void WG_PCMQueueTraceObserve(wg_pcm_queue_trace_t *t, int busy, uint64_t now)
{
    if (!t->started) return;
    if (!busy && !t->empty) { t->empty=1; t->since=now; ++t->episodes; }
    if (busy && t->empty) {
        uint64_t gap=now>=t->since ? now-t->since : 0;
        t->total+=gap; if(gap>t->longest)t->longest=gap;
        ++t->completed; t->empty=0;
    }
}
#endif
