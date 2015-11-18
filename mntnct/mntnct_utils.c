/*
 * Copyright (c) 2012-2013 Yunshan Networks
 * All right reserved.
 *
 * Filename: mntnct_utils.c
 * Author Name: Xiang Yang
 * Date: 2013-7-18
 *
 * Description: utils for mntnct
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>

#include "mntnct_db.h"
#include "mntnct_utils.h"

#define MNTNCT_CMD_RESULT_DIR   "/var/tmp/"
#define REMOTE_API_TIMEOUT 6

static char domain[MNTNCT_DOMAIN_LEN] = { 0 };

void chomp(char *str)
{
    while (*str) {
        if (*str == '\n') {
            *str = '\0';
            break;
        }
        ++str;
    }
}

int get_param(char *value, int size, char *get_str, ...)
{
    FILE *fp;
    char cmd[512];
    va_list ap;

    memset(cmd, 0, sizeof(cmd));
    va_start(ap, get_str);
    vsprintf(cmd, get_str, ap);
    va_end(ap);
    if ((fp = popen(cmd, "r")) == 0) {
        return -1;
    }
    fgets(value, size - 2, fp);
    chomp(value);
    pclose(fp);

    return 0;
}

int call_system_output(char *raw_cmd, char *result, int result_buf_len)
{
    int err = 1, fp, rd_len;
    const int suffix_len = 64;
    int cmd_len = strlen(raw_cmd) + suffix_len;
    char fname[suffix_len];
    char cmd[cmd_len];
    pthread_t tid = pthread_self();

    if (result) {
        *result = 0;
    }
    if (!raw_cmd || !result) {
        return 1;
    }

    snprintf(fname, suffix_len, MNTNCT_CMD_RESULT_DIR "/mntnct-%lu", tid);
    snprintf(cmd, cmd_len, "%s > %s 2>&1", raw_cmd, fname);
    err = system(cmd);

    if ((fp = open(fname, O_RDONLY)) == -1) {
        fprintf(stderr, "%s: cmd result open failed [%s]\n",
                __FUNCTION__, cmd);
        return 1;
    }
    rd_len = read(fp, result, result_buf_len);
    close(fp);

    if (rd_len < 0) {
        fprintf(stderr, "%s: cmd result read failed [%s]\n",
                __FUNCTION__, cmd);
        return 1;
    } else if (rd_len >= result_buf_len) {
        fprintf(stderr, "%s: cmd result too large, buf_size=%d [%s]\n",
                __FUNCTION__, result_buf_len, cmd);
        snprintf(result, result_buf_len, "command result too large");
        return 1;
    }
    result[rd_len] = 0;

    if (err) {
        fprintf(stderr, "cmd failed [%s]:[%s]\n", raw_cmd, result);
    }

    sprintf(cmd, "rm -f %s", fname);
    system(cmd);

    return err;
}

char *get_domain()
{
    if (!domain[0]) {
        db_select(&domain, MNTNCT_DOMAIN_LEN, 1,
                "sys_configuration", "value", "param_name='domain'");
        if (!domain[0]) {
            return 0;
        }
    }

    return domain;
}

void str_replace(char *str, char dst, char ori)
{
    while (*str) {
        if (*str == ori) {
            *str = dst;
            break;
        }
        ++str;
    }
}

int get_masklen(char *netmask)
{
    uint32_t u_netmask = 0;
    struct in_addr i_netmask;
    int masklen;

    if (!netmask || !netmask[0]) {
        return -1;
    }

    memset(&i_netmask, 0, sizeof i_netmask);
    if (inet_aton(netmask, &i_netmask)) {
        u_netmask = ntohl(i_netmask.s_addr);
    } else {
        return -1;
    }

    masklen = 0;
    while (u_netmask) {
        if (u_netmask & 0x1) {
            ++masklen;
        }
        u_netmask >>= 1;
    }

    return masklen;
}

#define MIN(a, b)                       ((a) < (b) ? (a) : (b))

size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    size_t data_size = size * nmemb;

    if (!data_size || !userp) {
        return data_size;
    }
    memset(userp, 0, CURL_BUFFER_SIZE);
    memcpy(userp, buffer, MIN(data_size, CURL_BUFFER_SIZE - 1));

    return data_size;
}

/* returns length of str */
int generate_uuid(char *buffer, int buflen)
{
    CURL *curl = 0;
    const nx_json * json = 0;
    char tmp[CURL_BUFFER_SIZE];

    if (!buffer || buflen <= 0) {
        return 0;
    }

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error: calling curl_easy_init()\n");
        return 0;
    }

    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:20013/v1/uuid/");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, REMOTE_API_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &tmp[0]);
    if (curl_easy_perform(curl)) {
        fprintf(stderr, "Error: failed in calling uuid API\n");
        return 0;
    }
    curl_easy_cleanup(curl);

    json = nx_json_parse(tmp, 0);
    if (json && nx_json_get(nx_json_get(json, "DATA"),
                            "UUID")->type == NX_JSON_STRING) {
        memset(buffer, 0, buflen);
        strncpy(buffer,
                nx_json_get(nx_json_get(json, "DATA"), "UUID")->text_value,
                buflen - 1);
        return strlen(buffer);
    } else {
        fprintf(stderr, "Error: Invalid JSON response\n");
        return 0;
    }
}

int dump_json_msg(const nx_json *root, char *msg_buf, int buf_len)
{
    int buf_used = 0;
    char s[2] = {0};

    if (!msg_buf) {
        return 0;
    }
    *msg_buf = 0;

    if (!root) {
        return 0;;
    }

    if (root->key) {
        buf_used += snprintf(msg_buf + buf_used, buf_len - buf_used,
                "\"%s\": ", root->key);
    }

    memset(s, 0, sizeof(s));
    switch (root->type) {
        case NX_JSON_NULL:
            break;

        case NX_JSON_OBJECT:
            buf_used += snprintf(msg_buf + buf_used, buf_len - buf_used, "{");
            buf_used += dump_json_msg(root->child, msg_buf + buf_used, buf_len - buf_used);
            buf_used += snprintf(msg_buf + buf_used, buf_len - buf_used,
                    "}%s", root->next ? ", " : "");
            break;

        case NX_JSON_ARRAY:
            buf_used += snprintf(msg_buf + buf_used, buf_len - buf_used, "[");
            buf_used += dump_json_msg(root->child, msg_buf + buf_used, buf_len - buf_used);
            buf_used += snprintf(msg_buf + buf_used, buf_len - buf_used,
                    "]%s", root->next ? ", " : "");
            break;

        case NX_JSON_STRING:
            buf_used += snprintf(msg_buf + buf_used, buf_len - buf_used,
                    "\"%s\"%s", root->text_value, root->next ? ", " : "");
            break;

        case NX_JSON_INTEGER:
            buf_used += snprintf(msg_buf + buf_used, buf_len - buf_used,
                    "%ld%s", root->int_value, root->next ? ", " : "");
            break;

        case NX_JSON_DOUBLE:
            /* should
             * never
             * use
             * double
             * */
            buf_used += snprintf(msg_buf + buf_used, buf_len - buf_used,
                    "%lf%s", root->dbl_value, root->next ? ", " : "");
            break;

        case NX_JSON_BOOL:
            buf_used += snprintf(msg_buf + buf_used, buf_len - buf_used, "%s%s",
                    root->int_value ? "true" : "false", root->next ? ", " : "");
            break;

        default:
            break;
    }

    if (root->next) {
        buf_used += dump_json_msg(root->next, msg_buf + buf_used, buf_len - buf_used);
    }

    return buf_used;
}

static int parse_curl_json_msg(char *msg, const nx_json **root)
{
    const nx_json *opt = NULL, *desc = NULL;

    if (!msg || !root) {
        return -1;
    }

    /* json message */
    *root = nx_json_parse(msg, NULL);
    if (!*root) {
        fprintf(stderr, "Error: Invalid JSON response: %s\n", msg);
        return -1;
    }

    opt = nx_json_get(*root, MNTNCT_JSON_OPT_KEY);
    if (opt->type != NX_JSON_STRING) {
        fprintf(stderr, "Error: Invalid JSON response, no OPT_STATUS: %s\n", msg);
        return -1;
    }

    desc = nx_json_get(*root, MNTNCT_JSON_DESC_KEY);
    if (desc->type != NX_JSON_STRING) {
        fprintf(stderr, "Error: Invalid JSON response, no DESCRIPTION: %s\n", msg);
        return -1;
    }

    return 0;
}

size_t curl_check_result(void *buffer, size_t size, size_t nmemb, void *userp)
{
    int segsize  = size *nmemb;
    char result[MNTNCT_BIG_CMD_RESULT_LEN] = {0};
    const nx_json *root = NULL, *opt = NULL, *desc = NULL;
    int ret;

    memcpy((void *)result, buffer, (size_t)segsize);
    result[segsize] = 0;
    ret = parse_curl_json_msg(result, &root);
    if (ret != 0) {
        fprintf(stderr, "Error: parse_curl_json_msg [%s]", result);
        return 0;
    } else {
        opt = nx_json_get(root, MNTNCT_JSON_OPT_KEY);
        if (strcmp(opt->text_value,
                    MNTNCT_CODE_LABEL(SUCCESS))) {
            desc = nx_json_get(root, MNTNCT_JSON_DESC_KEY);
            fprintf(stderr, "Error: Curl result failed [%s] [%s].\n", opt->text_value, desc->text_value);
            return 0;
        }
    }
    if (userp != NULL && segsize > 0) {
        memset(userp, 0, CURL_BUFFER_SIZE);
        memcpy(userp, buffer, MIN(segsize, CURL_BUFFER_SIZE - 1));
    }

    return segsize;
}

int call_curl_api(char *url, int method, char *data, char *userp)
{
    int ret = 0;
    CURL *curl_handle = NULL;
    CURLcode result;
    struct curl_slist *http_headers = NULL;

    curl_handle = curl_easy_init();
    http_headers = curl_slist_append(http_headers, "Content-Type: application/json;charset=UTF-8");
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, http_headers);
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, REMOTE_API_TIMEOUT);
    curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);

    switch (method)
    {
        case API_METHOD_PUT:
            curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, API_METHOD_PUT_CMD);
            if(data != NULL){
                curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
            }
            break;

        case API_METHOD_POST:
            curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
            if(data != NULL){
                curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
            }
            break;

        case API_METHOD_DEL:
            curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, API_METHOD_DEL_CMD);
            if(data != NULL){
                curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
            }
            break;

        case API_METHOD_GET:
            curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, API_METHOD_GET_CMD);
            break;

        case API_METHOD_PATCH:
            curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, API_METHOD_PATCH_CMD);
            if(data != NULL){
                curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
            }
            break;

        default:
            break;
    }
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_check_result);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, userp);
    result = curl_easy_perform(curl_handle);

    if (result != CURLE_OK) {
        if (result == CURLE_COULDNT_CONNECT) {
            fprintf(stderr, "Error: Couldn't connect to talker\n");
        }
        ret = -1;;
    }
    curl_easy_cleanup(curl_handle);

    return ret;
}

static int mntnct_queue_lock_fd;

int acquire_mntnct_queue_lock(void)
{
    struct flock fl;
    int ret;

    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_pid = getpid();
    mntnct_queue_lock_fd = open("/var/lock/mntnct-queue.lock",
                                O_CREAT | O_WRONLY);

    ret = fcntl(mntnct_queue_lock_fd, F_SETLK, &fl);
    if (ret == -1) {
        fprintf(stderr, "Error: another mt process is running\n");
    }

    return ret;
}

int release_mntnct_queue_lock(void)
{
    struct flock fl;
    int ret;

    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_pid = getpid();

    ret = fcntl(mntnct_queue_lock_fd, F_SETLK, &fl);

    return ret;
}
