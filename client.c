#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include "tinyfile/api.h"

int main(int argc, char **argv) {
    static struct option long_options[] = {
            {"file",  required_argument, 0, 'f'},
            {"files", required_argument, 0, 'l'},
            {"state", required_argument, 0, 's'},
            {0, 0,                       0, 0}
    };

    tinyfile_arg_t arg, out;
    char call_method; // 'a' for async, 's' for sync

    int option_index = 0, c;
    while ((c = getopt_long(argc, argv, "f:l:s:", long_options, &option_index)) != -1) {
        switch (c) {
            case 0:
                printf("option %s", long_options[option_index].name);
                if (optarg)
                    printf(" with arg %s", optarg);
                printf("\n");
                break;

            case 'f':
                printf("option -f with value %s\n", optarg);
                memcpy(arg.source_file_path, optarg, strlen(optarg));
                break;

            case 'l':
                printf("option -l with value %s\n", optarg);
                break;

            case 's':
                printf("option -s with value %s\n", optarg);
                if (strcmp(optarg, "ASYNC") == 0)
                    call_method = 'a';
                else if (strcmp(optarg, "SYNC") == 0)
                    call_method = 's';
                else
                    printf("Unexpected state value\n");
                break;

            default:
                abort();
        }
    }

    printf("arg.source_file_path = %s, call_method = %c\n", arg.source_file_path, call_method);


    tinyfile_init();

    tinyfile_request_entry_idx_t entry_index;
    tinyfile_request_priority_t priority = 1;

    if (call_method == 'a') {
        /* Asynchronous call to library */
        printf("Making an asynchronous call to tinyfile service.\n");

        tinyfile_async(&arg, TINYFILE_COMPRESS, priority, &entry_index);

        printf("Asynchronous call tinyfile_async() made.\n");
        printf("Now calling tinyfile_async_wait().\n");

        /* Wait till computation done. */
        tinyfile_async_wait(entry_index, &out);

        printf("tinyfile_async_wait() finished.\n");
        printf("Compressed file = %s.\n", out.compressed_file_path);

        tinyfile_exit();
    } else if (call_method == 's') {
        /* Synchronous call to library */
        printf("Making synchronous call to tinyfile_sync().\n");

        tinyfile_sync(&arg, TINYFILE_COMPRESS, priority, &out);

        printf("Synchronous call completed.\n");
        printf("Compressed file = %s.\n", out.compressed_file_path);
    }

    tinyfile_exit();

    return 0;
}
