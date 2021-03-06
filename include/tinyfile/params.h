//
// Created by parallels on 3/4/21.
//

#ifndef TINYFILE_PARAMS_H
#define TINYFILE_PARAMS_H

static char *TINYFILE_REGISTRY_QUEUE = "/tinyfile-registryq";

static char *TINYFILE_SENDQ_PREFIX = "/tinyfile-sendq-";
static char *TINYFILE_RECVQ_PREFIX = "/tinyfile-recvq-";

static char *TINYFILE_SHM_PREFIX = "/tinyfile-shm-";

static int TINYFILE_SHM_SIZE = 1024;
static int TINYFILE_SHM_MAX_SIZE = 1024 * 1024;

#endif //TINYFILE_PARAMS_H
