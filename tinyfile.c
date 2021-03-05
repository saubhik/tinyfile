#include <stdlib.h>
#include <stdio.h>

#include "snappy-c/snappy.h"


void init_service() {}

void compress(char *input, size_t input_length, char *compressed,
			  size_t *compressed_length) {
	struct snappy_env env;
	snappy_init_env(&env); // TODO: Now or during init_service?
	snappy_compress(&env, input, input_length, compressed, compressed_length);
	snappy_free_env(&env);
}

int main() {
//	int n_sms = atoi(argv[1]);
//	int sms_size = atoi(argv[2]);
	// init_service();

	char *source = NULL;
	FILE *fp = fopen("bin/input/test.txt", "r");
	if (fp != NULL) {
		if (fseek(fp, 0L, SEEK_END) == 0) {
			long bufsize = ftell(fp);

			if (bufsize == -1) { /* Error */ }

			source = malloc(sizeof(char) * (bufsize + 1));

			if (fseek(fp, 0L, SEEK_SET) != 0) { /* Error */ }

			size_t source_len = fread(source, sizeof(char), bufsize, fp);

			if (ferror(fp) != 0) {
				fputs("Error reading file", stderr);
			} // else {
			// 	source[source_len++] = '\0';
			// }

			/* Compress */
			size_t compressed_len = snappy_max_compressed_length(source_len);
			char *compressed = malloc(sizeof(char) * compressed_len);
			compress(source, bufsize, compressed, &compressed_len);

			/* Verify Compression */
			/* Uncompress to check */
			// size_t uncompressed_len;
			// snappy_uncompressed_length(compressed, compressed_len,
			// 						   &uncompressed_len);
			// char *uncompressed = malloc(sizeof(char) * uncompressed_len);
			// snappy_uncompress(compressed, compressed_len, uncompressed);

			// printf("source_len=%lu\n", source_len);
			// printf("uncompressed_len=%lu, compressed_len=%lu\n",
			// 	   uncompressed_len, compressed_len);

			// /* Write the uncompressed file to compare */
			// FILE *output_fp = fopen("bin/input/test_uncompressed.txt", "w");
			// fprintf(output_fp, "%s", uncompressed);
			// fclose(output_fp);
		}
		fclose(fp);
	}
	free(source);


	printf("Hello!\n");
}