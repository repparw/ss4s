# Offline diagnostic validation, November 11, 2026

Conclusion: **insufficient evidence** to identify why a 120 FPS stream has not been proven to present 120 distinct frames on the LG C3. The implementation is opt-in instrumentation. No performance or audio-initialization behavior change is proposed.

The actual-source stubbed tests reproduce the baseline ordering: initial PCM and Opus priming precede assignment of mediaLoadedTime. The first priming timestamp uses a zero origin, and reload priming uses the previous origin. The tests prove this code ordering, not how the proprietary NDL clock responds to it. An audio-clock cause remains a hypothesis.

## Verified offline

- SS4S baseline: dfba721b85420ccabf91dac65be73984bf1865f9. Candidate: this diagnostic PR, based directly on that revision.
- Moonlight source: efa4e93258332f45e843df7ab770f57dd1c4bd20. Both ARM builds use MOONLIGHT_BENCH_STATS=ON, RelWithDebInfo, the same recursive dependency pins, and the same downloaded dependency archives. Only the candidate enables SS4S_NDL_PRESENT_DIAGNOSTICS and applies this SS4S delta.
- Pinned SDK: webos-a38c582, Buildroot 2025.02.1/GCC 14.2.0. Archive SHA256 04ad3311b48b4557a7002aef56ae2e167478e8e129f37daac04649bddf813616.
- Pinned packager: ares-cli-rs 0.7.0-1. Debian archive SHA256 ff23939b5e4165c4ac7eb37b07662e4ff56af46075a7b239527883266cde8e11.
- Native enabled suite: seven passed, four unavailable audio-backend tests skipped. Focused AddressSanitizer and UndefinedBehaviorSanitizer tests: three passed. Diagnostic correlation tests: eight passed. Existing Moonlight benchmark parser tests: eight passed.
- An intentional failing-check invocation exits nonzero. New checks remain active with NDEBUG. Existing baseline assertion-dependent tests were run with assertions enabled after the initial NDEBUG run hung.
- Disabled native ndl_video.c, ndl_player.c and ndl_audio.c object .text sections match the baseline byte for byte under the same compiler and flags. Enabled/disabled lifecycle tests verify that disabled builds emit no diagnostic events.
- Both ARM packages were extracted and verified: the client contains ML_BENCH_V1; only the candidate NDL module contains NDL_PRESENT_V1 and NDL_CLOCK_V1. Package manifests and dependency archive hashes match. Compiler command lines confirm the opt-in definition only in the candidate.

The package ID/version is com.limelight.webos 1.6.36 arm in both builds. SHA256:

```text
baseline   b1da42e0fa3430dad5aaebb53d5b537faceaa3c33ec11e39fa4a8d04b2136523
candidate  a948122996c7a83bb1bdee602dfc69a7083aa2037dac57c331a35af63019971f
```

These are locally built, statically checked artifacts. Neither was installed. No TV contact or production operation occurred during this task.

## Overhead limits

Five alternating native on/off pairs exercised one million actual FeedVideo calls per run against no-op NDL stubs, checking every return value. Clock reads, the diagnostic mutex and aggregation add measurable local cost. The logger formats messages without emitting to syslog, and these short runs do not model sustained per-second output. This exercise therefore cannot establish meaningful LG C3 overhead. It supports neither a target speedup nor a no-overhead claim. A separately authorized matched on-device run is still required.

## Next experiment

After separate operator authorization, use the existing guarded Moonshine TV tools and the procedure below. Preserve the installed client and rollback payload. Run an unchanged 1080p120 HEVC SDR source with a unique visible frame counter. Capture synchronized host, ML_BENCH_V1, NDL submission/audio-clock diagnostics and verified 240 FPS camera footage. Determine distinct visible IDs and source skips, with unreadable IDs marked unknown. Stable accepted cadence plus missing camera IDs would support a probable downstream presentation issue; current offline results alone do not.

Use the correlation tool to retain mismatched and missing windows rather than filling them with zero. Evaluate instrumentation overhead before using timing differences as evidence. Any later controlled change needs five alternating pairs and at least twenty seconds of post-warmup samples per run. Do not change audio origin as part of this instrumentation PR.
