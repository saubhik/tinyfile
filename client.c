#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include "tinyfile/api.h"

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

        int j;
        for (j = 0; j < lines; ++j) {
            if (call_method == 'a') {

                printf("Making non-blocking async call: tinyfile_async() for file %s.\n", args[j].source_file_path);
                tinyfile_async(&args[j], TINYFILE_COMPRESS, priority, &keys[j]);

            } else if (call_method == 's') {

                printf("Making blocking sync call: tinyfile_sync() for file %s.\n", args[j].source_file_path);
                tinyfile_sync(&args[j], TINYFILE_COMPRESS, priority, &out[j]);
                printf("Sync call done. Compressed file: %s.\n", out[j].compressed_file_path);

            } else {

                fprintf(stderr, "ERROR: Unexpected state\n");
                exit(EXIT_FAILURE);

            }
        }

        if (call_method == 'a') {

            printf("Calling tinyfile_async_join().\n");
            tinyfile_async_join(keys, lines, out);
            printf("tinyfile_async_join() completed. Compressed files:\n");

            for (j = 0; j < lines; ++j)
                printf("%s\n", out[j].compressed_file_path);

        }
    } else {
        tinyfile_arg_t arg, out;
        tinyfile_request_entry_idx_t key;

        strcpy(arg.source_file_path, file_path);

        if (call_method == 'a') {

            printf("Making non-blocking async call: tinyfile_async() for file %s.\n", arg.source_file_path);
            tinyfile_async(&arg, TINYFILE_COMPRESS, priority, &key);

            printf("Async call done. Calling tinyfile_async_wait().\n");

            tinyfile_async_wait(key, &out);
            printf("tinyfile_async_wait() completed. Compressed file: %s.\n", out.compressed_file_path);

        } else if (call_method == 's') {

            printf("Making blocking sync call: tinyfile_sync() for file %s.\n", arg.source_file_path);
            tinyfile_sync(&arg, TINYFILE_COMPRESS, priority, &out);
            printf("Sync call done. Compressed file: %s.\n", out.compressed_file_path);

        }
    }

    tinyfile_exit();

    return 0;
}
