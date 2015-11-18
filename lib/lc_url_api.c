/*
 * Copyright (c) 2012-2013 Yunshan Networks
 * All right reserved.
 *
 * Filename: lc_url_api.c
 * Author Name: Xiang Yang
 * Date: 2012-7-4
 *
 * Description: Livegate APIs
 *
 */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<pthread.h>
#include<fcntl.h>
#include<unistd.h>
#include<syslog.h>
#include<curl/curl.h>

#include "lc_livegate_api.h"
#include "lc_lcmg_api.h"
#include "vm/vm_host.h"
#include "nxjson.h"
#include "lc_vmwaf_api.h"

#define LC_CMD_EXEC_RESULT_DIR "/var/tmp/"
#define REMOTE_API_TIMEOUT 30

int API_LOG_DEFAULT_LEVEL = LOG_DEBUG;
int *API_LOG_LEVEL_P = &API_LOG_DEFAULT_LEVEL;

char *API_LOGLEVEL[8]={
    "NONE",
    "ALERT",
    "CRIT",
    "ERR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG"
};

#define API_LOG(level, format, ...)  \
    if(level <= *API_LOG_LEVEL_P){\
        syslog(level, "[tid=%lu] %s %s: " format, \
            pthread_self(), API_LOGLEVEL[level], __FUNCTION__, ##__VA_ARGS__);\
    }

int call_curl_api(char *url, char *post, result_process fp, char *data);

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

/* depreciated */
static int parse_lg_json_msg(char *msg, const nx_json **root)
{
    const nx_json *meta = NULL, *data = NULL;
    const nx_json *tmp = NULL;

    if (!msg || !root) {
        return LG_CODE_ERROR;
    }

    /* json message */

    *root = nx_json_parse(msg, NULL);
    if (!*root) {
        API_LOG(LOG_ERR, "%s: invalid msg recvived: %s\n", __FUNCTION__, msg);
        return LG_CODE_INVALID_JSON;
    }

    meta = nx_json_get(*root, LG_JSON_META_KEY);
    if (!meta) {
        API_LOG(LOG_ERR, "%s: invalid msg recvived, no meta: %s\n",
                __FUNCTION__, msg);
        return LG_CODE_INVALID_REQ;
    }
    data = nx_json_get(*root, LG_JSON_DATA_KEY);
    if (!data) {
        API_LOG(LOG_ERR, "%s: invalid msg recvived, no data: %s\n",
                __FUNCTION__, msg);
        return LG_CODE_INVALID_REQ;
    }

    /* meta */

    tmp = nx_json_get(meta, LG_JSON_VERSION_KEY);
    if (!tmp || !tmp->text_value || strcmp(tmp->text_value, LG_API_VERSION)) {
        API_LOG(LOG_ERR, "%s: invalid msg recvived, version=%s.\n",
                __FUNCTION__, (tmp && tmp->text_value) ? tmp->text_value : "");
        return LG_CODE_INVALID_VERSION;
    }

    tmp = nx_json_get(meta, LG_JSON_CODE_KEY);
    if (!tmp || !tmp->text_value) {
        API_LOG(LOG_ERR, "%s: invalid msg recvived, code=%s.\n",
                __FUNCTION__, (tmp && tmp->text_value) ? tmp->text_value : "");
        return LG_CODE_INVALID_CODE;
    }

    return LG_CODE_SUCCESS;
}

/* depreciated */
size_t livegate_check_result(void *buffer, size_t size, size_t nmemb, void *userp)
{
    int segsize  = size *nmemb;
    char result[LG_API_BIG_RESULT_SIZE] = {0};
    const nx_json *req_json = NULL, *req_code = NULL, *req_msg = NULL;
    int res_code;

    memcpy((void *)result, buffer, (size_t)segsize);
    result[segsize] = 0;
    res_code = parse_lg_json_msg(result, &req_json);
    if (res_code != LG_CODE_SUCCESS) {
        return 0;
    } else {
        req_code = nx_json_get(nx_json_get(req_json, LG_JSON_META_KEY),
                LG_JSON_CODE_KEY);
        if (strcmp(req_code->text_value,
                    LG_CODE_LABEL(LG_CODE_SUCCESS))) {
            req_msg = nx_json_get(nx_json_get(req_json, LG_JSON_DATA_KEY),
                    LG_JSON_MESSAGE_KEY);
            API_LOG(LOG_ERR, "%s: cmd result failed [%s] [%s].\n",
                    __FUNCTION__, req_code->text_value, req_msg->text_value);
            return 0;
        }
    }

    return segsize;
}

int call_livegate_api(char *url, char *post)
{
    if (NULL == url || NULL == post) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        return -1;
    }
    return call_curl_api(url, post, livegate_check_result, NULL);
}

static int parse_lcpd_json_msg(char *msg, const nx_json **root)
{
    const nx_json *opt = NULL, *desc = NULL;

    if (!msg || !root) {
        return -1;
    }

    /* json message */
    *root = nx_json_parse(msg, NULL);
    if (!*root) {
        API_LOG(LOG_ERR, "%s: invalid msg recvived: %s\n", __FUNCTION__, msg);
        return -1;
    }

    opt = nx_json_get(*root, LCPD_JSON_OPT_KEY);
    if (opt->type != NX_JSON_STRING) {
        API_LOG(LOG_ERR, "%s: invalid msg recvived, no OPT_STATUS: %s\n",
                __FUNCTION__, msg);
        return -1;
    }

    desc = nx_json_get(*root, LCPD_JSON_DESC_KEY);
    if (desc->type != NX_JSON_STRING) {
        API_LOG(LOG_ERR, "%s: invalid msg recvived, no DESCRIPTION: %s\n",
                __FUNCTION__, msg);
        return -1;
    }

    return 0;
}

size_t lcpd_check_result(void *buffer, size_t size, size_t nmemb, void *userp)
{
    int segsize  = size *nmemb;
    char result[LG_API_BIG_RESULT_SIZE] = {0};
    const nx_json *root = NULL, *opt = NULL, *desc = NULL;
    int ret;

    memcpy((void *)result, buffer, (size_t)segsize);
    result[segsize] = 0;
    if (userp != NULL) {
        memcpy(userp, (void *)result, LG_API_BIG_RESULT_SIZE);
    }
    ret = parse_lcpd_json_msg(result, &root);
    if (ret != 0) {
        API_LOG(LOG_ERR, "%s: cmd result parse failed:%s.\n",
                __FUNCTION__, result);
        return 0;
    } else {
        opt = nx_json_get(root, LCPD_JSON_OPT_KEY);
        if (strcmp(opt->text_value,
                    LCPD_CODE_LABEL(SUCCESS))) {
            desc = nx_json_get(root, LCPD_JSON_DESC_KEY);
            API_LOG(LOG_ERR, "%s: cmd result failed [%s] [%s].\n",
                    __FUNCTION__, opt->text_value, desc->text_value);
            return 0;
        }
    }
    return segsize;
}

int parse_json_msg_get_mac(char *mac, void *post)
{
    const nx_json *root = NULL;
    const nx_json *opt = NULL;
    const nx_json *opt1 = NULL;
    char result[LG_API_BIG_RESULT_SIZE] = {0};
    int ret;
    memcpy((void *)result, post, LG_API_BIG_RESULT_SIZE);
    ret = parse_lcpd_json_msg(result, &root);
    if (ret != 0) {
        return 0;
    } else {
        opt = nx_json_get(root, LCPD_JSON_DATA_KEY);
        if (opt->type != NX_JSON_OBJECT) {
            API_LOG(LOG_ERR, "%s: invalid post recvived, no DATA: %s\n",
                    __FUNCTION__, result);
            return -1;
        } else {
            opt1 = nx_json_get(opt, "MAC");
            if (opt1->type == NX_JSON_STRING) {
                strcpy(mac, opt1->text_value);
                API_LOG(LOG_DEBUG, "%s: mac is: %s\n",
                        __FUNCTION__, mac);
                return 0;
            } else {
                API_LOG(LOG_ERR, "%s: invalid post recvived, no MAC: %s\n",
                        __FUNCTION__, result);
                return -1;
            }
        }
    }
}


int parse_json_msg_get_boot_time(uint64_t *btime, void *post)
{
    const nx_json *root = NULL;
    const nx_json *opt = NULL;
    const nx_json *opt1 = NULL;
    const nx_json *item_j = NULL;
    char result[LG_API_BIG_RESULT_SIZE] = {0};
    int ret;
    memcpy((void *)result, post, LG_API_BIG_RESULT_SIZE);
    ret = parse_lcpd_json_msg(result, &root);
    if (ret != 0) {
        return 0;
    } else {
        opt = nx_json_get(root, LCPD_JSON_DATA_KEY);
        if (opt->type != NX_JSON_ARRAY) {
            API_LOG(LOG_ERR, "%s: invalid post received, no DATA: %s\n",
                    __FUNCTION__, result);
            return -1;
        } else {
            item_j = nx_json_item(opt, 0);
            if (item_j->type == NX_JSON_NULL) {
                API_LOG(LOG_ERR, "%s: invalid post received, data is empty: %s\n",
                        __FUNCTION__, result);
                return -1;
            }
            opt1 = nx_json_get(item_j, "BOOT_TIME");
            if (opt1->type == NX_JSON_INTEGER) {
                *btime = (uint64_t)opt1->int_value;
                API_LOG(LOG_DEBUG, "%s: boot time is: %"PRIu64"\n",
                        __FUNCTION__, *btime);
                return 0;
            } else {
                API_LOG(LOG_ERR,
                        "%s: invalid post recvived, no boot time: %s\n",
                        __FUNCTION__, result);
                return -1;
            }
        }
    }
}

static int parse_lcsnf_json_msg(char *msg, const nx_json **root)
{
    const nx_json *opt = NULL, *desc = NULL;

    if (!msg || !root) {
        return -1;
    }

    /* json message */
    *root = nx_json_parse(msg, NULL);
    if (!*root) {
        API_LOG(LOG_ERR, "%s: invalid msg recvived: %s\n", __FUNCTION__, msg);
        return -1;
    }

    opt = nx_json_get(*root, LCSNF_JSON_OPT_KEY);
    if (opt->type != NX_JSON_STRING) {
        API_LOG(LOG_ERR, "%s: invalid msg recvived, no OPT_STATUS: %s\n",
                __FUNCTION__, msg);
        return -1;
    }

    desc = nx_json_get(*root, LCSNF_JSON_DESC_KEY);
    if (desc->type != NX_JSON_STRING) {
        API_LOG(LOG_ERR, "%s: invalid msg recvived, no DESCRIPTION: %s\n",
                __FUNCTION__, msg);
        return -1;
    }

    return 0;
}

size_t lcsnf_check_and_get_result(void *buffer, size_t size, size_t nmemb, void *userp)
{
    size_t segsize = size * nmemb, used_size, rest_size;
    char *result = (char *)userp;

    used_size = strlen(result);
    rest_size = LCMG_API_SUPER_BIG_RESULT_SIZE - sizeof(const nx_json *) - used_size;
    if (segsize >= rest_size) {
        segsize = rest_size;
    }
    memcpy(result + used_size, buffer, segsize);
    API_LOG(LOG_INFO, "%s: bufsize %d segsize %zd totalsize %zd.\n",
            __FUNCTION__, CURL_MAX_WRITE_SIZE, segsize, strlen(result));

    return segsize;
}

size_t lcmon_check_and_get_result(void *buffer, size_t size, size_t nmemb, void *userp)
{
    size_t segsize = size * nmemb, used_size, rest_size;
    char *result = (char *)userp;

    used_size = strlen(result);
    rest_size = LCMG_API_SUPER_BIG_RESULT_SIZE - sizeof(const nx_json *) - used_size;
    if (segsize >= rest_size) {
        segsize = rest_size;
    }
    memcpy(result + used_size, buffer, segsize);
    API_LOG(LOG_INFO, "%s: bufsize %d segsize %zd totalsize %zd.\n",
            __FUNCTION__, CURL_MAX_WRITE_SIZE, segsize, strlen(result));

    return segsize;
}

int lcpd_call_api(char *url, int method, char *data, char *res)
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
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, lcpd_check_result);
    if (res != NULL) {
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, res);
    } else {
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, NULL);
    }
    result = curl_easy_perform(curl_handle);

    if (result != CURLE_OK) {
        ret = -1;;
    }
    curl_easy_cleanup(curl_handle);

    return ret;
}

int lcsnf_call_api(char *url, int method, char *data)
{
    int ret = 0;
    CURL *curl_handle = NULL;
    CURLcode result;
    struct curl_slist *http_headers = NULL;
    const nx_json *root = NULL, *opt = NULL, *desc = NULL;
    char *datum = data + sizeof(const nx_json *);

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
            if (data != NULL){
                curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
            }
            break;

        case API_METHOD_POST:
            curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
            if (data != NULL){
                curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
            }
            break;

        case API_METHOD_DEL:
            curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, API_METHOD_DEL_CMD);
            if (data != NULL){
                curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
            }
            break;

        case API_METHOD_GET:
            curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, API_METHOD_GET_CMD);
            break;

        case API_METHOD_PATCH:
            curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, API_METHOD_PATCH_CMD);
            if (data != NULL){
                curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
            }
            break;

        default:
            break;
    }
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, lcsnf_check_and_get_result);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, datum);
    result = curl_easy_perform(curl_handle);

    if (result != CURLE_OK) {
        return -1;
    }
    curl_easy_cleanup(curl_handle);

    ret = parse_lcsnf_json_msg(datum, &root);
    *((const nx_json **)data) = root;
    if (ret != 0) {
        return -2;
    } else {
        opt = nx_json_get(root, LCSNF_JSON_OPT_KEY);
        if (strcmp(opt->text_value, LCSNF_CODE_LABEL(SUCCESS))) {
            desc = nx_json_get(root, LCSNF_JSON_DESC_KEY);
            API_LOG(LOG_ERR, "%s: cmd result failed [%s] [%s].\n",
                    __FUNCTION__, opt->text_value, desc->text_value);
            return -3;
        }
    }

    return ret;
}

int lcmon_call_api(char *url, int method, char *data)
{
    int ret = 0;
    CURL *curl_handle = NULL;
    CURLcode result;
    struct curl_slist *http_headers = NULL;
    const nx_json *root = NULL, *opt = NULL, *desc = NULL;
    char *datum = data + sizeof(const nx_json *);

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
            if (data != NULL){
                curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
            }
            break;

        case API_METHOD_POST:
            curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
            if (data != NULL){
                curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
            }
            break;

        case API_METHOD_DEL:
            curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, API_METHOD_DEL_CMD);
            if (data != NULL){
                curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
            }
            break;

        case API_METHOD_GET:
            curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, API_METHOD_GET_CMD);
            break;

        case API_METHOD_PATCH:
            curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, API_METHOD_PATCH_CMD);
            if (data != NULL){
                curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
            }
            break;

        default:
            break;
    }
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, lcmon_check_and_get_result);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, datum);
    result = curl_easy_perform(curl_handle);

    if (result != CURLE_OK) {
        return -1;
    }
    curl_easy_cleanup(curl_handle);

    ret = parse_lcsnf_json_msg(datum, &root);
    *((const nx_json **)data) = root;
    if (ret != 0) {
        return -2;
    } else {
        opt = nx_json_get(root, LCMON_JSON_OPT_KEY);
        if (strcmp(opt->text_value, LCMON_CODE_LABEL(SUCCESS))) {
            desc = nx_json_get(root, LCMON_JSON_DESC_KEY);
            API_LOG(LOG_ERR, "%s: cmd result failed [%s] [%s].\n",
                    __FUNCTION__, opt->text_value, desc->text_value);
            return -3;
        }
    }

    return ret;
}

int call_lcmg_api(char *url, char *result, int result_len)
{
    if (call_system_output(url, result, result_len)) {
        return -1;
    }

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

    snprintf(fname, suffix_len, LC_CMD_EXEC_RESULT_DIR "lcc-%lu", tid);
    snprintf(cmd, cmd_len, "%s > %s 2>&1", raw_cmd, fname);
    err = system(cmd);

    if ((fp = open(fname, O_RDONLY)) == -1) {
        API_LOG(LOG_ERR, "%s: cmd result open failed [%s]\n",
                __FUNCTION__, cmd);
        return 1;
    }
    rd_len = read(fp, result, result_buf_len);
    close(fp);

    if (rd_len < 0) {
        API_LOG(LOG_ERR, "%s: cmd result read failed [%s]\n",
                __FUNCTION__, cmd);
        return 1;
    } else if (rd_len >= result_buf_len) {
        API_LOG(LOG_ERR, "%s: cmd result too large, buf_size=%d [%s]\n",
                __FUNCTION__, result_buf_len, cmd);
        snprintf(result, result_buf_len, "command result too large");
        return 1;
    }
    result[rd_len] = 0;

    if (err) {
        API_LOG(LOG_ERR, "%s: cmd failed [%s]:[%s]\n", __FUNCTION__,
                cmd, result);
    } else if (rd_len >= 512) {
#if 0
        /* only for debug */
        char *t, *p = result;
        for (t = strstr(p, "\n\n"); ; p = t + 1, t = strstr(p, "\n\n")) {
            if (t) {
                *t = 0;
            }
            API_LOG(LOG_DEBUG, "%s: cmd exec, result_len=%d [%s]:[%s]\n", __FUNCTION__,
                    rd_len, cmd, p);
            if (!t) {
                break;
            }
        }
#endif
        API_LOG(LOG_DEBUG, "%s: cmd exec, result_len=%d [%s]\n", __FUNCTION__,
                rd_len, cmd);
    } else {
        API_LOG(LOG_DEBUG, "%s: cmd exec [%s][%s]\n", __FUNCTION__,
                cmd, result);
    }

    sprintf(cmd, "rm -f %s", fname);
    system(cmd);

    return err;
}

size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    int segsize  = size *nmemb;
    int rd_len   = 0;
    char result[LCMG_API_BIG_RESULT_SIZE] = {0};

    memcpy((void *)result, buffer, (size_t)segsize);
    rd_len = segsize;
    result[rd_len] = 0;

    if (!strstr(result, LCMG_API_SUCCESS_RESULT)) {
        API_LOG(LOG_DEBUG, "%s: api exec, result_len=%d [%s]\n", __FUNCTION__,
               rd_len, result);
        return 0;
    }

    if (NULL != userp) {
        memcpy(userp, buffer, (size_t)segsize);
    }
    return segsize;
}

size_t ofs_write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    int segsize  = size *nmemb;
    int rd_len   = 0;
    char result[LCMG_API_BIG_RESULT_SIZE] = {0};
    memcpy((void *)result, buffer, (size_t)segsize);
    rd_len = segsize;
    result[rd_len] = 0;
    if (!strstr(result, "SUCCESS")) {
        API_LOG(LOG_DEBUG, "%s: api exec, result_len=%d [%s]\n", __FUNCTION__,
               rd_len, result);
        return 0;
    }
    if (NULL != userp) {
        memcpy(userp, buffer, (size_t)segsize);
    }
    return segsize;
}

int call_curl_api(char *url, char *post, result_process fp, char *data)
{
    int ret = 0;
    CURL *curl_handle = NULL;
    CURLcode result;

    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, REMOTE_API_TIMEOUT);
    curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, post);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, fp);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, data);
    result = curl_easy_perform(curl_handle);

    if (result != CURLE_OK) {
        ret = -1;;
    }
    curl_easy_cleanup(curl_handle);

    return ret;
}

int call_vmwaf_curl_api(char *url, int method, char *data, char *cookie, result_process fp, char *echo)
{
    int ret = 0;
    CURL *curl_handle = NULL;
    CURLcode result;

    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, REMOTE_API_TIMEOUT);
    curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl_handle, CURLOPT_COOKIE, cookie);
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

        default:
            break;
    }
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, fp);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, echo);
    result = curl_easy_perform(curl_handle);

    if (result != CURLE_OK) {
        ret = -1;;
    }
    curl_easy_cleanup(curl_handle);

    return ret;
}

int call_curl_init_ofs_port_confg(char *ctrl_ip, char *ofs_post)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE]       = {0};
    char post[LCMG_API_POST_SIZE]     = {0};

    if (0 == ctrl_ip[0] || 0 == ofs_post) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_OFS_URL, ctrl_ip);

    memset(post, 0, LCMG_API_POST_SIZE);
    snprintf(post, LCMG_API_POST_SIZE, ofs_post);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s][%s]\n", __FUNCTION__, url, post);
    if (call_curl_api(url, post, ofs_write_data, NULL)) {
        API_LOG(LOG_ERR, "call_curl_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}

int call_curl_delete_all_flow_table(char *ctrl_ipt)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE]       = {0};

    if (0 == ctrl_ipt[0]) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_OFS_FLOW_URL, ctrl_ipt);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s]\n", __FUNCTION__, url);

    if (lcpd_call_api(url, API_METHOD_DEL, NULL, NULL)) {
        API_LOG(LOG_ERR, "lcpd_call_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}
int call_curl_ofs_port_confg(char *ctrl_ip, char *ofs_post)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE]       = {0};
    char post[LCMG_API_POST_SIZE]     = {0};

    if (0 == ctrl_ip[0] || 0 == ofs_post) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_OFS_PORT_URL, ctrl_ip);

    memset(post, 0, LCMG_API_POST_SIZE);
    snprintf(post, LCMG_API_POST_SIZE, ofs_post);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s][%s]\n", __FUNCTION__, url, post);
    if (call_curl_api(url, post, ofs_write_data, NULL)) {
        API_LOG(LOG_ERR, "call_curl_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}

int call_curl_add_vmwaf_nat(char *ctrl_ip, char *nat_post)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE]       = {0};
    char post[LCMG_API_POST_SIZE]     = {0};

    if (0 == ctrl_ip[0] || 0 == nat_post) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_PUBLIC_URL LCMG_API_ADD_VMWAF_NAT, ctrl_ip);

    memset(post, 0, LCMG_API_POST_SIZE);
    snprintf(post, LCMG_API_POST_SIZE, nat_post);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s][%s]\n", __FUNCTION__, url, post);
    if (call_curl_api(url, post, write_data, NULL)) {
        API_LOG(LOG_ERR, "call_curl_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}

int call_curl_del_vmwaf_nat(char *ctrl_ip, char *nat_post)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE]       = {0};
    char post[LCMG_API_POST_SIZE]     = {0};

    if (0 == ctrl_ip[0] || 0 == nat_post) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_PUBLIC_URL LCMG_API_DEL_VMWAF_NAT, ctrl_ip);

    memset(post, 0, LCMG_API_POST_SIZE);
    snprintf(post, LCMG_API_POST_SIZE, nat_post);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s][%s]\n", __FUNCTION__, url, post);
    if (call_curl_api(url, post, write_data, NULL)) {
        API_LOG(LOG_ERR, "call_curl_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}

int call_curl_add_pubip(char *ctrl_ip, char *ip_post[], int ip_num)
{
    int ret = 0;
    int i   = 0;
    char url[LCMG_API_URL_SIZE]       = {0};
    char post[LCMG_API_POST_SIZE]     = {0};
    char add_pubip[LCMG_API_POST_SIZE]= {0};

    if (0 == ctrl_ip[0] || 0 == ip_post[0][0] || 0 == ip_num) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_PUBLIC_URL LCMG_API_ADD_PUBIP, ctrl_ip);

    memset(add_pubip, 0, LCMG_API_POST_SIZE);
    snprintf(add_pubip, LCMG_API_POST_SIZE, "%s", ip_post[0]);
    for (i = 1; i < ip_num; i++) {
        strcat(add_pubip, ",");
        strcat(add_pubip, ip_post[i]);
    }

    memset(post, 0, LCMG_API_POST_SIZE);
    snprintf(post, LCMG_API_POST_SIZE, LCMG_API_PUBLIC_POST, add_pubip);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s][%s]\n", __FUNCTION__, url, post);
    if (call_curl_api(url, post, write_data, NULL)) {
        API_LOG(LOG_ERR, "call_curl_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}

int call_curl_set_gw(char *ctrl_ip, char *gw)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE]    = {0};
    char post[LCMG_API_POST_SIZE]  = {0};
    char set_gw[LCMG_API_POST_SIZE]= {0};

    if (0 == ctrl_ip[0] || 0 == gw[0]) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_PUBLIC_URL LCMG_API_SET_GW, ctrl_ip);

    memset(set_gw, 0, LCMG_API_POST_SIZE);
    snprintf(set_gw, LCMG_API_POST_SIZE, "%s", gw);

    memset(post, 0, LCMG_API_POST_SIZE);
    snprintf(post, LCMG_API_POST_SIZE, LCMG_API_PUBLIC_POST, set_gw);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s][%s]\n", __FUNCTION__, url, post);
    if (call_curl_api(url, post, write_data, NULL)) {
        API_LOG(LOG_ERR, "call_curl_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}

int call_curl_get_hastate(char *ctrl_ip, int *hastate)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE]       = {0};
    char post[LCMG_API_POST_SIZE]     = {0};
    char result[LCMG_API_RESULT_SIZE] = {0};

    if (0 == ctrl_ip[0] || NULL == hastate) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_PUBLIC_URL LCMG_API_GET_GWMODE, ctrl_ip);

    memset(post, 0, LCMG_API_POST_SIZE);
    memset(result, 0, LCMG_API_RESULT_SIZE);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s][%s]\n", __FUNCTION__, url, post);
    if (call_curl_api(url, post, write_data, result)) {
        API_LOG(LOG_ERR, "call_curl_api failed!\n");
        ret = -1;
        goto error;
    }

    if (strstr(result, LCMG_API_RESULT_HASTATE_MASTER)) {
        *hastate = LC_VM_HASTATE_MASTER;
    } else if (strstr(result, LCMG_API_RESULT_HASTATE_BACKUP)) {
        *hastate = LC_VM_HASTATE_BACKUP;
    } else if (strstr(result, LCMG_API_RESULT_HASTATE_FAULT)) {
        *hastate = LC_VM_HASTATE_FAULT;
    } else {
        API_LOG(LOG_ERR, "%s: hastate err, result[%s]\n", __FUNCTION__, result);
        ret = -1;
        goto error;
    }
    return ret;

error:
    *hastate = LC_VM_HASTATE_UNKOWN;
    return ret;
}

int call_curl_set_master_manual(char *ctrl_ip)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE]   = {0};
    char post[LCMG_API_POST_SIZE] = {0};

    if (0 == ctrl_ip[0]) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_PUBLIC_URL LCMG_API_SET_MASTER_MANUAL, ctrl_ip);

    memset(post, 0, LCMG_API_POST_SIZE);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s][%s]\n", __FUNCTION__, url, post);
    if (call_curl_api(url, post, write_data, NULL)) {
        API_LOG(LOG_ERR, "call_curl_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}

int call_curl_set_backup_manual(char *ctrl_ip)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE]   = {0};
    char post[LCMG_API_POST_SIZE] = {0};

    if (0 == ctrl_ip[0]) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_PUBLIC_URL LCMG_API_SET_BACKUP_MANUAL, ctrl_ip);

    memset(post, 0, LCMG_API_POST_SIZE);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s][%s]\n", __FUNCTION__, url, post);
    if (call_curl_api(url, post, write_data, NULL)) {
        API_LOG(LOG_ERR, "call_curl_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}

int call_curl_set_auto(char *ctrl_ip)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE]   = {0};
    char post[LCMG_API_POST_SIZE] = {0};

    if (0 == ctrl_ip[0]) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_PUBLIC_URL LCMG_API_SET_AUTO, ctrl_ip);

    memset(post, 0, LCMG_API_POST_SIZE);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s][%s]\n", __FUNCTION__, url, post);
    if (call_curl_api(url, post, write_data, NULL)) {
        API_LOG(LOG_ERR, "call_curl_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}

int call_curl_patch_all_nsp_vgws(char *lcuuid, char *nsp_post)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE] = {0};
    char post[LCMG_API_POST_SIZE] = {0};

    if (NULL == lcuuid) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_NSP_VGW_URL,
            lcuuid);

    memset(post, 0, LCMG_API_POST_SIZE);
    snprintf(post, LCMG_API_POST_SIZE, LCMG_API_NSP_VGW_SERVER_POST,
            nsp_post);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s][%s]\n", __FUNCTION__,
            url, post);

    if (lcpd_call_api(url, API_METHOD_PATCH, post, NULL)) {
        API_LOG(LOG_ERR, "lcpd_call_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}

int call_curl_patch_all_nsp_valves(char *lcuuid, char *nsp_post)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE] = {0};
    char post[LCMG_API_POST_SIZE] = {0};

    if (NULL == lcuuid) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_NSP_VALVE_URL,
            lcuuid);

    memset(post, 0, LCMG_API_POST_SIZE);
    snprintf(post, LCMG_API_POST_SIZE, LCMG_API_NSP_VGW_SERVER_POST,
            nsp_post);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s][%s]\n", __FUNCTION__,
            url, post);

    if (lcpd_call_api(url, API_METHOD_PATCH, post, NULL)) {
        API_LOG(LOG_ERR, "lcpd_call_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}

int call_curl_get_nsp_stat(char *host_ip, char *master_ctrl_ip, char *data)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE] = {0};

    if (NULL == host_ip) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_NSP_STAT_URL,
            host_ip, master_ctrl_ip);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s]\n", __FUNCTION__,
            url);

    if (lcsnf_call_api(url, API_METHOD_GET, data)) {
        API_LOG(LOG_ERR, "lcsnf_call_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}

int call_curl_get_nsp_stat_rt(char *host_ip, char *data)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE] = {0};

    if (NULL == host_ip) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_NSP_STAT_RT_URL,
            host_ip);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s]\n", __FUNCTION__,
            url);

    if (lcsnf_call_api(url, API_METHOD_GET, data)) {
        API_LOG(LOG_ERR, "lcsnf_call_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}

int call_curl_get_lb_stat(char *host_ip, char *data)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE] = {0};

    if (NULL == host_ip) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_LB_STAT_URL, host_ip);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s]\n", __FUNCTION__,
            url);

    if (lcsnf_call_api(url, API_METHOD_GET, data)) {
        API_LOG(LOG_ERR, "lcsnf_call_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}

int call_curl_get_switch_load(char *switch_mip, char *data)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE] = {0};

    if (NULL == switch_mip) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_SWITCH_LOAD_URL,
            switch_mip);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s]\n", __FUNCTION__,
            url);

    if (lcsnf_call_api(url, API_METHOD_GET, data)) {
        API_LOG(LOG_ERR, "lcsnf_call_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}

int call_curl_get_bk_vm_health(char *host_ip, char *ctrl_ip, char *data)
{
    int ret = 0;
    char url[LCMG_API_URL_SIZE] = {0};

    if (NULL == host_ip || NULL == ctrl_ip) {
        API_LOG(LOG_ERR, "%s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(url, 0, LCMG_API_URL_SIZE);
    snprintf(url, LCMG_API_URL_SIZE, LCMG_API_LB_BK_VM_HEALTH_STATE_URL,
             host_ip, ctrl_ip);

    API_LOG(LOG_DEBUG, "%s: curl exec [%s]\n", __FUNCTION__,
            url);

    if (lcmon_call_api(url, API_METHOD_GET, data)) {
        API_LOG(LOG_ERR, "lcmon_call_api failed!\n");
        ret = -1;
        goto error;
    }

error:
    return ret;
}

static int parse_json_msg_alloc_vlan_result(uint32_t *p_vlantag, void *post)
{
    int ret = 0;
    const nx_json *root = NULL;
    const nx_json *data = NULL;
    const nx_json *vlan = NULL;
    char result[LG_API_BIG_RESULT_SIZE] = {0};

    memcpy((void *)result, post, LG_API_BIG_RESULT_SIZE);
    ret = parse_lcpd_json_msg(result, &root);
    if (ret != 0) {
        return -1;
    } else {
        data = nx_json_get(root, LCPD_JSON_DATA_KEY);
        if (data->type != NX_JSON_OBJECT) {
            API_LOG(LOG_ERR, "%s: invalid post received, no DATA: %s\n",
                    __FUNCTION__, result);
            return -1;
        } else {
            vlan = nx_json_get(data, TALKER_API_VL2VLAN_VLAN_TAG);
            if (vlan->type == NX_JSON_INTEGER) {
                *p_vlantag = (uint32_t)vlan->int_value;
                API_LOG(LOG_DEBUG, "%s: vlan tag is: %u\n",
                        __FUNCTION__, *p_vlantag);
                return 0;
            } else {
                API_LOG(LOG_ERR,
                          "%s: invalid post recvived, no vlan tag: %s\n",
                        __FUNCTION__, result);
                return -1;
            }
        }
    }
}

int call_url_post_vl2_vlan(uint32_t *p_vlantag,
        uint32_t vl2id, uint32_t rackid, uint32_t vlan_request, uint32_t force)
{
    int res = 0;
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    char url[LG_API_URL_SIZE] = {0};
    char post[LG_API_BIG_RESULT_SIZE] = {0};
    char result[LG_API_BIG_RESULT_SIZE] = {0};

    if (p_vlantag) {
        *p_vlantag = 0;
    } else {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;
    tmp = create_json(NX_JSON_INTEGER, TALKER_API_VL2VLAN_VL2ID, root);
    tmp->int_value = vl2id;

    tmp = create_json(NX_JSON_INTEGER, TALKER_API_VL2VLAN_RACKID, root);
    tmp->int_value = rackid;

    tmp = create_json(NX_JSON_INTEGER, TALKER_API_VL2VLAN_VLAN_REQ, root);
    tmp->int_value = vlan_request;

    tmp = create_json(NX_JSON_BOOL, TALKER_API_VL2VLAN_FORCE, root);
    tmp->int_value = force;

    dump_json_msg(root, post, LG_API_BIG_RESULT_SIZE);
    nx_json_free(root);

    snprintf(url, LG_API_URL_SIZE, TALKER_API_PREFIX TALKER_API_VL2VLAN,
             LCPD_TALKER_LISTEN_IP, LCPD_TALKER_LISTEN_PORT,
             TALKER_API_VERSION);

    API_LOG(LOG_INFO, "Send url post:%s\n", post);
    memset(result, 0x00, sizeof(result));
    res = lcpd_call_api(url, API_METHOD_POST, post, result);
    if (res != 0) {  // curl failed or api failed
        API_LOG(LOG_ERR, "Recv result:%s\n", result);
        return -1;
    }
    if (parse_json_msg_alloc_vlan_result(p_vlantag, result) != 0){
        return -1;
    }
    return 0;
}