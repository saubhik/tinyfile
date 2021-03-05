#ifndef TINYFILE_SERVER_H
#define TINYFILE_SERVER_H

#include <mqueue.h>
#include <pthread.h>
#include <unistd.h>

// Number of worker threads for a single client
#define THREADS_PER_CLIENT 10


/*
 * Client description as seen by server
 */
typedef struct __client {
    // Client pid
    pid_t pid;

    // Queues
    mqd_t send_queue;
    mqd_t recv_queue;

    // Shared Memory
    char *shm_addr;
    size_t shm_size;
    char shm_name[100];

    // About client worker threads
    int stop_client_threads;
    int num_threads_started;
    int num_threads_completed;
    pthread_mutex_t threads_mutex;
    pthread_cond_t threads_cond;
    pthread_t workers[THREADS_PER_CLIENT];
} client;


/*
 * Doubly linked list of clients
 */
typedef struct __client_node {
    client client;
    struct __client_node *tail;
    struct __client_node *next;
    struct __client_node *prev;
} client_list;


/* Client list management */
client_list *find_client(int pid);

void remove_client(client_list *node);

void append_client(client_list *node);


/* Client registration functions */
int register_client(ipc_registry *reg);

int unregister_client(int pid, int close);


/* POSIX IPC setup and cleanup */
void open_shm_object(ipc_registry *reg, client *client);

void *resize_shm_object(void *c);

void init_server();

void exit_server();


/* Server API functions */
void mul_service(ipc_mul_arg *arg);

void handle_request(ipc_request *req, client *client);

#endif // TINYFILE_SERVER_H
