#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <tinyfile/params.h>
#include <tinyfile/server.h>
#include <sys/mman.h>
#include <string.h>
#include <ftw.h>

#include <snappy-c/snappy.h>

mqd_t global_registry;
pthread_t registry_thread;
static int STOP_REGISTRY = 0;

/* Linked list of clients */
client_list_t *clients = NULL;

void append_client(client_list_t *node) {
    if (clients == NULL) {
        node->next = NULL;
        node->prev = NULL;
        node->tail = node;

        clients = node;
    } else {
        client_list_t *tail = clients->tail;

        node->prev = tail;
        node->next = NULL;
        node->tail = node;

        tail->next = node;

        clients->tail = node;
    }
}

void remove_client(client_list_t *node) {
    if (node != NULL) {
        if (node->prev == NULL && node->next == NULL)
            clients = NULL;
        else {
            if (node->prev != NULL)
                node->prev->next = node->next;

            if (node->next != NULL)
                node->next->prev = node->prev;
            else
                clients->tail = node->prev;
        }
    }
}

int compress_service(tinyfile_arg_t *arg) {
    FILE *fp;

    /* Read source file to be compressed */
    if ((fp = fopen(arg->source_file_path, "r")) == NULL)
        return -1;

    if (fseek(fp, 0L, SEEK_END) != 0)
        return -1;

    long buf_size = ftell(fp);
    if (buf_size == -1)
        return -1;

    if (fseek(fp, 0L, SEEK_SET) != 0)
        return -1;

    char *source = malloc(sizeof(char) * (buf_size + 1));
    size_t source_len = fread(source, sizeof(char), buf_size, fp);
    if (ferror(fp) != 0)
        return -1;

    fclose(fp);

    /* Compression using snappy-c */
    size_t compressed_len = snappy_max_compressed_length(source_len);
    char *compressed = malloc(sizeof(char) * compressed_len);
    struct snappy_env env;
    snappy_init_env(&env);
    snappy_compress(&env, source, buf_size, compressed, &compressed_len);
    snappy_free_env(&env);

    /* Generate compressed file path */
    strcpy(arg->compressed_file_path, arg->source_file_path);
    strcat(arg->compressed_file_path, ".compressed");

    /* Write compressed buffer to compressed file path */
    if ((fp = fopen(arg->compressed_file_path, "w")) == NULL)
        return -1;

    fprintf(fp, "%s", compressed);

    fclose(fp);

    return 0;
}

client_list_t *find_client(int pid) {
    client_list_t *list = clients;
    while (list != NULL && list->client.pid != pid)
        list = list->next;
    return list;
}

void handle_request(tinyfile_request_t *req, client_t *client) {
    tinyfile_shared_entry_t *shared_entry = (tinyfile_shared_entry_t *) (client->shm_addr +
                                                                         req->entry_idx *
                                                                         sizeof(tinyfile_shared_entry_t));
    tinyfile_arg_t *arg = &shared_entry->arg;

    switch (req->service) {
        case TINYFILE_COMPRESS:
            if (compress_service(arg))
                fprintf(stderr, "ERROR: Error during file handling for client %d\n", req->pid);
            break;
        default:
            fprintf(stderr, "ERROR: Invalid service in request by client %d\n", req->pid);
    }

    pthread_mutex_lock(&shared_entry->mutex);
    shared_entry->done = 1;
    pthread_mutex_unlock(&shared_entry->mutex);
}

void *service_worker(void *data) {
    client_t *client = data;

    char *buf = malloc(sizeof(tinyfile_request_t));
    tinyfile_request_t *request;
    unsigned int priority;

    struct timespec ts; // Timeout for message queue operations

    while (!client->stop_client_threads) {
        /* 1 ms timeout to get request */
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 1000000;
        ts.tv_sec = 0;

        if (mq_timedreceive(client->recv_q, buf, sizeof(tinyfile_request_t), &priority, &ts) != -1) {
            request = (tinyfile_request_t *) buf;

            if (request->request_id == -1) {
                /* Resize shm request from client */
                pthread_t resize_thread;
                pthread_create(&resize_thread, NULL, resize_shm, (void *) client);
                pthread_detach(resize_thread);
            } else {
                pthread_mutex_lock(&client->threads_mutex);
                client->num_requests_started++;
                pthread_mutex_unlock(&client->threads_mutex);

                handle_request(request, client);

                pthread_mutex_lock(&client->threads_mutex);
                client->num_requests_completed++;
                pthread_mutex_unlock(&client->threads_mutex);
            }
        }
    }

    return NULL;
}

void start_worker_threads(client_t *client) {
    client->stop_client_threads = 0;
    int i;
    for (i = 0; i < THREADS_PER_CLIENT; ++i)
        pthread_create(&client->workers[i], NULL, service_worker, (void *) client);
}

void init_worker_threads(client_t *client) {
    client->stop_client_threads = 0;
    client->num_requests_started = 0;
    client->num_requests_completed = 0;
    client->threads_mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;

    start_worker_threads(client);
}

void join_worker_threads(client_t *client) {
    client->stop_client_threads = 1;

    /* Join all threads except myself */
    int i;
    for (i = 0; i < THREADS_PER_CLIENT; ++i) {
        if (client->workers[i] != pthread_self())
            pthread_join(client->workers[i], NULL);
    }
}

void open_shm(tinyfile_registry_entry_t *registry_entry, client_t *client) {
    int fd;

    if ((fd = shm_open(registry_entry->shm_name, O_RDWR, S_IRUSR | S_IWUSR)) == -1) {
        fprintf(stderr, "ERROR: shm_open() failed\n");
        exit(EXIT_FAILURE);
    }

    memcpy(client->shm_name, registry_entry->shm_name, 100);

    struct stat s;
    fstat(fd, &s);
    client->shm_size = (size_t) s.st_size;

    client->shm_addr = mmap(NULL, client->shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    close(fd);
}

void *resize_shm(void *data) {
    client_t *client = data;

    int fd;

    if ((fd = shm_open(client->shm_name, O_RDWR, S_IRUSR | S_IWUSR)) == -1) {
        fprintf(stderr, "ERROR: shm_open() failed in resize_shm() for client %d\n", client->pid);
        exit(EXIT_FAILURE);
    }

    struct stat s;
    fstat(fd, &s);
    size_t new_shm_size = (size_t) s.st_size;

    char *new_shm_addr = mmap(NULL, new_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    tinyfile_shared_entry_t shared_entry;
    shared_entry.used = 0;
    shared_entry.done = 0;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

    /* Initialize the new shared memory segment */
    unsigned int i;
    for (i = (int) client->shm_size / sizeof(tinyfile_shared_entry_t);
         i < new_shm_size / sizeof(tinyfile_shared_entry_t); ++i) {
        memcpy(new_shm_addr + i * sizeof(tinyfile_shared_entry_t), &shared_entry, sizeof(tinyfile_shared_entry_t));

        tinyfile_shared_entry_t *entry = (tinyfile_shared_entry_t *) (new_shm_addr +
                                                                      i * sizeof(tinyfile_shared_entry_t));
        pthread_mutex_init(&entry->mutex, &attr);

    }

    pthread_mutexattr_destroy(&attr);

    join_worker_threads(client);

    memcpy(new_shm_addr, client->shm_addr, client->shm_size);

    /* Flush changes since server has most updated copy of shared memory */
    msync(new_shm_addr, client->shm_size, MS_INVALIDATE | MS_SYNC);

    close(fd);

    /* Send response back to client */
    tinyfile_response_t response;
    response.request_id = -1;
    mq_send(client->send_q, (char *) &response, sizeof(tinyfile_response_t), 1);

    /* Unmap old memory */
    munmap(client->shm_addr, client->shm_size);

    client->shm_addr = new_shm_addr;
    client->shm_size = new_shm_size;

    start_worker_threads(client);

    return NULL;
}

int register_client(tinyfile_registry_entry_t *registry_entry) {
    if (find_client(registry_entry->pid) != NULL)
        /* Client already registered */
        return 0;

    client_t client;
    client.pid = registry_entry->pid;

    /* Send to recv_q of client */
    client.send_q = mq_open(registry_entry->recv_q_name, O_RDWR);
    if (client.send_q == (mqd_t) -1) {
        fprintf(stderr, "ERROR: mq_open(registry_entry->send_q_name) failed in register_client()\n");
        exit(EXIT_FAILURE);
    }

    /* Receive from send_q of client */
    client.recv_q = mq_open(registry_entry->send_q_name, O_RDWR);
    if (client.recv_q == (mqd_t) -1) {
        fprintf(stderr, "ERROR: mq_open(registry_entry->recv_q_name) failed in register_client()\n");
        exit(EXIT_FAILURE);
    }

    open_shm(registry_entry, &client);

    client_list_t *node = malloc(sizeof(client_list_t));
    node->client = client;
    append_client(node);

    init_worker_threads(&node->client);

    return 0;
}

void cleanup_worker_threads(client_t *client) {
    pthread_mutex_destroy(&client->threads_mutex);

    int i;
    for (i = 0; i < THREADS_PER_CLIENT; i++)
        pthread_cancel(client->workers[i]);
}

int unregister_client(int pid, int close) {
    client_list_t *node = find_client(pid);
    if (node == NULL) {
        /* Client already unregistered */
        return 0;
    }

    client_t *client = &node->client;

    if (close) {
        /* Send poison pill if server is closing */
        tinyfile_registry_entry_t registry_entry;
        registry_entry.cmd = TINYFILE_SERVER_CLOSE;
        if (mq_send(client->send_q, (char *) &registry_entry, sizeof(registry_entry), 1)) {
            fprintf(stderr, "ERROR: Error in mq_send() during unregister_client() to client %d\n", pid);
        }
    }

    remove_client(node);

    mq_close(client->send_q);
    mq_close(client->recv_q);

    /* Wait for all worker threads to complete */
    while (client->num_requests_started != client->num_requests_completed);

    cleanup_worker_threads(client);

    munmap(client->shm_addr, client->shm_size);

    free(node);

    return 0;
}

static void *registry_handler(__attribute__((unused)) void *p) {
    char *recv_buf = malloc(sizeof(tinyfile_registry_entry_t));
    tinyfile_registry_entry_t *registry_entry;

    /* Timeout for message queue operations */
    struct timespec ts;

    while (!STOP_REGISTRY) {
        /* Receive timeout of 100 ms */
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 100000000;
        ts.tv_sec = 0;

        if (mq_timedreceive(global_registry, recv_buf, sizeof(tinyfile_registry_entry_t), NULL, &ts) == -1)
            continue;

        registry_entry = (tinyfile_registry_entry_t *) recv_buf;
        switch (registry_entry->cmd) {
            case TINYFILE_CLIENT_REGISTER:
                register_client(registry_entry);
                printf("Client %d registered\n", registry_entry->pid);
                break;
            case TINYFILE_CLIENT_UNREGISTER:
            case TINYFILE_CLIENT_CLOSE:
                unregister_client(registry_entry->pid, 0);
                printf("Client %d unregistered\n", registry_entry->pid);
                break;
            default:
                fprintf(stderr, "ERROR: Unknown registry command from client %d\n", registry_entry->pid);
        }
    }

    return NULL;
}

void exit_server() {
    client_list_t *client_list = clients;

    while (client_list != NULL) {
        unregister_client(client_list->client.pid, 1);
        client_list = client_list->next;
    }

    if (global_registry) {
        mq_close(global_registry);
        mq_unlink(TINYFILE_REGISTRY_QUEUE);
    }

    STOP_REGISTRY = 1;
    pthread_join(registry_thread, NULL);
}

void init_server() {
    atexit(exit_server);

    struct mq_attr attr;
    attr.mq_msgsize = sizeof(tinyfile_registry_entry_t);
    attr.mq_maxmsg = 10;

    global_registry = mq_open(TINYFILE_REGISTRY_QUEUE, O_EXCL | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attr);
    if (global_registry == (mqd_t) -1) {
        if (errno == EEXIST) {
            /* Global registry queue already exists */
            mq_unlink(TINYFILE_REGISTRY_QUEUE);
            global_registry = mq_open(TINYFILE_REGISTRY_QUEUE, O_EXCL | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attr);
        } else {
            fprintf(stderr, "FATAL: mq_open(TINYFILE_REGISTRY_QUEUE) failed in init_server()\n");
            exit(EXIT_FAILURE);
        }
    }

    if (pthread_create(&registry_thread, NULL, registry_handler, NULL)) {
        fprintf(stderr, "FATAL: pthread_create(&registry_thread) failed in init_server()\n");
        exit(EXIT_FAILURE);
    }
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char **argv) {
    init_server();

    char input[2];

    printf("Server initialized. Press q to quit.\n");

    while (input[0] != 'q') fgets(input, 2, stdin);

    exit_server();

    printf("Server exited.\n");

    return 0;
}
