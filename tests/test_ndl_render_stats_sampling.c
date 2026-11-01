// The test must run assertions in Release as well as Debug builds.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../modules/webos/ndl/webos5/render_stats_sampling.h"

static void test_rates(void) {
    NDL_RenderStatsSampler s = {0};
    ndl_sampler_init(&s, 60000, 1001, 1);
    assert(s.framesPerBlock == 6);
    ndl_sampler_init(&s, 60, 1, 1);
    assert(s.framesPerBlock == 6);
    ndl_sampler_init(&s, 30, 1, 1);
    assert(s.framesPerBlock == 3);
    ndl_sampler_init(&s, 24, 1, 1);
    assert(s.framesPerBlock == 3);
    ndl_sampler_init(&s, 120, 1, 1);
    assert(s.framesPerBlock == 12);
    ndl_sampler_init(&s, 144, 1, 1);
    assert(s.framesPerBlock == 15);
    ndl_sampler_init(&s, -1, 0, 0);
    assert(s.framesPerBlock == 6 && s.rngState != 0);
    ndl_sampler_init(&s, 2147483647, 1, 0);
    assert(s.framesPerBlock > 0);
}

static void test_exact_count_and_reset(void) {
    NDL_RenderStatsSampler s = {0};
    ndl_sampler_restart(&s); // safe before video initialization
    ndl_sampler_init(&s, 60, 1, 789);
    for (int block = 0; block < 10000; block++) {
        int count = 0;
        for (int frame = 0; frame < 6; frame++) {
            count += ndl_sampler_should_query(&s);
        }
        assert(count == 1);
    }
    ndl_sampler_restart(&s); // e.g. HDR or resolution reload
    assert(s.frameInBlock == 0);
    int count = 0;
    for (int frame = 0; frame < 6; frame++) {
        count += ndl_sampler_should_query(&s);
    }
    assert(count == 1);
}

static void test_jitter_phase_selection(void) {
    // Six frame intervals sum to 100 ms, but have a repeating jitter phase.
    // The old 100 ms threshold consistently selects the longest interval.
    const uint32_t intervals[6] = {29000, 14000, 14000, 14000, 14000, 15000};
    const unsigned blocks = 1000;
    uint64_t nowUs = 1000000, lastSampleUs = 0;
    uint64_t oldSum = 0, fullSum = 0;
    unsigned oldSamples = 0;
    for (unsigned i = 0; i < blocks * 6; i++) {
        uint32_t interval = intervals[i % 6];
        nowUs += interval;
        fullSum += interval;
        if (lastSampleUs == 0 || nowUs - lastSampleUs >= 100000) {
            lastSampleUs = nowUs;
            oldSum += interval;
            oldSamples++;
        }
    }
    double reference = (double)fullSum / (blocks * 6);
    double oldMean = (double)oldSum / oldSamples;
    assert(oldMean > reference * 1.3);

    // The stratified estimator is unbiased in expectation, not necessarily
    // in any single session. Compare the average across independent seeds.
    double estimateSum = 0;
    for (uint32_t seed = 1; seed <= 64; seed++) {
        NDL_RenderStatsSampler s = {0};
        ndl_sampler_init(&s, 60, 1, seed);
        uint64_t sampleSum = 0;
        unsigned samples = 0;
        for (unsigned i = 0; i < blocks * 6; i++) {
            if (ndl_sampler_should_query(&s)) {
                sampleSum += intervals[i % 6];
                samples++;
            }
        }
        assert(samples == blocks);
        estimateSum += (double)sampleSum / samples;
    }
    double estimate = estimateSum / 64.0;
    printf("reference_us=%.2f time_gate_us=%.2f stratified_us=%.2f\n",
           reference, oldMean, estimate);
    assert(estimate > reference * 0.99 && estimate < reference * 1.01);
}

int main(void) {
    test_rates();
    test_exact_count_and_reset();
    test_jitter_phase_selection();
    puts("passed");
    return 0;
}
