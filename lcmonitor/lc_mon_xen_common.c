/*
 * Copyright (c) 2012 Yunshan Networks
 * All right reserved.
 *
 * Filename:lc_mon_xen_common.c
 * Date: 2012-11-26
 *
 * Description: get host/vm info by XAPI
 *
 *
 */

#define _GNU_SOURCE
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <libxml/parser.h>
#include <curl/curl.h>

#include <xen/api/xen_all.h>

#include "lc_xen_monitor.h"
#include "lc_xen.h"
#include "lc_utils.h"

typedef struct xen_comms_s {
    xen_result_func func;
    void *handle;
} xen_comms;


size_t
write_func(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    xen_comms *comms = (xen_comms *)userdata;
    size_t n = size * nmemb;
#if 0
    printf("\n\n---Result from server -----------------------\n");
    printf("%s\n",((char*) ptr));
    fflush(stdout);
#endif
    return (size_t)(comms->func((void *)ptr, n, comms->handle) ? n : 0);
}

static int
call_func(const void *data, size_t len, void *user_handle,
          void *result_handle, xen_result_func result_func, char *url_name)
{
    int i;
    pthread_t tid = pthread_self();
    char url[32];
    CURLcode result = -1;
    (void)user_handle;

    CURL *curl = curl_easy_init();
    if (!curl) {
        return -1;
    }

    xen_comms comms = {
        .func = result_func,
        .handle = result_handle
    };

    memset(url, 0, 32);
    for (i = 0; i < 16; i++) {
        if (pthread_equal(tid, thread_url[i].tid)) {
            strcpy(url, thread_url[i].url);
            break;
        }
    }
    if (strlen(url) == 0) {
        goto cleanup;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
#ifdef CURLOPT_MUTE
    curl_easy_setopt(curl, CURLOPT_MUTE, 1);
#endif
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_func);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &comms);
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, len);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

    result = curl_easy_perform(curl);

cleanup:
    curl_easy_cleanup(curl);

    return result;
}

void xen_common_init(void)
{
    xmlInitParser();
    xmlKeepBlanksDefault(0);
    xen_init();
    curl_global_init(CURL_GLOBAL_ALL);
    return;
}

void xen_common_destroy(void)
{
    curl_global_cleanup();
    xen_fini();
    xmlCleanupParser();
    return;
}

xen_session *
xen_create_session(char *server_url, char *username, char *password)
{
    int i;
    xen_session *session = NULL;
    pthread_t tid = pthread_self();

    for (i = 0; i < MAX_THREAD; i++) {
        if (pthread_equal(tid, thread_url[i].tid)) {
           memset(thread_url[i].url, 0, LC_MAX_CURL_LEN);
           strcpy(thread_url[i].url, server_url);
           break;
        }
    }

    session = xen_session_login_with_password(call_func, NULL,
            username, password, xen_api_latest_version, server_url);

    return session;
}

bool lc_host_health_check(char *host_address, xen_session *session)
{
    bool result = 0;
    xen_host_ha_xapi_healthcheck(session, &result);
    if (!result) {
        lc_print_xapi_error("Monitor", "Host", host_address, session, NULL);
    }
    return result;
}

int get_vm_actual_vdi_size(uint64_t *number, char *num)
{
    uint64_t size;
    size = strtoull(num, NULL, 0);
    *number = size/LC_GB;
    return 0;
}

int get_vm_actual_memory(uint32_t *number, char *num)
{
    *number = atoll(num)/LC_MB;
    return 0;
}

int get_cpu_num(int *number, char *num)
{
    *number = atoi(num);
    return 0;
}

int get_vm_cpu_usage (char *result, char *usage)
{
    int i;
    for (i = 0;i < strlen(usage); i++) {
        if (usage[i] == ':') {
            usage[i] = ',';
        }
        if (usage[i] == ';') {
            usage[i] = '#';
        }
    }
    usage[strlen(usage)-1] = '#';
    strncpy(result, usage, strlen(usage));

    return 0;
}
