#include "ndl_common.h"
#include <inttypes.h>

uint64_t SS4S_NDL_DiagNow(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static void Distribution(SS4S_PlayerContext *c, const char *name, const NDL_DiagDistribution *d) {
    SS4S_NDL_webOS5_Log(SS4S_LogLevelInfo, "NDL", "NDL_PRESENT_V1 session=%" PRIu64
        " generation=%" PRIu64 " window=%" PRIu64 " metric=%s n=%" PRIu64 " sum=%" PRIu64
        " min=%" PRIu64 " max=%" PRIu64 " buckets=%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
        ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64,
        c->diagSession, c->diagGeneration, c->diagWindowId, name, d->count, d->sum, d->min, d->max,
        d->buckets[0], d->buckets[1], d->buckets[2], d->buckets[3], d->buckets[4], d->buckets[5],
        d->buckets[6], d->buckets[7], d->buckets[8], d->buckets[9], d->buckets[10], d->buckets[11]);
}

static void Flush(SS4S_PlayerContext *c) {
    NDL_DiagWindow *w = &c->diagWindow;
    if (!w->attempts) return;
    SS4S_NDL_webOS5_Log(SS4S_LogLevelInfo, "NDL", "NDL_PRESENT_V1 session=%" PRIu64
        " generation=%" PRIu64 " window=%" PRIu64 " start_us=%" PRIu64 " end_us=%" PRIu64
        " emitted_us=%" PRIu64 " attempts=%" PRIu64 " accepted=%" PRIu64 " failed=%" PRIu64 " not_ready=%" PRIu64
        " pts_backwards=%" PRIu64 " accepted_backwards=%" PRIu64,
        c->diagSession, c->diagGeneration, c->diagWindowId, w->startUs, w->endUs, SS4S_NDL_DiagNow(),
        w->attempts, w->accepted, w->failed, w->notReady, w->ptsBackwards, w->acceptedBackwards);
    Distribution(c, "pts_ms", &w->ptsMs);
    Distribution(c, "accepted_pts_ms", &w->acceptedPtsMs);
    Distribution(c, "play_us", &w->playUs);
    Distribution(c, "feed_us", &w->feedUs);
    c->diagWindowId++;
    NDL_DiagClearWindow(w);
}

void SS4S_NDL_DiagFeed(SS4S_PlayerContext *c, uint64_t start, bool ready, uint64_t pts, int rc, uint64_t playUs) {
    uint64_t end = SS4S_NDL_DiagNow();
    pthread_mutex_lock(&c->diagLock);
    NDL_DiagRecord(&c->diagWindow, start, end, ready, pts, rc, playUs);
    if (NDL_DiagDue(&c->diagWindow)) Flush(c);
    pthread_mutex_unlock(&c->diagLock);
}

void SS4S_NDL_DiagEvent(SS4S_PlayerContext *c, const char *event, uint64_t pts, int rc) {
    pthread_mutex_lock(&c->diagLock);
    if (strcmp(event, "load_attempt") == 0) {
        Flush(c);
        memset(&c->diagWindow, 0, sizeof(c->diagWindow));
        c->diagGeneration++;
        c->diagFirstAudio = false;
    }
    if (strcmp(event, "unload") == 0) Flush(c);
    if (strcmp(event, "audio_feed") == 0 && c->diagFirstAudio) {
        pthread_mutex_unlock(&c->diagLock);
        return;
    }
    if (strcmp(event, "audio_feed") == 0) c->diagFirstAudio = true;
    if (strcmp(event, "origin_assigned") == 0) c->diagOriginGeneration = c->diagGeneration;
    uint64_t origin = (uint64_t)c->mediaLoadedTime.tv_sec * 1000 + c->mediaLoadedTime.tv_nsec / 1000000;
    SS4S_NDL_webOS5_Log(SS4S_LogLevelInfo, "NDL", "NDL_CLOCK_V1 session=%" PRIu64
        " generation=%" PRIu64 " event=%s mono_us=%" PRIu64 " origin_ms=%" PRIu64
        " origin_generation=%" PRIu64 " pts_ms=%" PRIu64 " rc=%d loaded=%d audio_type=%u video_type=%u",
        c->diagSession, c->diagGeneration, event, SS4S_NDL_DiagNow(), origin, c->diagOriginGeneration,
        pts, rc, c->mediaLoaded, c->mediaInfo.audio.type, c->mediaInfo.video.type);
    pthread_mutex_unlock(&c->diagLock);
}
