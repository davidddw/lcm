#define _GNU_SOURCE
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <libxml/parser.h>
#include <curl/curl.h>

#include <xen/api/xen_all.h>
#include <xen/api/xen_all.h>
#include <lc_global.h>
#include "vm/vm_host.h"
#include "vm/vm_template.h"
#include "message/msg_lcm2dri.h"
#include "lc_queue.h"
#include "lc_db.h"
#include "lc_db_host.h"
#include "lc_db_vm.h"
#include "lc_utils.h"
#include "lc_xen.h"
#include "lc_db_vm.h"
#include "lc_db_errno.h"
#include "lc_db_log.h"
#include "lc_db_pool.h"

#define VDI_UUID_GET_STR    "xe -s %s -p 443 -u %s -pw %s vbd-list vm-name-label=%s type=Disk userdevice=%d params=vdi-uuid --minimal"
#define VDI_PARAM_GET_STR   "xe -s %s -p 443 -u %s -pw %s vdi-param-get uuid=%s param-name=%s --minimal"
#define SR_PARAM_GET_STR    "xe -s %s -p 443 -u %s -pw %s sr-list params=%s name-label=%s --minimal"
#define HOST_DISK_WARNING_USAGE        6018

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

#if 0
    printf("\n\n---(tid=%lu)Data to server: --- %s -------\n",
           pthread_self(), url);
    printf("%s\n",((char*) data));
    fflush(stdout);
#endif
    if (url == NULL) {
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
