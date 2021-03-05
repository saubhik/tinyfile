#ifndef TINYFILE_MESSAGES_H
#define TINYFILE_MESSAGES_H

#include <pthread.h>
#include "types.h"

typedef struct tinyfile_registry_entry_t {
    // Register or unregister current client
    tinyfile_registry_cmd_t cmd;

    // Client's pid
    pid_t pid;

    // Send and receive queue names
    char send_q_name[100];
    char recv_q_name[100];

    // Name of shared memory
    char shm_name[100];
} tinyfile_registry_entry_t;

/*
 * IPC request from client to server to initiate a service
 */
typedef struct tinyfile_request_t {
    tinyfile_service_t service;
    tinyfile_request_priority_t priority;
    int request_id;
    int entry_idx;
    int pid;
} tinyfile_request_t;

/*
 * Entry in the shared memory segment accessible by both client and server.
 * This is used to pass requests and arguments back and forth.
 */
typedef struct tinyfile_shared_entry_t {
    int used;
    int done;
    tinyfile_arg_t arg;
    pthread_mutex_t mutex;
} tinyfile_shared_entry_t;

/*
 * IPC response from server to client.
 * This is sent once request completes.
 */
typedef struct tinyfile_response_t {
    int request_id;
    int entry_idx;
    tinyfile_arg_t arg;
} tinyfile_response_t;

#endif //TINYFILE_MESSAGES_H
