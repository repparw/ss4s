# Investigate NDL submissions and audio-clock initialization

Enable `SS4S_NDL_PRESENT_DIAGNOSTICS=ON` only in a diagnostic build. It defaults to OFF and only affects the webOS 5 NDL backend. Use Moonlight's `MOONLIGHT_BENCH_STATS=ON` at the same time. This instrumentation does not change PTS units, scheduling, render-buffer polling, drop policy, audio priming, or encoding.

## Records

`NDL_PRESENT_V1` emits one summary and four histogram records per window, approximately once per second when video Feed is called. Every record carries session (player creation monotonic microseconds), load-attempt generation, and window ID. Unload and reload flush partial windows. No timer thread is added. If feeds stop entirely, no empty heartbeat is fabricated. A missing interval is unknown, not evidence of zero delivery.

The summary gives monotonic `start_us`, `end_us`, and `emitted_us`, attempts, accepted, failed, not_ready, and backward-PTS counts. `attempts = accepted + failed + not_ready`. Accepted means NDL_DirectVideoPlay returned zero; it does not mean a frame was decoded or displayed.

The four distributions are `pts_ms` (all actual play attempts), `accepted_pts_ms` (successful attempts only), `play_us` (NDL_DirectVideoPlay wall duration), and `feed_us` (Feed entry to just before diagnostic aggregation). Successful feed duration includes the existing render-buffer query and stats work. Diagnostic mutex wait, aggregation, and output happen afterward and therefore appear in Moonlight's outer Feed timer but not in `feed_us`.

Each distribution includes count, sum, min, max, and twelve bucket counts. Upper bounds are 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, and 1024 in the named unit, followed by overflow above 1024. Zero shares the first bucket. Bounds can locate a percentile bucket, not an exact percentile. `n=0` means no observed interval. Equal timestamps remain zero intervals. Backward timestamps increment a separate counter and do not become unsigned gaps. Consecutive-window boundaries retain the previous PTS; reload attempts discard it. Failed feeds do not advance the last accepted PTS. Not-ready feeds provide no PTS or play-duration sample.

NDL PTS here is integer milliseconds generated from the local monotonic clock relative to mediaLoadedTime. It is not the host's frame PTS or a physical presentation timestamp. At 120 FPS, integer quantization alone can produce 8/9 ms intervals.

`NDL_CLOCK_V1` records reload requests, deferred loads, load attempts and results, PCM or Opus priming, origin assignment, first ordinary audio feed per generation, and unload. Fields include monotonic microseconds, audio/video plane types, mediaLoaded, raw PTS milliseconds, return code, origin milliseconds, and the generation that last assigned that origin. The first priming record has origin_generation=0. Reload priming can refer to the previous generation. Plane type plus successful load is the requested/accepted configuration; it does not expose the proprietary audio clock or callback completion.

## Correlate with Moonlight

Retain timestamped native logs with ISO 8601 UTC prefixes, including rotations. Do not truncate a minute-long capture to the latest few lines. Record clock offsets between host, client log collector, and camera. Run:

```sh
python3 tools/correlate_ndl.py --ndl ndl.log --moonlight moonlight.log \
    --moonlight-benchmark-dir /path/to/moonlight-tv/tools/benchmark > correlation.json
```

The script reuses PR640's strict ML_BENCH_V1 parser. It associates NDL histogram components and lists intersecting Moonlight windows. The emitted_us to end_us difference corrects delayed partial-window flushes. UTC logging delay still limits alignment. Intersections contain whole original ML counters and may repeat across NDL windows. Do not sum intersections as new frames. Missing components and missing window IDs remain explicit. Clock lifecycle records remain in the raw trace for origin analysis.

## Offline checks

Configure native mocked tests with Debug or release flags that keep assertions enabled. Existing baseline tests contain side-effecting assert calls; NDEBUG can remove necessary operations. The new tests use always-evaluated checks.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSS4S_ENABLE_SAMPLES=OFF \
    -DSS4S_NDL_PRESENT_DIAGNOSTICS=ON
cmake --build build
ctest --test-dir build --output-on-failure --timeout 60
python3 -m unittest discover -s tools -v
```

The lifecycle tests execute actual player/video source with deterministic NDL stubs, in both enabled and disabled modes. They check first-load/reload priming order, failed loads, no-audio loads, feed failure/not-ready accounting, and resets. Aggregation tests cover zero/equal/backward timestamps, gaps, bucket bounds, and missing samples. They do not validate proprietary NDL behavior. Native mock overhead excludes real NDL and real syslog cost; target overhead remains a separate measurement.

## Physical validation requires separate authorization

Do not install or launch from this document alone. Retain the installed app payload, configuration, source mapping, and rollback artifact. Use Moonshine's existing tools/tv/verify_tv.py and tv_state.py guards, shared TV/GPU reservation, idle/ownership checks, durable restoration journal, and normal Moonlight Quit UI. Leave production Moonshine, pairing, and network configuration unchanged. The current installed client and both new packages have the same app ID, so package installation would replace it and requires authorization.

Use a controlled 1080p120 HEVC SDR source with a unique visible frame ID on every source frame. Freeze host/client/module hashes, settings, and the source renderer. Synchronize host logs, ML_BENCH_V1, NDL_PRESENT_V1, NDL_CLOCK_V1, and actual 240 FPS camera footage. Confirm camera capture rate rather than relying on playback metadata. Use short exposure, a fixed camera, a legible counter, and a shared visual synchronization marker. Track counter-read failures and partial scanout separately.

After warmup, count distinct visible IDs, repeats, skips, and intervals. A 240 FPS recording of a 120 Hz panel will normally sample each panel frame more than once; count IDs, not camera frames. Burned-in IDs establish which source frames reached the panel. Neither NDL success nor a nominal videooutput frameRate proves distinct presentation.

First measure diagnostic overhead without changing behavior. If the evidence later warrants a controlled variable, use at least five alternating baseline/candidate runs with at least 20 seconds of post-warmup samples per run. Do not alter audio origin, PTS units, or buffering while measuring the baseline. Finish normal cleanup and verify original app/configuration, audio, TV state, and host identity.

## Decision rule

Uneven attempted PTS and matching outer received/submitted gaps support an input-timing hypothesis. Stable attempted cadence with NDL failures or long play calls locates a feed boundary problem. Pre-origin priming is an observed ordering issue; an audio-clock cause requires additional evidence linking it to later timing. Stable accepted feed cadence plus camera-confirmed missing distinct IDs supports a probable downstream presentation issue. Without synchronized physical evidence, the conclusion remains **insufficient evidence**.
