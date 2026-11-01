# Prototype: frame-stratified NDL render statistics

This branch is stacked on ss4s #22, which remains **unchanged and unmerged**.

## Hypothesis

The 100 ms wall-time threshold in #22 selects the first frame crossing a
threshold, potentially locking onto a repeating jitter phase. Instead, choose
one uniformly pseudorandom *eligible frame* per fixed-size block of N frames,
where N = ceil(configured_fps / 10). At 60 FPS, N=6.

The frame is chosen independently of its arrival timing; each frame in a
complete block has equal inclusion probability. This eliminates the systematic
time-threshold selection mechanism *in expectation*. It does **not** guarantee
low error in any single session, remove buffer-occupancy sampling error,
or measure actual decoder/display latency.

## Tradeoffs

- At a stable 60 FPS, the mean query rate is ~10/s (one per six frames), not
  *at most one query in every 100 ms*. Adjacent queries can occur across block
  boundaries; stalls/bursts may cause nonuniform wall-time query rates.
- During substantial FPS drops, the query frequency drops proportionally;
  no separate timer keeps a minimum stats refresh rate.
- No shared global PRNG, thread, heap allocation or per-frame logging.
- On NDL media reload, the block restarts with a newly chosen position.
- An unsuccessful NDL query still consumes that block's single opportunity.
- The player initializes the block length using the configured stream FPS.

## Reproducible host-only demonstration

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -DNDEBUG \
   tests/test_ndl_render_stats_sampling.c -o /tmp/ndl_sampling_test
/tmp/ndl_sampling_test
```

For a repeating 60 FPS jitter cycle with interframe intervals
[29, 14, 14, 14, 14, 15] ms, the exact per-frame mean is 16.667 ms.
The original time-gate mean is 29.000 ms. Averaging the stratified result
over 64 deterministic seeds yields ~16.701 ms. This test exercises policy
logic, not physical NDL queries; taking the mean over seeds does not
represent any one player's run.

## Before deployment

Run actual-code jitter replays from the TV testing artifacts using both
policies, including variable render-buffer occupancy (not just interframe
duration), and compare mean error, variance, max query burst, and estimator
staleness. Then run controlled C3 sessions only if those results justify
another hardware batch. Do not merge #22 or this prototype based on the
synthetic test alone.
