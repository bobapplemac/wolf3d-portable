#ifndef WG_OPL_TIMELINE_H
#define WG_OPL_TIMELINE_H
#include <stdint.h>
#include <string.h>
#define WG_OPL_QUEUE_CAPACITY 16384U
typedef struct wg_opl_event {
    uint64_t frame;
    uint16_t reg;
    uint8_t value;
} wg_opl_event_t;
typedef struct wg_opl_timeline {
    wg_opl_event_t events[WG_OPL_QUEUE_CAPACITY];
    unsigned head, count;
    uint32_t rate, last_position;
    uint64_t frequency, anchor_frame, anchor_ticks, submitted, observed, emitted;
    int started;
} wg_opl_timeline_t;
static void WG_OPLTimelineInit(wg_opl_timeline_t *t, uint32_t rate, uint64_t frequency)
{
    memset(t, 0, sizeof(*t)); t->rate = rate; t->frequency = frequency;
}
static uint64_t WG_OPLTimelineNow(wg_opl_timeline_t *t, uint64_t now)
{
    uint64_t frame = t->anchor_frame;
    if (now >= t->anchor_ticks && t->frequency)
        frame += (now - t->anchor_ticks) * t->rate / t->frequency;
    if (frame > t->submitted) frame = t->submitted;
    return frame < t->emitted ? t->emitted : frame;
}

static void WG_OPLTimelineSubmit(wg_opl_timeline_t *t, unsigned frames, uint64_t now)
{
    /* Freeze elapsed time at the end of submitted PCM during starvation. */
    if (!t->started || WG_OPLTimelineNow(t, now) >= t->submitted)
    {
        t->anchor_frame = t->submitted; t->anchor_ticks = now;
    }
    t->submitted += frames; t->started = 1;
}
static void WG_OPLTimelineObserve(wg_opl_timeline_t *t, uint32_t position, uint64_t now)
{
    uint32_t delta = position - t->last_position;
    if (!delta) return;
    /* DWORD sample counter wraps naturally. Ignore impossible reset/jump. */
    if (t->observed + delta > t->submitted) return;
    t->observed += delta; t->last_position = position;
    t->anchor_frame = t->observed; t->anchor_ticks = now;
}
static int WG_OPLTimelinePush(wg_opl_timeline_t *t, uint16_t reg, uint8_t value,
                               uint64_t frame, uint32_t rate)
{
    unsigned index;
    if (t->count == WG_OPL_QUEUE_CAPACITY || rate != t->rate) return 0;
    if (t->count && frame < t->events[(t->head + t->count - 1) % WG_OPL_QUEUE_CAPACITY].frame)
        return 0;
    index = (t->head + t->count) % WG_OPL_QUEUE_CAPACITY;
    t->events[index].frame = frame; t->events[index].reg = reg;
    t->events[index].value = value; ++t->count;
    return 1;
}
static int WG_OPLTimelinePop(wg_opl_timeline_t *t, uint64_t now, wg_opl_event_t *event)
{
    uint64_t frame;
    if (!t->started || !t->count) return 0;
    frame = WG_OPLTimelineNow(t, now);
    if (t->events[t->head].frame > frame) return 0;
    *event = t->events[t->head];
    t->head = (t->head + 1) % WG_OPL_QUEUE_CAPACITY; --t->count;
    t->emitted = frame;
    return 1;
}
#endif
