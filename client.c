#include <stdio.h>
#include <stdlib.h>
#include "tinyfile/api.h"

int main() {
    tinyfile_init();

    /*
     * Check asynchronous call to tinyfile
     */
    tinyfile_arg_t async_arg;
    tinyfile_request_entry_idx_t entry_index;
    tinyfile_request_priority_t priority = 1;

    FILE *fp = fopen("bin/input/test.txt", "r");
    if (fp != NULL) {
        if (fseek(fp, 0L, SEEK_END) == 0) {
            long bufsize = ftell(fp);
            if (bufsize == -1) {
                fprintf(stderr, "ERROR: ftell() failed\n");
                exit(EXIT_FAILURE);
            }

            async_arg.source = malloc(sizeof(char) * (bufsize + 1));

            if (fseek(fp, 0L, SEEK_SET) != 0) {
                fprintf(stderr, "ERROR: fseek() failed\n");
                exit(EXIT_FAILURE);
            }

            async_arg.source_len = fread(async_arg.source, sizeof(char), bufsize, fp);

            if (ferror(fp) != 0) {
                fprintf(stderr, "ERROR: Error reading file");
                exit(EXIT_FAILURE);
            }
        }
    }

    tinyfile_async(&async_arg, TINYFILE_COMPRESS, priority, &entry_index);

    tinyfile_exit();

    return 0;
}
