#ifndef TINYFILE_API_H
#define TINYFILE_API_H

/* API error codes */
static int TINYFILE_INIT_ERROR = 1;
static int TINYFILE_EXIT_ERROR = 2;
static int TINYFILE_CREATE_QUEUE_ERROR = 3;
static int TINYFILE_CREATE_SHM_ERROR = 4;
static int TINYFILE_DESTROY_SHM_ERROR = 5;

/* Initialize API and exit */
extern int tinyfile_init();

extern int tinyfile_exit();


/* Synchronous API */
extern int tinyfile_sync(ipc_arg *arg, ipc_service service, ipc_request_priority priority, ipc_arg *out);


/* Asynchronous API */
extern int tinyfile_async(ipc_arg *arg, ipc_service service, ipc_request_priority priority, ipc_request_key *key);


/* Wait for asynchronous request to complete */
extern int tinyfile_async_wait(ipc_request_key key, ipc_arg *arg);


/* Join on a group of asynchronous requests */
extern int tinyfile_async_join(ipc_request_key key, ipc_arg *arg);


/* Call a callback function on each key and argument once request is completed */
extern int tinyfile_async_map(ipc_request_key *keys, int size, void (*fn)(ipc_request_key, ipc_arg *));

#endif //TINYFILE_API_H
