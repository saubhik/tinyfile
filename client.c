#include <time.h>
#include <stdio.h>
#include "tinyfile/api.h"

int main() {
    tinyfile_init();

    int i;
    int num_async = 2048;
    int num_sync = 128;

    tinyfile_arg_t async_args[num_async];
    tinyfile_request_entry_idx_t entry_indxs[num_async];
    tinyfile_request_priority_t priority = 1;

    struct timespec ts1, ts2, diff;
    long diff_usec;
    clock_gettime(CLOCK_REALTIME, &ts1);

    for (i = 0; i < num_async / 2; ++i) {
        async_args[i].mul.x = i * 20;
        async_args[i].mul.y = i * 22;
        tinyfile_async(&async_args[i], TINYFILE_MUL, priority, &entry_indxs[i]);
    }

    clock_gettime(CLOCK_REALTIME, &ts2);
    diff.tv_sec = ts2.tv_sec - ts1.tv_sec;
    diff.tv_nsec = ts2.tv_nsec - ts1.tv_nsec;
    diff_usec = diff.tv_sec * 1000000 + (long) (diff.tv_nsec / 1000.0);

    printf("Time to perform first %d async requests: %ld usecs\n", num_async / 2, diff_usec);

    tinyfile_arg_t sync_args[num_sync], sync_outs[num_sync];
    priority = 10;

    for (i = 0; i < num_sync; ++i) {
        sync_args[i].mul.x = i * 20;
        sync_args[i].mul.y = i * 22;
        tinyfile_sync(&sync_args[i], TINYFILE_MUL, priority, &sync_outs[i]);
    }

    for (i = num_async / 2; i < num_async; ++i) {
        async_args[i].mul.x = i * 20;
        async_args[i].mul.y = i * 22;
        tinyfile_async(&async_args[i], TINYFILE_MUL, priority, &entry_indxs[i]);
    }

    tinyfile_async_join(entry_indxs, async_args, num_async);

    clock_gettime(CLOCK_REALTIME, &ts2);
    diff.tv_sec = ts2.tv_sec - ts1.tv_sec;
    diff.tv_nsec = ts2.tv_nsec - ts1.tv_nsec;
    diff_usec = diff.tv_sec * 1000000 + (long) (diff.tv_nsec / 1000.0);

    printf("Time to complete all %d sync and %d async requests (in parallel): %ld usecs\n", num_sync, num_async,
           diff_usec);

    tinyfile_exit();

    return 0;
}
