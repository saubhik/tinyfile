#include <stdio.h>
#include <mqueue.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>

#include "tinyfile/params.h"
#include "tinyfile/api.h"
#include "tinyfile/messages.h"

/* Global registry queue */
mqd_t global_registry;
tinyfile_registry_entry_t tinyfile_registry_entry;

/* Current request ID */
volatile int global_request_id = 0;
pthread_spinlock_t global_request_lock;

/* TODO: Mutex to sync multiple threads during request key search */
pthread_mutex_t global_request_key_mutex;

/* Check whether library is initialised */
volatile int INIT = 0;

struct client {
    mqd_t send_q;
    mqd_t recv_q;

    char send_q_name[100];
    char recv_q_name[100];

    char *shm_addr;
    char shm_name[100];
    int shm_fd;
    size_t shm_size;
    pthread_mutex_t shm_resize_lock;
} client;

int create_queues() {
    pid_t pid = getpid();

    int length;

    /* Set send queue name */
    length = sprintf(client.send_q_name, "%s", TINYFILE_SENDQ_PREFIX);
    sprintf(client.send_q_name + length, "%ld", (long) pid);

    /* Set recv queue name */
    length = sprintf(client.recv_q_name, "%s", TINYFILE_RECVQ_PREFIX);
    sprintf(client.recv_q_name + length, "%ld", (long) pid);

    struct mq_attr attr;
    attr.mq_maxmsg = 10; // TODO: Check this.

    /* Create send queue for requests from client */
    attr.mq_msgsize = sizeof(tinyfile_request_t);
    client.send_q = mq_open(client.send_q_name, O_EXCL | O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR, &attr);
    if (client.send_q == (mqd_t) -1) {
        fprintf(stderr, "FATAL: mq_open(send_q_name) failed in create_queues\n");
        return TINYFILE_CREATE_QUEUE_ERROR;
    }

    /* Create receive queue for responses to client */
    attr.mq_msgsize = sizeof(tinyfile_response_t);
    client.recv_q = mq_open(client.recv_q_name, O_EXCL | O_CREAT | O_RDONLY, S_IRUSR | S_IWUSR, &attr);
    if (client.recv_q == (mqd_t) -1) {
        fprintf(stderr, "FATAL: mq_open(recv_q_name) failed in create_queues\n");
        return TINYFILE_CREATE_QUEUE_ERROR;
    }

    return 0;
}

void destroy_queues() {
    mq_close(client.send_q);
    mq_unlink(client.send_q_name);

    mq_close(client.recv_q);
    mq_unlink(client.recv_q_name);
}

int create_shm() {
    pid_t pid = getpid();

    int length;
    length = sprintf(client.shm_name, "%s", TINYFILE_SHM_PREFIX);
    sprintf(client.shm_name + length, "%ld", (long) pid);

    client.shm_fd = shm_open(client.shm_name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (client.shm_fd == -1) {
        fprintf(stderr, "FATAL: shm_open() failed in create_shm()\n");
        return TINYFILE_CREATE_SHM_ERROR;
    }

    client.shm_size = sizeof(tinyfile_shared_entry_t) * TINYFILE_SHM_SIZE;
    ftruncate(client.shm_fd, client.shm_size);

    client.shm_addr = mmap(NULL, client.shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, client.shm_fd, 0);
    if (client.shm_addr == MAP_FAILED) {
        fprintf(stderr, "FATAL: mmap() failed in create_shm()\n");
        return TINYFILE_CREATE_SHM_ERROR;
    }

    tinyfile_shared_entry_t shared_entry;
    shared_entry.used = 0;
    shared_entry.done = 0;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

    int i;
    for (i = 0; i < TINYFILE_SHM_SIZE; ++i) {
        /* Copy shared_entry object to shared memory region */
        memcpy(client.shm_addr + i * sizeof(tinyfile_shared_entry_t), &shared_entry, sizeof(tinyfile_shared_entry_t));

        /* Init the mutexes in each shared_entry object */
        tinyfile_shared_entry_t *entry =
                (tinyfile_shared_entry_t *) client.shm_addr + i * sizeof(tinyfile_shared_entry_t);
        pthread_mutex_init(&entry->mutex, &attr);
    }

    pthread_mutexattr_destroy(&attr);

    client.shm_resize_lock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;

    return 0;
}

int destroy_shm() {
    close(client.shm_fd);

    if (shm_unlink(client.shm_name)) {
        fprintf(stderr, "FATAL: shm_unlink() failed in destroy_shm()\n");
        return TINYFILE_DESTROY_SHM_ERROR;
    }

    munmap(client.shm_addr, client.shm_size);

    pthread_mutex_destroy(&client.shm_resize_lock);

    return 0;
}

int async_req_q_init() {}

int async_req_q_destroy() {}

int tinyfile_init() {
    int error;

    if (INIT)
        return TINYFILE_INIT_ERROR;

    global_registry = mq_open(TINYFILE_REGISTRY_QUEUE, O_RDWR);

    if (global_registry == (mqd_t) -1) {
        fprintf(stderr, "FATAL: mq_open(global_registry) failed during init\n");
        return TINYFILE_INIT_ERROR;
    }

    error = create_queues();
    if (error) return error;

    error = create_shm();
    if (error) return error;

    tinyfile_registry_entry.pid = getpid();
    tinyfile_registry_entry.cmd = TINYFILE_CLIENT_REGISTER;
    strncpy(tinyfile_registry_entry.send_q_name, client.send_q_name, 100);
    strncpy(tinyfile_registry_entry.recv_q_name, client.recv_q_name, 100);
    strncpy(tinyfile_registry_entry.shm_name, client.shm_name, 100);

    if (mq_send(global_registry, (const char *) &tinyfile_registry_entry, sizeof(tinyfile_registry_entry), 1)) {
        fprintf(stderr, "FATAL: mq_send(global_registry) failed during init\n");
        return TINYFILE_INIT_ERROR;
    }

    global_request_id = 0;
    pthread_spin_init(&global_request_lock, PTHREAD_PROCESS_PRIVATE);

    global_request_key_mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;

    async_req_q_init();

    INIT = 1;
    return 0;
}

int tinyfile_exit() {
    if (!INIT)
        return TINYFILE_EXIT_ERROR;

    tinyfile_registry_entry.cmd = TINYFILE_CLIENT_UNREGISTER;

    if (mq_send(global_registry, (const char *) &tinyfile_registry_entry, sizeof(tinyfile_registry_entry), 1)) {
        fprintf(stderr, "FATAL: mq_send(global_registry) failed during exit\n");
        return TINYFILE_EXIT_ERROR;
    }

    async_req_q_destroy();
    destroy_queues();
    destroy_shm();

    pthread_spin_destroy(&global_request_lock);

    pthread_mutex_destroy(&global_request_key_mutex);

    INIT = 0;

    return 0;
}
