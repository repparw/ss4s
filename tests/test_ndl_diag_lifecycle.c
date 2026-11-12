#include "../modules/webos/ndl/webos5/ndl_common.h"
#include "../modules/webos/ndl/webos5/opus_empty.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)
static uint64_t primePts;
static int primes, events, loadRc, videoRc;
static bool primedBeforeOrigin;
bool SS4S_NDL_webOS5_Initialized = true;
pthread_mutex_t SS4S_NDL_webOS5_Lock = PTHREAD_MUTEX_INITIALIZER;
static void ReportFrame(SS4S_Player *p, uint32_t latency) { (void)p; (void)latency; }
static const SS4S_LibraryContext library = { .VideoStats.ReportFrame = ReportFrame };
const SS4S_LibraryContext *SS4S_NDL_webOS5_Lib = &library;
int SS4S_NDL_webOS5_Driver_PostInit(int argc, char *argv[]) { (void)argc; (void)argv; return 0; }
void SS4S_NDL_webOS5_Driver_Quit(void) {}
bool SS4S_webOS_Is_HighEnd_SoC(void) { return false; }
int SS4S_webOS_GetMaxVideoResolution(SS4S_VideoCodec codec, unsigned int *w, unsigned int *h, unsigned int *fps) {
    (void)codec; (void)w; (void)h; (void)fps; return -1;
}
int NDL_DirectVideoPlay(void *data, unsigned int size, long long pts) {
    (void)data; (void)size; (void)pts; return videoRc;
}
int NDL_DirectVideoGetRenderBufferLength(int *length) { *length = 0; return 0; }
int NDL_DirectVideoSetHDRInfo(NDL_DIRECTVIDEO_HDR_INFO_T info) { (void)info; return 0; }
static void Log(SS4S_LogLevel level, const char *tag, const char *format, ...) {
    (void)level; (void)tag;
    char line[2048];
    va_list args; va_start(args, format); vsnprintf(line, sizeof(line), format, args); va_end(args);
    if (strstr(line, "NDL_CLOCK_V1")) events++;
    if (strstr(line, "event=prime_pcm") || strstr(line, "event=prime_opus")) {
        primedBeforeOrigin = strstr(line, "origin_generation=0 ") != NULL;
    }
}
SS4S_LoggingFunction *SS4S_NDL_webOS5_Log = Log;
int NDL_DirectMediaInit(const char *app) { (void)app; return 0; }
int NDL_DirectMediaUnload(void) { return 0; }
const char *NDL_DirectMediaGetError(void) { return "test"; }
int NDL_DirectMediaLoad(NDL_DIRECTMEDIA_DATA_INFO_T *info, NDLMediaLoadCallback callback) {
    (void)info; (void)callback; return loadRc;
}
int NDL_DirectAudioPlay(void *data, unsigned int size, long long pts) {
    (void)data; (void)size; primePts = pts; primes++; return 0;
}
int SS4S_OpusEmptyFeed(SS4S_OpusEmpty *empty, SS4S_NDLOpusEmptyFeedFunc feed, void *arg) {
    (void)empty; unsigned char data = 0; return feed(arg, &data, 1);
}

int main(int argc, char **argv) {
    (void)argv;
    SS4S_PlayerContext *c = SS4S_NDL_webOS5_PlayerDriver.Create(NULL);
    c->mediaInfo.audio.type = NDL_AUDIO_TYPE_PCM;
    c->mediaInfo.audio.pcm.channelMode = "stereo";
    CHECK(SS4S_NDL_webOS5_ReloadMedia(c) == 0 && primes == 1);
    CHECK(primePts > 0 && c->mediaLoadedTime.tv_sec > 0);
#ifdef SS4S_NDL_PRESENT_DIAGNOSTICS
    CHECK(events > 0 && primedBeforeOrigin);
    CHECK(c->diagGeneration == 1 && c->diagOriginGeneration == 1);
    SS4S_NDL_DiagFeed(c, SS4S_NDL_DiagNow(), true, 100, 0, 4);
    CHECK(c->diagWindow.hasPts);
#endif
    CHECK(SS4S_NDL_webOS5_ReloadMedia(c) == 0 && primes == 2);
#ifdef SS4S_NDL_PRESENT_DIAGNOSTICS
    CHECK(!primedBeforeOrigin && !c->diagWindow.hasPts);
    CHECK(c->diagGeneration == 2 && c->diagOriginGeneration == 2);
#endif
    loadRc = -1;
    CHECK(SS4S_NDL_webOS5_ReloadMedia(c) == -1 && primes == 2);
    CHECK(!c->mediaLoaded);
#ifdef SS4S_NDL_PRESENT_DIAGNOSTICS
    CHECK(c->diagGeneration == 3 && c->diagOriginGeneration == 2);
#else
    CHECK(events == 0);
#endif
    loadRc = 0;
    c->mediaInfo.audio.type = NDL_AUDIO_TYPE_OPUS;
    c->opusEmpty = (void *)1;
    CHECK(SS4S_NDL_webOS5_ReloadMedia(c) == 0 && primes == 3);
    c->opusEmpty = NULL;
    c->mediaInfo.audio.type = 0;
    c->mediaInfo.video.type = NDL_VIDEO_TYPE_H264;
    CHECK(SS4S_NDL_webOS5_ReloadMedia(c) == 0 && primes == 3);
    unsigned char frame = 0;
    SS4S_VideoInstance *video = (void *)c;
    CHECK(SS4S_NDL_webOS5_VideoDriver.Feed(video, &frame, 1, 0) == SS4S_VIDEO_FEED_OK);
    videoRc = -1;
    CHECK(SS4S_NDL_webOS5_VideoDriver.Feed(video, &frame, 1, 0) == SS4S_VIDEO_FEED_ERROR);
    c->mediaLoaded = false;
    CHECK(SS4S_NDL_webOS5_VideoDriver.Feed(video, &frame, 1, 0) == SS4S_VIDEO_FEED_NOT_READY);
#ifdef SS4S_NDL_PRESENT_DIAGNOSTICS
    CHECK(c->diagWindow.attempts == 3 && c->diagWindow.accepted == 1);
    CHECK(c->diagWindow.failed == 1 && c->diagWindow.notReady == 1);
#endif
    if (argc > 1) {
        videoRc = 0;
        c->mediaLoaded = true;
        struct timespec begin, end;
        clock_gettime(CLOCK_MONOTONIC, &begin);
        for (int i = 0; i < 1000000; i++) {
            CHECK(SS4S_NDL_webOS5_VideoDriver.Feed(video, &frame, 1, 0) == SS4S_VIDEO_FEED_OK);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        uint64_t elapsed = (end.tv_sec - begin.tv_sec) * 1000000000ULL + end.tv_nsec - begin.tv_nsec;
        printf("mock_feed_calls=1000000 elapsed_ns=%llu\n", (unsigned long long)elapsed);
    }
    SS4S_NDL_webOS5_PlayerDriver.Destroy(c);
    puts("Actual player lifecycle and diagnostic on/off: PASS");
    return 0;
}
