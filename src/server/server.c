#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <tinyfile/params.h>
#include <tinyfile/server.h>

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

int register_client(tinyfile_registry_entry_t *registry_entry) {
    if (find_client(registry_entry->pid) != NULL)
        /* Client already registered */
        return 0;

    client_t client;
    client.pid = registry_entry->pid;

    client.send_queue = mq_open(registry_entry->send_q_name, O_RDWR);
    if (client.send_queue == (mqd_t) -1) {
        fprintf(stderr, "ERROR: mq_open(registry_entry->send_q_name) failed in register_client()\n");
        exit(EXIT_FAILURE);
    }

    client.recv_queue = mq_open(registry_entry->recv_q_name, O_RDWR);
    if (client.recv_queue == (mqd_t) -1) {
        fprintf(stderr, "ERROR: mq_open(registry_entry->recv_q_name) failed in register_client()\n");
        exit(EXIT_FAILURE);
    }

    open_shm(registry_entry, &client);

    client_list_t *node = malloc(sizeof(client_list_t));
    node->client = client;
    append_client(node);

    init_worker_threads(&client);

    return 0;
}

int unregister_client(int pid, int close) {
    client_list_t *node = find_client(pid);
    if (node == NULL)
        /* Client already unregistered */
        return 0;

    if (close) {

    }
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

int main(int argc, char **argv) {
    init_server();

    char input[2];

    printf("Server initialized. Press q to quit.\n");

    while (input[0] != 'q') fgets(input, 2, stdin);

    exit_server();

    printf("Server exited.\n");

    return 0;
}
