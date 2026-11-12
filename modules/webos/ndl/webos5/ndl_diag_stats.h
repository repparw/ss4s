#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define NDL_DIAG_BUCKETS 12

typedef struct {
    uint64_t count, sum, min, max;
    uint64_t buckets[NDL_DIAG_BUCKETS];
} NDL_DiagDistribution;

typedef struct {
    uint64_t startUs, endUs, attempts, accepted, failed, notReady;
    uint64_t lastPts, lastAcceptedPts, ptsBackwards, acceptedBackwards;
    bool hasPts, hasAcceptedPts;
    NDL_DiagDistribution ptsMs, acceptedPtsMs, playUs, feedUs;
} NDL_DiagWindow;

/* Buckets are <=1,2,4,...,1024, then >1024 in the named unit. */
static inline void NDL_DiagAdd(NDL_DiagDistribution *d, uint64_t value) {
    unsigned bucket = 0;
    while (bucket < NDL_DIAG_BUCKETS - 1 && value > (1ULL << bucket)) bucket++;
    d->buckets[bucket]++;
    if (d->count == 0 || value < d->min) d->min = value;
    if (value > d->max) d->max = value;
    d->count++;
    d->sum += value;
}

static inline void NDL_DiagRecord(NDL_DiagWindow *w, uint64_t startUs, uint64_t endUs,
                                   bool ready, uint64_t pts, int rc, uint64_t playUs) {
    if (w->attempts == 0) w->startUs = startUs;
    w->endUs = endUs;
    w->attempts++;
    NDL_DiagAdd(&w->feedUs, endUs - startUs);
    if (!ready) {
        w->notReady++;
        return;
    }
    if (w->hasPts) {
        if (pts < w->lastPts) w->ptsBackwards++;
        else NDL_DiagAdd(&w->ptsMs, pts - w->lastPts);
    }
    w->hasPts = true;
    w->lastPts = pts;
    NDL_DiagAdd(&w->playUs, playUs);
    if (rc != 0) {
        w->failed++;
        return;
    }
    w->accepted++;
    if (w->hasAcceptedPts) {
        if (pts < w->lastAcceptedPts) w->acceptedBackwards++;
        else NDL_DiagAdd(&w->acceptedPtsMs, pts - w->lastAcceptedPts);
    }
    w->hasAcceptedPts = true;
    w->lastAcceptedPts = pts;
}

static inline bool NDL_DiagDue(const NDL_DiagWindow *w) {
    return w->attempts && w->endUs - w->startUs >= 1000000;
}

static inline void NDL_DiagClearWindow(NDL_DiagWindow *w) {
    uint64_t last = w->lastPts, accepted = w->lastAcceptedPts;
    bool has = w->hasPts, hasAccepted = w->hasAcceptedPts;
    memset(w, 0, sizeof(*w));
    w->lastPts = last;
    w->lastAcceptedPts = accepted;
    w->hasPts = has;
    w->hasAcceptedPts = hasAccepted;
}
