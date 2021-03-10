#ifndef TINYFILE_SERVER_H
#define TINYFILE_SERVER_H

#include <mqueue.h>
#include <pthread.h>
#include <unistd.h>

#include "messages.h"

// Number of worker threads for a single client
#define THREADS_PER_CLIENT 10


/*
 * Client description as seen by server
 */
typedef struct client {
    // Client pid
    pid_t pid;

    // Queues
    mqd_t send_q;
    mqd_t recv_q;

    // Shared Memory
    char *shm_addr;
    size_t shm_size;
    char shm_name[100];

    // Data regarding client's worker threads
    int stop_client_threads;
    int num_requests_started;
    int num_requests_completed;
    pthread_mutex_t threads_mutex;
    pthread_t workers[THREADS_PER_CLIENT];
} client_t;


/*
 * Doubly linked list of clients
 */
typedef struct client_node {
    client_t client;
    struct client_node *tail;
    struct client_node *next;
    struct client_node *prev;
} client_list_t;


/* Client list management */
client_list_t *find_client(int pid);

void remove_client(client_list_t *node);

void append_client(client_list_t *node);


/* Client registration functions */
int register_client(tinyfile_registry_entry_t *reg);

int unregister_client(int pid, int close);


/* POSIX IPC setup and cleanup */
void open_shm(tinyfile_registry_entry_t *reg, client_t *client);

void *resize_shm(void *c);

void init_server();

void exit_server();


/* Server API functions */
int compress_service(tinyfile_arg_t *arg);

void handle_request(tinyfile_request_t *req, client_t *client);

#endif // TINYFILE_SERVER_H
