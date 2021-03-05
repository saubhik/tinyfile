#ifndef TINYFILE_TYPES_H
#define TINYFILE_TYPES_H

typedef struct tinyfile_mul_arg_t {
    int x, y, res;
} tinyfile_mul_arg_t;

typedef union tinyfile_arg_t {
    tinyfile_mul_arg_t mul;
} tinyfile_arg_t;

typedef enum tinyfile_service_t {
    IPC_MUL
} tinyfile_service_t;

typedef enum tinyfile_registry_cmd_t {
    TINYFILE_CLIENT_REGISTER,
    TINYFILE_CLIENT_UNREGISTER,
    TINYFILE_CLIENT_CLOSE,
    TINYFILE_SERVER_CLOSE
} tinyfile_registry_cmd_t;

// Unique key per request to retrieve result from shared memory
typedef int tinyfile_request_key_t;

// Priority of request
typedef unsigned int tinyfile_request_priority_t;

#endif //TINYFILE_TYPES_H
