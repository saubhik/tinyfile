#include <stdio.h>
#include <mqueue.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <malloc.h>

#include "tinyfile/params.h"
#include "tinyfile/api.h"
#include "tinyfile/messages.h"

/* Global registry queue */
mqd_t global_registry;
tinyfile_registry_entry_t tinyfile_registry_entry;

/* Current request ID */
volatile int global_request_id = 0;
pthread_spinlock_t global_request_lock;

/* Synchronize multiple client threads to access shm */
pthread_mutex_t global_request_entry_idx_mutex;

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

typedef struct request_q {
    tinyfile_request_t request;
    struct request_q *next;
} request_q;

struct request_q_params {  // TODO: Change name?
    pthread_t worker;
    int stop_worker;  // TODO: Change name?
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    request_q *queue;
} request_q_params;

void request_q_enqueue(tinyfile_request_t *request) {
    request_q *entry = malloc(sizeof(request_q));
    entry->request = *request;
    entry->next = NULL;

    pthread_mutex_lock(&request_q_params.mutex);

    /* Insert request after all higher priority request */
    request_q *curr = request_q_params.queue, *prev = NULL;
    while (curr != NULL && curr->request.priority >= request->priority) {
        prev = curr;
        curr = curr->next;
    }

    if (prev == NULL) {
        /* List has no requests or 1 request with lower priority */
        entry->next = curr;
        request_q_params.queue = entry;
    } else {
        entry->next = curr;
        prev->next = entry;
    }

    pthread_mutex_unlock(&request_q_params.mutex);
}

int request_q_dequeue(tinyfile_request_t *request) {
    pthread_mutex_lock(&request_q_params.mutex);

    request_q *head = request_q_params.queue;

    if (head == NULL) {
        pthread_mutex_unlock(&request_q_params.mutex);
        return -1;
    }

    *request = head->request;
    request_q_params.queue = head->next;

    pthread_mutex_unlock(&request_q_params.mutex);

    free(head);

    return 0;
}

void *request_q_handler(__attribute__((unused)) void *p) {
    tinyfile_request_t request;

    while (!request_q_params.stop_worker) {
        if (!request_q_dequeue(&request)) {
            if (mq_send(client.send_q, (char *) &request, sizeof(tinyfile_request_t), request.priority)) {
                fprintf(stderr, "FATAL: mq_send() failed in request queue worker\n");
            }
        }
    }

    return NULL;
}

void async_req_q_init() {
    request_q_params.mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    request_q_params.cond = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
    request_q_params.queue = NULL;
    request_q_params.stop_worker = 0;
    pthread_create(&request_q_params.worker, NULL, request_q_handler, NULL);
}

void async_req_q_destroy() {
    pthread_mutex_lock(&request_q_params.mutex);
    request_q *head = request_q_params.queue, *next;
    while (head != NULL) {
        next = head->next;
        free(head);
        head = next;
    }
    pthread_mutex_unlock(&request_q_params.mutex);

    /* Stop the request queue worker thread */
    request_q_params.stop_worker = 1;
    pthread_join(request_q_params.worker, NULL);

    pthread_mutex_destroy(&request_q_params.mutex);
    pthread_cond_destroy(&request_q_params.cond);
}

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

    global_request_entry_idx_mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;

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

    pthread_mutex_destroy(&global_request_entry_idx_mutex);

    INIT = 0;

    return 0;
}

int resize_shm() {
    /* Increase by TINYFILE_SHM_SIZE slots */
    size_t new_shm_size =
            ((client.shm_size / sizeof(tinyfile_shared_entry_t)) + TINYFILE_SHM_SIZE) * sizeof(tinyfile_shared_entry_t);

    /* Don't send requests to server for now */
    request_q_params.stop_worker = 1;
    pthread_join(request_q_params.worker, NULL);

    if (new_shm_size > TINYFILE_SHM_MAX_SIZE) {
        /* Do not resize beyond shm max size */
        request_q_params.stop_worker = 0;
        pthread_create(&request_q_params.worker, NULL, request_q_handler, NULL);
        return 0;
    }

    pthread_mutex_lock(&client.shm_resize_lock);

    int fd = shm_open(client.shm_name, O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        fprintf(stderr, "FATAL: shm_open() failed in resize_shm()\n");
        return TINYFILE_RESIZE_SHM_ERROR;
    }

    ftruncate(fd, new_shm_size);

    char *new_shm_addr = mmap(NULL, new_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (new_shm_addr == MAP_FAILED) {
        fprintf(stderr, "FATAL: mmap() failed in resize_shm()\n");
        return TINYFILE_RESIZE_SHM_ERROR;
    }

    /* Notify server about shm resize, with high priority message */
    tinyfile_request_t request;
    request.request_id = -1;
    mq_send(client.send_q, (char *) &request, sizeof(tinyfile_request_t), 100);

    /* Get response from server */
    char *buf = malloc(sizeof(tinyfile_response_t));
    mq_receive(client.recv_q, (char *) buf, sizeof(tinyfile_response_t), NULL);
    tinyfile_response_t *response = (tinyfile_response_t *) buf;

    if (response->request_id == -1) {
        munmap(client.shm_addr, client.shm_size);

        client.shm_addr = new_shm_addr;
        client.shm_size = new_shm_size;
        client.shm_fd = fd;

        pthread_mutex_unlock(&client.shm_resize_lock);

        /* Start async request queue worker */
        request_q_params.stop_worker = 0;
        pthread_create(&request_q_params.worker, NULL, request_q_handler, NULL);

        return 0;
    }

    pthread_mutex_unlock(&client.shm_resize_lock);

    return TINYFILE_RESIZE_SHM_ERROR;
}

/*
 * Find the index of shared entry in the shared memory region.
 * This is offset from the base address of the shared memory region.
 * This also calls procedure to resizes shm if the shm is half full.
 */
int find_shared_entry(tinyfile_arg_t *arg) {
    int idx = 0;
    tinyfile_shared_entry_t *shared_entry;

    size_t shm_size = client.shm_size / sizeof(tinyfile_shared_entry_t);
    int half_shm_size = (int) (shm_size / 2);

    while (1) {
        /* Resize shm when half full */
        if (idx == half_shm_size)
            resize_shm();

        shared_entry = (tinyfile_shared_entry_t *) (client.shm_addr + idx * sizeof(tinyfile_shared_entry_t));

        pthread_mutex_lock(&shared_entry->mutex);

        if (!shared_entry->used) {
            shared_entry->arg = *arg;
            shared_entry->used = 1;
            shared_entry->done = 0;

            pthread_mutex_unlock(&shared_entry->mutex);

            break;
        }

        pthread_mutex_unlock(&shared_entry->mutex);

        idx = (idx + 1) % (int) shm_size;
    }

    return idx;
}

int prepare_request(tinyfile_arg_t *arg, tinyfile_service_t service, tinyfile_request_priority_t priority,
                    tinyfile_request_t *req) {
    req->service = service;
    req->pid = getpid();

    if (priority < 0 || priority > 31)
        return TINYFILE_PRIORITY_ERROR;

    req->priority = priority;

    pthread_spin_lock(&global_request_lock);
    req->request_id = global_request_id++;
    pthread_spin_unlock(&global_request_lock);

    pthread_mutex_lock(&global_request_entry_idx_mutex);
    req->entry_idx = find_shared_entry(arg);
    pthread_mutex_unlock(&global_request_entry_idx_mutex);

    return 0;
}

/*
 * Check is request is served by server by looking at shared memory entry.
 */
int is_request_done(tinyfile_request_entry_idx_t entry_idx, tinyfile_arg_t *arg) {
    /* Make sure resizing is not occurring since client.shm_addr can change */
    pthread_mutex_lock(&client.shm_resize_lock);

    tinyfile_shared_entry_t *entry = (tinyfile_shared_entry_t *) (client.shm_addr +
                                                                  entry_idx * sizeof(tinyfile_shared_entry_t));

    pthread_mutex_unlock(&client.shm_resize_lock);

    int done = 0;

    pthread_mutex_lock(&entry->mutex);

    if (entry->done == 1) {
        /* Copy the result and make the shared entry ready for re-use */
        entry->used = 0;
        if (arg != NULL) *arg = entry->arg;
        entry->done = 0;
        done = 1;
    }

    pthread_mutex_unlock(&entry->mutex);

    return done;
}

/*
 * Wait for response from Tinyfile server.
 * Implemented using Exponential Backoff.
 */
void recv_response(tinyfile_request_entry_idx_t entry_idx, tinyfile_arg_t *arg) {
    unsigned int backoff = 1;

    while (1) {
        if (is_request_done(entry_idx, arg)) break;
        backoff *= 2;
        usleep(backoff);
    }
}

int tinyfile_sync(tinyfile_arg_t *arg, tinyfile_service_t service, tinyfile_request_priority_t priority,
                  tinyfile_arg_t *out) {
    int error;

    /* Prepare the request */
    tinyfile_request_t req;
    error = prepare_request(arg, service, priority, &req);
    if (error) return error;

    request_q_enqueue(&req);

    /* Wait for response */
    recv_response(req.entry_idx, out);

    return 0;
}

int tinyfile_async(tinyfile_arg_t *arg, tinyfile_service_t service, tinyfile_request_priority_t priority,
                   tinyfile_request_entry_idx_t *entry_idx) {
    tinyfile_request_t req;
    int error = prepare_request(arg, service, priority, &req);
    if (error) return error;

    *entry_idx = req.entry_idx;

    request_q_enqueue(&req);

    return 0;
}

int tinyfile_async_wait(tinyfile_request_entry_idx_t entry_idx, tinyfile_arg_t *arg) {
    recv_response(entry_idx, arg);

    return 0;
}

int tinyfile_async_join(tinyfile_request_entry_idx_t *entry_idxs, tinyfile_arg_t *args, int num_requests) {
    if (num_requests <= 0)
        return -1;

    int i = 0, done = 0;

    char *requests_done = malloc(sizeof(char) * num_requests);
    memset(requests_done, 0, (size_t) num_requests);

    while (done < num_requests) {
        if (!requests_done[i]) {
            requests_done[i] = (char) is_request_done(entry_idxs[i], args + i);
            done += requests_done[i];
        }

        i = (i + 1) % num_requests;
    }

    free(requests_done);

    return 0;
}
