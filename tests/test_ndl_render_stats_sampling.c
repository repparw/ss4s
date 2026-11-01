// Keep assert-based tests effective in Release builds too.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../modules/webos/ndl/webos5/render_stats_sampling.h"

static void testCacheLifecycle(void) {
    NDL_RenderStatsCache cache = {0};
    int latency = 0;
    const uint64_t first = UINT64_C(1000000);

    assert(ndl_should_sample_render_stats(first, 0));
    assert(!ndl_should_sample_render_stats(first + 1, first));
    assert(!ndl_should_sample_render_stats(first + 99999, first));
    assert(ndl_should_sample_render_stats(first + 100000, first));
    assert(ndl_should_sample_render_stats(1, first)); // clock rollback

    assert(!ndl_estimate_render_stats_latency(&cache, first, 16000, &latency));
    ndl_record_render_stats_sample(&cache, first, 0, 2);
    assert(ndl_estimate_render_stats_latency(&cache, first + 100000, 16000, &latency));
    assert(latency == 32000);
    assert(ndl_estimate_render_stats_latency(&cache, first + NDL_RENDER_STATS_MAX_AGE_US,
                                              16000, &latency));
    assert(!ndl_estimate_render_stats_latency(&cache, first + NDL_RENDER_STATS_MAX_AGE_US + 1,
                                               16000, &latency));
    assert(!ndl_estimate_render_stats_latency(&cache, first - 1, 16000, &latency));

    ndl_record_render_stats_sample(&cache, first + 100000, 0, 0);
    assert(ndl_estimate_render_stats_latency(&cache, first + 100000, 16000, &latency));
    assert(latency == 8000); // zero queue depth maps to existing 0.5 fallback

    ndl_record_render_stats_sample(&cache, first + 200000, -1, 99);
    assert(!ndl_estimate_render_stats_latency(&cache, first + 200000, 16000, &latency));

    ndl_record_render_stats_sample(&cache, first + 300000, 0, 1);
    assert(ndl_estimate_render_stats_latency(&cache, first + 300000, 14000, &latency));
    ndl_reset_render_stats_cache(&cache); // SDR/HDR or resolution reload
    assert(cache.lastSampleTimeUs == 0);
    assert(!ndl_estimate_render_stats_latency(&cache, first + 300000, 14000, &latency));
}

static void replay(bool varyingDepth) {
    // Repeated jitter: five 14-ms intervals and one 30-ms interval.
    // Under a 100-ms threshold, the selected interval aliases the pattern.
    NDL_RenderStatsCache cache = {0};
    uint64_t now = UINT64_C(1000000);
    uint64_t originalSampleTime = 0;
    uint64_t baselineTotal = 0, thresholdTotal = 0, cachedTotal = 0;
    unsigned int baselineFrames = 0, thresholdFrames = 0, cachedFrames = 0;

    for (unsigned int cycle = 0; cycle < 100; cycle++) {
        for (unsigned int phase = 0; phase < 6; phase++) {
            uint32_t frameIntervalUs = phase == 5 ? 30000 : 14000;
            int depth = varyingDepth ? (phase == 5 ? 1 : 4) : 1;
            int output;

            now += frameIntervalUs;
            baselineTotal += (uint64_t)frameIntervalUs * depth;
            baselineFrames++;

            if (ndl_should_sample_render_stats(now, originalSampleTime)) {
                originalSampleTime = now;
                thresholdTotal += (uint64_t)frameIntervalUs * depth;
                thresholdFrames++;
                ndl_record_render_stats_sample(&cache, now, 0, depth);
            }
            if (ndl_estimate_render_stats_latency(&cache, now, frameIntervalUs, &output)) {
                cachedTotal += (uint64_t)output;
                cachedFrames++;
            }
        }
    }

    assert(baselineFrames == 600);
    assert(thresholdFrames == 100); // exactly 10 hardware queries/s
    assert(cachedFrames == 600);    // per-frame reporting without extra queries
    double baselineAvg = (double)baselineTotal / baselineFrames;
    double thresholdAvg = (double)thresholdTotal / thresholdFrames;
    double cachedAvg = (double)cachedTotal / cachedFrames;
    printf("%s: baseline=%.3fms, threshold=%.3fms, cached=%.3fms, queries=%u/600\n",
           varyingDepth ? "phase-dependent depth" : "constant depth",
           baselineAvg / 1000, thresholdAvg / 1000, cachedAvg / 1000, thresholdFrames);

    if (!varyingDepth) {
        // Holding the latest buffer sample eliminates frame-interval bias
        // exactly when the underlying queue occupancy stays constant.
        assert(cachedTotal == baselineTotal);
        assert(thresholdTotal * baselineFrames != baselineTotal * thresholdFrames);
    }
    else {
        // Counterexample: if the depth has a 100-ms phase-dependent pattern,
        // the held depth is 4 when the 30-ms frame actually has depth 1.
        // Cached per-frame reporting can be *worse* than threshold sampling.
        assert(cachedAvg > thresholdAvg);
        assert(thresholdAvg > baselineAvg);
    }
}

int main(void) {
    testCacheLifecycle();
    replay(false);
    replay(true);
    return 0;
}
