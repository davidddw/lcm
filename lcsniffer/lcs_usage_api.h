/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Zhou Qi
 * Finish Date: 2014-01-16
 *
 */
#ifndef _LCS_USAGE_API_H
#define _LCS_USAGE_API_H

#include <pthread.h>
#include <time.h>
#include "lc_agent_msg.h"
#include "lc_snf.h"

#define MAX_ID_COUNT  32
#define MAX_BUF_COUNT 2
#define MAX_DATA_BUF_LEN (1 << 15)
#define SECONDS_PER_MINUTES 60

void *api_thread(void *msg);

typedef struct api_msg_s {
    char            *buf;
    int             len;
    pthread_mutex_t lock;
} api_msg_t;

typedef struct api_msg_buf_s {
    int type;
    int id;
    time_t time;
    api_msg_t msg[MAX_BUF_COUNT];
    int location;
} api_msg_buf_t;

typedef struct api_arg_s {
    int  fd;
    int  idx;
    int  location;
    host_t hostinfo;
    vm_info_t vm_info;
    int  nvif;
    agent_interface_id_t vintf_infos[LC_VM_MAX_VIF];
} api_arg_t;

extern api_msg_buf_t g_msg[MAX_ID_COUNT];

static inline void lcs_g_msg_init() {
    int i, j;

    for (i = 0; i < MAX_ID_COUNT; i++) {
        for (j = 0; j< MAX_BUF_COUNT; j++) {
            pthread_mutex_init(&g_msg[i].msg[j].lock, NULL);
        }
    }
    return;
}

static inline void lcs_g_msg_destroy() {
    int i, j;

    for (i = 0; i < MAX_ID_COUNT; i++) {
        for (j = 0; j< MAX_BUF_COUNT; j++) {
            pthread_mutex_destroy(&g_msg[i].msg[j].lock);
        }
    }
    return;
}

static inline int api_msg_buf_init(api_msg_buf_t *data, int type, int id) {
    int i, j;

    for (i = 0; i < MAX_BUF_COUNT; i++) {
        data->msg[i].buf = (char *)malloc(MAX_DATA_BUF_LEN);
        data->msg[i].len = 0;
        if (!data->msg[i].buf) {
            break;
        }
    }
    if (i < MAX_BUF_COUNT) {
        for (j = 0; j < i; j++) {
            free(data->msg[j].buf);
            data->msg[j].buf = NULL;
        }
        return -1;
    }
    data->time = time(NULL);
    data->type = type;
    data->id = id;
    data->location = 0;
    return 0;
}

static inline void api_msg_buf_destroy(api_msg_buf_t *data) {
    int i;

    for (i = 0; i < MAX_BUF_COUNT; i++) {
        pthread_mutex_lock(&data->msg[i].lock);
        free(data->msg[i].buf);
        data->msg[i].buf = NULL;
        data->msg[i].len = 0;
        pthread_mutex_unlock(&data->msg[i].lock);
    }
    data->type = 0;
    data->id = 0;
    data->time = 0;
    data->location = 0;
    return;
}

#endif /* _LCS_USAGE_API_H */
