#include <stdio.h>
#include <string.h>
#include "tinyfile/api.h"

int main() {
    tinyfile_init();

    /*
     * Check asynchronous call to tinyfile
     */
    tinyfile_arg_t arg;
    tinyfile_request_entry_idx_t entry_index;
    tinyfile_request_priority_t priority = 1;

//    FILE *fp = fopen("bin/input/test.txt", "r");
//    if (fp != NULL) {
//        if (fseek(fp, 0L, SEEK_END) == 0) {
//            long bufsize = ftell(fp);
//            if (bufsize == -1) {
//                fprintf(stderr, "ERROR: ftell() failed\n");
//                exit(EXIT_FAILURE);
//            }
//
//            arg.source = malloc(sizeof(char) * (bufsize + 1));
//
//            if (fseek(fp, 0L, SEEK_SET) != 0) {
//                fprintf(stderr, "ERROR: fseek() failed\n");
//                exit(EXIT_FAILURE);
//            }
//
//            arg.source_len = fread(arg.source, sizeof(char), bufsize, fp);
//
//            if (ferror(fp) != 0) {
//                fprintf(stderr, "ERROR: Error reading file");
//                exit(EXIT_FAILURE);
//            }
//        }
//    }

    static char *source_file_path = "bin/input/input.txt";
    static char *compressed_file_path = "bin/input/output.txt";
    memcpy(arg.source_file_path, source_file_path, strlen(source_file_path));
    memcpy(arg.compressed_file_path, compressed_file_path, strlen(compressed_file_path));

    /* Make an asynchronous call to the library. This is non-blocking. */
    tinyfile_async(&arg, TINYFILE_COMPRESS, priority, &entry_index);

    printf("Asynchronous call tinyfile_async() made.\n");
    printf("Now calling tinyfile_async_wait().\n");

    /* Wait till the result is ready */
    tinyfile_arg_t result;
    tinyfile_async_wait(entry_index, &result);

    printf("tinyfile_async_wait() finished. Compression done.\n");

    tinyfile_exit();

    return 0;
}
