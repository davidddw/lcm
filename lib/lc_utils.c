#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <syslog.h>
#include "lc_utils.h"

void dump_buffer(void *data, int count)
{
    int i;
    char *buf = (char *)data;

    for (i = 0; i < count; i++) {
        if ((i != 0) && (i % 80) == 0) {
            printf("\n");
        } else {
            printf("%02x ", *(buf + i));
        }
    }
    printf("\n");
    return;
}

void lc_get_mac_str_for_vm(char *mac, int vnet_id, int vl2_id, int vm_id, int index)
{

    sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", 0, 0xF6,
        vnet_id%255, vl2_id%255, vm_id%255, (100+index)%255);
    return;
}

void lc_get_mac_str_for_gw(char *mac, int vnet_id, int vl2_id, int vm_id, int index)
{
    sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", 0, 0xF6,
        vnet_id%255, vl2_id%255, vm_id%255, (100+index)%255);
    return;
}

void get_vmname_by_id(char *vmname, int vnet_id, int vl2_id, int vm_id)
{
    sprintf(vmname, "vnet%06d_vl2%06d_vm%06d", vnet_id, vl2_id, vm_id);
}

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
    vsnprintf(cmd, 512, get_str, ap);
    va_end(ap);
    if ((fp = popen(cmd, "r")) == 0) {
        return -1;
    }
    fgets(value, size - 2, fp);
    chomp(value);
    pclose(fp);

    return 0;
}

void lc_print_xapi_error(char *action, char *obj, char *info, xen_session *session, lc_db_log_cb lc_db_log)
{
    int des_len = 100;
    int i = 0, len = 0;
    char *err_info = NULL;
    char str[des_len];

    len = des_len * session->error_description_count + 50;

    err_info = (char *)malloc(len);
    if (!err_info) {
        return;
    }

    memset(err_info, 0, len);
    sprintf(err_info, "%s %s: ", obj, info);
    for (i = 0; i < session->error_description_count; i++) {
         memset(str, 0, des_len);
         snprintf(str, des_len, "%s,", session->error_description[i]);
         strcat(err_info, str);
    }
    if (lc_db_log) {
        lc_db_log(action, err_info, LOG_ERR);
    }

    free(err_info);
}


int lc_call_file_stream(char *value, char* command,char *action, char *obj, char *info, lc_db_log_cb lc_db_log)
{
    int len= 0;
    char type_str[512];
    char *err_cmd = NULL;

    if (strlen(value)) {
        return 0;
    }
    len = strlen(command) + 6;
    err_cmd = (char *)malloc(len);
    if (!err_cmd) {
         return -1;
    }
    memset(err_cmd, 0, len);
    memset(type_str, 0, 512);
    sprintf(type_str, "%s %s: ", obj, info);
    len = strlen(type_str);
    strcpy(err_cmd, command);
    strcat(err_cmd, " 2>&1");
    get_param(type_str + len, 512 - len, err_cmd);
    if (lc_db_log && strlen(type_str+len)) {
        lc_db_log(action, type_str, LOG_ERR);
    }
    free(err_cmd);
    return -1;
}

static int get_err_description(char *value, char *filename)
{
    int fp;
    if ((fp = open(filename, O_RDONLY)) == -1) {
        return -1;
    }
    read(fp, value, 512);
    close(fp);

    return 0;
}

int lc_call_system(char* command,char *action, char *obj, char *info, lc_db_log_cb lc_db_log)
{
    int result = -1;
    result = system(command);

    if (WIFEXITED(result)) {
        if (0 != (result = WEXITSTATUS(result))) {
            char err_cmd[1049];
            char type_str[512];
            char tmp1[5], tmp2[20], tmp3[30];
            int len = 0;
            pthread_t tid = pthread_self();

            if (!lc_db_log) {
                return result;
            }
            memset(type_str, 0, 512);
            memset(err_cmd, 0, 1049);
            memset(tmp1, 0, 5);
            memset(tmp2, 0, 20);
            memset(tmp3, 0, 30);

            sprintf(tmp1, " 2>");
            sprintf(tmp2, "/var/tmp/%lu", tid);
            strcpy(err_cmd, command);
            strcat(err_cmd, tmp1);
            strcat(err_cmd, tmp2);
            system(err_cmd);

            sprintf(type_str, "%s %s: ", obj, info);
            len = strlen(type_str);
            get_err_description(type_str + len, tmp2);

            sprintf(tmp3, "rm -f %s", tmp2);
            system(tmp3);
            if (lc_db_log && strlen(type_str+len)) {
                lc_db_log(action, type_str, LOG_WARNING);
            }
        }
    }

    return result;
}

int wrap_cmd_param_inplace(char *param, int param_buf_len)
{
    int n_quota, lrp, i, j;

    if (!param) {
        return 1;
    }
    if (param_buf_len < 3) {
        param[0] = 0;
        return 1;
    }

    for (n_quota = lrp = 0; param[lrp]; ++lrp) {
        if (param[lrp] == '"' || param[lrp] == '\\') {
            ++n_quota;
        }
    }
    if (param_buf_len < lrp + n_quota + 3) {
        param[0] = '"';
        param[1] = '"';
        param[2] = 0;
        return 1;
    }

    for (i = lrp - 1, j = lrp + n_quota; i >= 0; ) {
        if (param[i] == '"' || param[i] == '\\') {
            param[j--] = param[i--];
            param[j--] = '\\';
        } else {
            param[j--] = param[i--];
        }
    }
    param[0] = '"';
    param[lrp + n_quota + 1] = '"';
    param[lrp + n_quota + 2] = 0;
    return 0;
}

int wrap_cmd_param(char *raw_param, char *param, int param_buf_len)
{
    int n_quota, i;
    int raw_param_len = raw_param ? strlen(raw_param) + 1 : 0;

    if (!raw_param || !param || param_buf_len < raw_param_len + 2 ||
            (raw_param <= param + param_buf_len &&
             param <= raw_param + raw_param_len) ) {
        if (param) {
            if (param_buf_len >= 3) {
                *param++ = '"';
                *param++ = '"';
            }
            *param = 0;
        }
        return 1;
    }

    for (n_quota = i = 0; raw_param[i]; ++i) {
        if (raw_param[i] == '"' || raw_param[i] == '\\') {
            ++n_quota;
        }
    }
    if (param_buf_len < raw_param_len + n_quota + 2) {
        *param++ = '"';
        *param++ = '"';
        *param = 0;
        return 1;
    }

    *param++ = '"';
    while (*raw_param) {
        if (*raw_param == '"' || *raw_param == '\\') {
            *param++ = '\\';
        }
        *param++ = *raw_param++;
    }
    *param++ = '"';
    *param = 0;

    return 0;
}
