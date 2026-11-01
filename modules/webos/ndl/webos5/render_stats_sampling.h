#pragma once

#include <stdbool.h>
#include <stdint.h>

#define NDL_RENDER_STATS_SAMPLE_INTERVAL_US UINT64_C(100000)
// Avoid replaying a stale queue-depth estimate indefinitely if the
// hardware query fails or the video thread stalls.
#define NDL_RENDER_STATS_MAX_AGE_US UINT64_C(250000)

typedef struct {
    uint64_t lastSampleTimeUs;
    int renderBufferLength;
    bool valid;
} NDL_RenderStatsCache;

static inline void ndl_reset_render_stats_cache(NDL_RenderStatsCache *cache) {
    *cache = (NDL_RenderStatsCache){0};
}

static inline bool ndl_should_sample_render_stats(uint64_t now_us, uint64_t last_sample_us) {
    if (last_sample_us == 0 || now_us < last_sample_us) {
        return true;
    }
    return now_us - last_sample_us >= NDL_RENDER_STATS_SAMPLE_INTERVAL_US;
}

// One hardware sample per ~100 ms. A failed query invalidates the cached
// occupancy instead of treating it as a fresh reading.
static inline void ndl_record_render_stats_sample(NDL_RenderStatsCache *cache, uint64_t now_us,
                                                    int query_result, int buffer_length) {
    cache->lastSampleTimeUs = now_us;
    cache->valid = query_result == 0;
    if (cache->valid) {
        cache->renderBufferLength = buffer_length;
    }
}

// Report once for every successfully fed frame, not only threshold-selected
// frames. This removes the frame-interval sampling bias for *constant* buffer
// occupancy, but is still an approximation when queue depth varies between
// hardware samples. All returned values remain latency *estimates*.
static inline bool ndl_estimate_render_stats_latency(const NDL_RenderStatsCache *cache,
                                                       uint64_t now_us, uint64_t frame_interval_us,
                                                       int *latency_us) {
    if (!cache->valid || now_us < cache->lastSampleTimeUs ||
        now_us - cache->lastSampleTimeUs > NDL_RENDER_STATS_MAX_AGE_US) {
        return false;
    }

    float length = cache->renderBufferLength > 0 ? (float)cache->renderBufferLength : 0.5f;
    *latency_us = (int)(length * (float)frame_interval_us);
    return true;
}
