# Experiment: per-frame latency estimates with cached NDL occupancy

**Prototype only.** Based on ss4s #22. Do not merge into #22 without
reviewing the fidelity tradeoff. No hardware measurements yet.

The NDL render-buffer query stays at most once per 100 ms (approximately
10 calls/s during 60 FPS streams), but each successful video Feed records a
`VideoStats.ReportFrame` estimate using the most recent successful queue
depth multiplied by that frame's own inter-submit interval. Failed queries
invalidate the cached value. Cache is reset at media reload and expires
after 250 ms without a successful update.

## Why

#22 reports only the frames at which a hardware query was made. When frame
cadence has a rhythm near the threshold interval, it can select fast or slow
frames disproportionately. Per-frame reporting eliminates this *specific*
selection bias if queue occupancy is stable.

## Offline replay (600 frames, 100 repetitions of 14/14/14/14/14/30 ms)

| Pattern | Every-frame NDL baseline | #22 threshold-only | Cached per-frame |
|---|---:|---:|---:|
| Constant depth 1 | 16.667 ms | 14.000 ms | 16.667 ms |
| Depth 4 on 14-ms frames, 1 on 30-ms frame | 51.667 ms | 56.000 ms | 66.667 ms |

Both sampled approaches make 100 NDL queries across 600 frames, 10 queries/s
for this 10-second replay. The cached approach creates 600 reported latency
estimates. All timings are synthetic and are deterministic test fixtures,
not real LG C3 measurements.

**Interpretation:** Per-frame reporting fixes selection of the frame interval
but cannot reconstruct queue depth between queries. Under phase-dependent
occupancy it can make the estimator *less* accurate.

To reproduce: build and run the existing
`ss4s_test_ndl_render_stats_sampling` via CTest, or compile
`tests/test_ndl_render_stats_sampling.c` directly with C11.

## Next decision

This approach should remain exploratory. For an accurate visible overlay,
consider querying per-frame **only while stats are displayed**, and disabling
render-buffer queries entirely when hidden. That needs an explicit signal
from the UI or a carefully synchronized demand state; it is not part of this
prototype. No claim is made about true decoded/displayed latency.
