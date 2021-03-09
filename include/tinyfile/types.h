#ifndef TINYFILE_TYPES_H
#define TINYFILE_TYPES_H

typedef struct tinyfile_arg_t {
    char source_file_path[150];
    char compressed_file_path[150];
} tinyfile_arg_t;

typedef enum tinyfile_service_t {
    TINYFILE_COMPRESS
} tinyfile_service_t;

typedef enum tinyfile_registry_cmd_t {
    TINYFILE_CLIENT_REGISTER,
    TINYFILE_CLIENT_UNREGISTER,
    TINYFILE_CLIENT_CLOSE,
    TINYFILE_SERVER_CLOSE
} tinyfile_registry_cmd_t;

// Unique entry index per request to retrieve result from shared memory
typedef int tinyfile_request_entry_idx_t;

// Priority of request
typedef unsigned int tinyfile_request_priority_t;

#endif //TINYFILE_TYPES_H
