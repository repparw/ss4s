#include "../modules/webos/ndl/webos5/ndl_diag_stats.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)

int main(int argc, char **argv) {
    if (argc > 1) CHECK(argv[1][0] != '!');
    NDL_DiagWindow w = {0};
    CHECK(!NDL_DiagDue(&w));
    NDL_DiagRecord(&w, 100, 120, false, 0, 0, 0);
    CHECK(w.notReady == 1 && w.ptsMs.count == 0 && w.playUs.count == 0);
    NDL_DiagRecord(&w, 200, 220, true, 0, 0, 10);
    NDL_DiagRecord(&w, 300, 330, true, 8, -1, 15);
    NDL_DiagRecord(&w, 400, 440, true, 17, 0, 20);
    CHECK(w.attempts == 4 && w.accepted == 2 && w.failed == 1);
    CHECK(w.ptsMs.count == 2 && w.ptsMs.sum == 17);
    CHECK(w.acceptedPtsMs.count == 1 && w.acceptedPtsMs.sum == 17);
    CHECK(w.playUs.min == 10 && w.playUs.max == 20 && w.playUs.sum == 45);
    CHECK(w.feedUs.count == 4 && w.feedUs.sum == 110);
    NDL_DiagRecord(&w, 500, 550, true, 17, 0, 25);
    CHECK(w.ptsMs.min == 0);
    NDL_DiagRecord(&w, 600, 660, true, 2, 0, 30);
    CHECK(w.ptsBackwards == 1 && w.acceptedBackwards == 1);
    NDL_DiagClearWindow(&w);
    CHECK(w.attempts == 0 && w.hasPts && w.lastPts == 2);
    NDL_DiagRecord(&w, 1000000, 1000010, true, 10, 0, 5);
    CHECK(w.ptsMs.count == 1 && w.ptsMs.sum == 8);
    NDL_DiagRecord(&w, 2000000, 2000010, true, 1010, 0, 5);
    CHECK(NDL_DiagDue(&w) && w.ptsMs.max == 1000);
    memset(&w, 0, sizeof(w));
    NDL_DiagRecord(&w, 3000000, 3000010, true, 0, 0, 5);
    CHECK(w.ptsMs.count == 0 && w.acceptedPtsMs.count == 0);
    NDL_DiagDistribution d = {0};
    NDL_DiagAdd(&d, 0); NDL_DiagAdd(&d, 1); NDL_DiagAdd(&d, 2);
    NDL_DiagAdd(&d, 1024); NDL_DiagAdd(&d, 1025);
    CHECK(d.buckets[0] == 2 && d.buckets[1] == 1 && d.buckets[10] == 1 && d.buckets[11] == 1);
    puts("NDL diagnostics aggregation: PASS");
    return 0;
}
