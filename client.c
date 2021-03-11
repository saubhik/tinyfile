#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include "tinyfile/api.h"

void *async_join(void *entry_idx) {
    /* Join the async requests if computation done. */
    tinyfile_arg_t out;
    tinyfile_async_wait(*(tinyfile_request_entry_idx_t *) entry_idx, &out);
    return NULL;
}

int main(int argc, char **argv) {
    static struct option long_options[] = {
            {"file",     required_argument, 0, 'f'},
            {"files",    required_argument, 0, 'l'},
            {"state",    required_argument, 0, 's'},
            {"sms_size", required_argument, 0, 'm'},
            {0, 0,                          0, 0}
    };

    char call_method; // 'a' for async, 's' for sync
    char file_path[150], file_list_path[150], *ptr;
    unsigned short int file_list = 0;

    int option_index = 0, c;
    while ((c = getopt_long(argc, argv, "f:l:s:", long_options, &option_index)) != -1) {
        switch (c) {
            case 0:
                printf("Using option %s", long_options[option_index].name);
                if (optarg)
                    printf(" with arg %s", optarg);
                printf("\n");
                break;

            case 'f':
                printf("Using option --file with value %s\n", optarg);
                strcpy(file_path, optarg);
                break;

            case 'l':
                printf("Using option --files with value %s\n", optarg);
                file_list = 1;
                strcpy(file_list_path, optarg);
                break;

            case 's':
                printf("Using option --state with value %s\n", optarg);
                if (strcmp(optarg, "ASYNC") == 0)
                    call_method = 'a';
                else if (strcmp(optarg, "SYNC") == 0)
                    call_method = 's';
                else
                    printf("Unexpected state value\n");
                break;

            case 'm':
                printf("Using option --sms_size with value %s\n", optarg);
                tinyfile_set_shm_size(strtol(optarg, &ptr, 10));
                break;

            default:
                printf("Required options (--file or --files), --state, --sms_size");
                abort();
        }
    }

    tinyfile_init();

    struct timespec ts1, ts2, diff;
    long diff_msec;

    tinyfile_request_priority_t priority = 1;

    if (file_list) {
        FILE *fp;

        if ((fp = fopen(file_list_path, "r")) == NULL) {
            fprintf(stderr, "ERROR: fopen() failed\n");
            exit(EXIT_FAILURE);
        }

        int lines = 0, ch;
        do {
            if ((ch = fgetc(fp)) == '\n')
                ++lines;
        } while (ch != EOF);

        if (fseek(fp, 0L, SEEK_SET) != 0) {
            fprintf(stderr, "ERROR: fseek() failed\n");
            exit(EXIT_FAILURE);
        }

        tinyfile_arg_t args[lines], out[lines];
        tinyfile_request_entry_idx_t keys[lines];

        char *line = NULL;
        size_t len = 0, i = 0;
        while (getline(&line, &len, fp) != -1) {
            line[strcspn(line, "\n")] = 0; // Remove trailing newline
            strcpy(args[i].source_file_path, line);
            ++i;
        }

        fclose(fp);
        free(line);

        pthread_t threads[lines];

        clock_gettime(CLOCK_REALTIME, &ts1);

        int j;
        for (j = 0; j < lines; ++j) {
            if (call_method == 'a') {
                tinyfile_async(&args[j], TINYFILE_COMPRESS, priority, &keys[j]);
                pthread_create(&threads[j], NULL, async_join, &keys[j]);
            } else if (call_method == 's') {
                tinyfile_sync(&args[j], TINYFILE_COMPRESS, priority, &out[j]);
            } else {
                fprintf(stderr, "ERROR: Unexpected state\n");
                exit(EXIT_FAILURE);
            }
        }

        for (j = 0; j < lines; ++j)
            pthread_join(threads[j], NULL);

        clock_gettime(CLOCK_REALTIME, &ts2);
        diff.tv_sec = ts2.tv_sec - ts1.tv_sec;
        diff.tv_nsec = ts2.tv_nsec - ts1.tv_nsec;
        diff_msec = diff.tv_sec * 1000 + (long) (diff.tv_nsec / 1000000.0);

        if (call_method == 'a')
            printf("Time to perform %d async requests: %ld millisecs.\n", lines, diff_msec);
        else
            printf("Time to perform %d sync requests: %ld millisecs.\n", lines, diff_msec);

        // printf("Compressed files:\n");
        // for (j = 0; j < lines; ++j)
        //    printf("%s\n", out[j].compressed_file_path);
    } else {
        /* Single file compression request. */

        tinyfile_arg_t arg, out;
        tinyfile_request_entry_idx_t key;
        strcpy(arg.source_file_path, file_path);

        clock_gettime(CLOCK_REALTIME, &ts1);

        if (call_method == 'a') {
            tinyfile_async(&arg, TINYFILE_COMPRESS, priority, &key);
            tinyfile_async_wait(key, &out);
        } else if (call_method == 's') {
            tinyfile_sync(&arg, TINYFILE_COMPRESS, priority, &out);
        } else {
            fprintf(stderr, "ERROR: Unexpected state\n");
            exit(EXIT_FAILURE);
        }

        clock_gettime(CLOCK_REALTIME, &ts2);
        diff.tv_sec = ts2.tv_sec - ts1.tv_sec;
        diff.tv_nsec = ts2.tv_nsec - ts1.tv_nsec;
        diff_msec = diff.tv_sec * 1000 + (long) (diff.tv_nsec / 1000000.0);

        if (call_method == 'a')
            printf("Time to perform single async request: %ld millisecs.\n", diff_msec);
        else
            printf("Time to perform single sync requests: %ld millisecs.\n", diff_msec);

        printf("Compressed file: %s\n", out.compressed_file_path);
    }

    tinyfile_exit();

    return 0;
}
