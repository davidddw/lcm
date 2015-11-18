#include <signal.h>
#include <sys/time.h>
#include <stdlib.h>
#include <time.h>

#include "lcs_usage_api.h"
#include "lc_mongo_db.h"
#include "lc_db.h"
#include "lcs_utils.h"
#include "lcs_socket.h"
#include "lcs_usage_db.h"
#include "lcs_usage_xen.h"
#include "lcs_usage_vmware.h"
#include "lcs_usage.h"
#include "lc_db_sys.h"
#include "cloudmessage.pb-c.h"
#include "agexec_inner.h"
#include "lc_bus.h"
#include "lc_header.h"
#include "lcs_utils.h"
#include "nxjson.h"

#define LCSNF_JSON_OPT_KEY          "OPT_STATUS"
#define LCSNF_JSON_ERR_MSG_KEY      "ERR_MSG"
#define LCSNF_JSON_DATA_MSG_KEY     "DATA"
#define LCSNF_JSON_TYPE_KEY         "type"
#define LCSNF_JSON_ID_KEY           "id"

api_msg_buf_t g_msg[MAX_ID_COUNT];

static int is_json_complete(char *msg, int offset, int *nquote, int *nbrace)
{
    int i;

    if (!nquote || !nbrace) {
        return 0;
    }

    if (!msg || !msg[offset]) {
        return *nbrace == 0 ? 1 : 0;
    }

    for (i = offset; msg[i]; ++i) {
        /* FIXME consider " escaping and message
         * slicing */
        if (msg[i] == '"') {
            *nquote = 1 - *nquote;
        }

        if (*nquote == 0) {
            if (msg[i] == '{') {
                ++(*nbrace);
            } else if (msg[i] == '}') {
                --(*nbrace);
            }
        }

        if (*nbrace == 0) {
            return 1;
        }
    }

    return 0;
}

static int dump_json_msg(const nx_json *root, char *msg_buf, int buf_len)
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

int parse_lcsnf_json_msg(char *msg, const nx_json **root)
{
    const nx_json *type = NULL, *id = NULL;

    if (!msg || !root) {
        return -1;
    }

    /* json message */
    *root = nx_json_parse(msg, NULL);
    if (!*root) {
        SNF_SYSLOG(LOG_ERR, "%s: invalid msg recvived: %s\n", __FUNCTION__, msg);
        return -1;
    }

    type = nx_json_get(*root, LCSNF_JSON_TYPE_KEY);
    if (!type || !type->text_value) {
        SNF_SYSLOG(LOG_ERR, "%s: invalid msg recvived, no type: %s\n",
                __FUNCTION__, msg);
        return -1;
    }

    id = nx_json_get(*root, LCSNF_JSON_ID_KEY);
    if (!id || !id->text_value) {
        SNF_SYSLOG(LOG_ERR, "%s: invalid msg recvived, no label: %s\n",
                __FUNCTION__, msg);
        return -1;
    }

    return 0;
}

static int pack_err_response_json_msg(nx_json *root, char *msg)
{
    nx_json *opt = NULL;
    nx_json *err_msg = NULL;

    if (!root || !msg) {
        return -1;
    }

    opt = create_json(NX_JSON_STRING, LCSNF_JSON_OPT_KEY, root);
    opt->text_value = "0";

    err_msg = create_json(NX_JSON_STRING, LCSNF_JSON_ERR_MSG_KEY, root);
    err_msg->text_value = msg;

    return 0;
}

static int pack_vm_response_json_msg(nx_json *root, vm_info_t *vm_info, vm_result_t *vm, char *buf)
{
    nx_json *opt = NULL;
    nx_json *data = NULL;
    nx_json *tmp = NULL, *vif = NULL;
    int offset = 0, n, i, vifnum;
    uint32_t mem_total =0;
    uint32_t mem_free =0;
    uint64_t system_disk_usage;
    uint64_t user_disk_usage;

    if (!root || !vm_info || !vm) {
        return -1;
    }

    opt = create_json(NX_JSON_STRING, LCSNF_JSON_OPT_KEY, root);
    opt->text_value = "1";

    data = create_json(NX_JSON_OBJECT, LCSNF_JSON_DATA_MSG_KEY, root);

    tmp = create_json(NX_JSON_STRING, "vmid", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%d", vm->vm_id);
    offset += (n + 1);

    tmp = create_json(NX_JSON_STRING, "vm_name", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%s", vm_info->vm_name);
    offset += (n + 1);

    tmp = create_json(NX_JSON_STRING, "state", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%s", vm->state);
    offset += (n + 1);

    tmp = create_json(NX_JSON_STRING, "timestamp", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%"PRId64"", (uint64_t)time(NULL));
    offset += (n + 1);

    get_vm_actual_memory(&mem_total, vm->memory_total);
    tmp = create_json(NX_JSON_STRING, "mem_total", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%d", mem_total);
    offset += (n + 1);

    get_vm_actual_memory(&mem_free, vm->memory_free);
    tmp = create_json(NX_JSON_STRING, "mem_usage", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%d", mem_total - mem_free);
    offset += (n + 1);

    tmp = create_json(NX_JSON_STRING, "cpu_number", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%s", vm->cpu_number);
    offset += (n + 1);

    tmp = create_json(NX_JSON_STRING, "cpu_usage", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%s", vm->cpu_usage);
    offset += (n + 1);

    tmp = create_json(NX_JSON_STRING, "system_disk_total", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%"PRId64"", vm_info->vdi_sys_size);
    offset += (n + 1);

    get_vm_actual_vdi_size(&system_disk_usage, vm->used_sys_disk);
    tmp = create_json(NX_JSON_STRING, "system_disk_usage", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%"PRId64"", system_disk_usage);
    offset += (n + 1);

    tmp = create_json(NX_JSON_STRING, "user_disk_total", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%"PRId64"", vm_info->vdi_user_size);
    offset += (n + 1);

    get_vm_actual_vdi_size(&user_disk_usage, vm->used_user_disk);
    tmp = create_json(NX_JSON_STRING, "user_disk_usage", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%"PRId64"", user_disk_usage);
    offset += (n + 1);

    vif = create_json(NX_JSON_OBJECT, "vif_info", data);

    if (vm->vm_type == LC_VM_TYPE_VM) {
        vifnum = 1;
    } else {
        vifnum = LC_VM_MAX_VIF;
    }

    for (i=0; i<vifnum; i++) {
        n = sprintf(buf + offset, "vif%d", i + 1);
        data = create_json(NX_JSON_OBJECT, buf + offset, vif);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "vif_id", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%d", vm->vifs[i].vif_id);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "mac", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%s", vm->vifs[i].mac);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "rx_bytes", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64"", vm->vifs[i].rx_bytes);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "rx_dropped", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64"", vm->vifs[i].rx_dropped);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "rx_errors", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64"", vm->vifs[i].rx_errors);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "rx_packets", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64"", vm->vifs[i].rx_packets);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "rx_bps", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64"", vm->vifs[i].rx_bps);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "rx_pps", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64"", vm->vifs[i].rx_pps);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "tx_bytes", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64"", vm->vifs[i].tx_bytes);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "tx_dropped", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64"", vm->vifs[i].tx_dropped);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "tx_errors", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64"", vm->vifs[i].tx_errors);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "tx_packets", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64"", vm->vifs[i].tx_packets);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "tx_bps", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64"", vm->vifs[i].tx_bps);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "tx_pps", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64"", vm->vifs[i].tx_pps);
        offset += (n + 1);
    }

    return 0;
}

static int pack_vgw_response_json_msg(nx_json *root, vm_info_t *vm_info, vm_result_t *vm, char *buf)
{
    nx_json *opt = NULL;
    nx_json *data = NULL;
    nx_json *tmp = NULL, *vif = NULL;
    int offset = 0, n, i, vifnum;
    uint32_t mem_total =0;
    uint32_t mem_free =0;
    uint64_t system_disk_usage;
    uint64_t user_disk_usage;

    if (!root || !vm_info || !vm) {
        return -1;
    }

    opt = create_json(NX_JSON_STRING, LCSNF_JSON_OPT_KEY, root);
    opt->text_value = "1";

    data = create_json(NX_JSON_OBJECT, LCSNF_JSON_DATA_MSG_KEY, root);

    tmp = create_json(NX_JSON_STRING, "vmid", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%d", vm->vm_id);
    offset += (n + 1);

    tmp = create_json(NX_JSON_STRING, "vm_name", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%s", vm_info->vm_name);
    offset += (n + 1);

    tmp = create_json(NX_JSON_STRING, "state", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%s", vm->state);
    offset += (n + 1);

    tmp = create_json(NX_JSON_STRING, "timestamp", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%"PRId64"", (uint64_t)time(NULL));
    offset += (n + 1);

    get_vm_actual_memory(&mem_total, vm->memory_total);
    tmp = create_json(NX_JSON_STRING, "mem_total", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%d", mem_total);
    offset += (n + 1);

    get_vm_actual_memory(&mem_free, vm->memory_free);
    tmp = create_json(NX_JSON_STRING, "mem_usage", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%d", mem_total - mem_free);
    offset += (n + 1);

    tmp = create_json(NX_JSON_STRING, "cpu_number", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%s", vm->cpu_number);
    offset += (n + 1);

    tmp = create_json(NX_JSON_STRING, "cpu_usage", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%s", vm->cpu_usage);
    offset += (n + 1);

    tmp = create_json(NX_JSON_STRING, "system_disk_total", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%"PRId64"", vm_info->vdi_sys_size);
    offset += (n + 1);

    get_vm_actual_vdi_size(&system_disk_usage, vm->used_sys_disk);
    tmp = create_json(NX_JSON_STRING, "system_disk_usage", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%"PRId64"", system_disk_usage);
    offset += (n + 1);

    tmp = create_json(NX_JSON_STRING, "user_disk_total", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%"PRId64"", vm_info->vdi_user_size);
    offset += (n + 1);

    get_vm_actual_vdi_size(&user_disk_usage, vm->used_user_disk);
    tmp = create_json(NX_JSON_STRING, "user_disk_usage", data);
    tmp->text_value = buf + offset;
    n = sprintf(buf + offset, "%"PRId64"", user_disk_usage);
    offset += (n + 1);

    vif = create_json(NX_JSON_OBJECT, "vif_info", data);

    vifnum = LC_VM_MAX_VIF;

    for (i=0; i<vifnum; i++) {
        n = sprintf(buf + offset, "vif%d", i + 1);
        data = create_json(NX_JSON_OBJECT, buf + offset, vif);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "vif_id", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%d", vm->vifs[i].vif_id);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "mac", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%s", vm->vifs[i].mac);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "rx_bytes", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64, vm->vifs[i].rx_bytes);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "rx_dropped", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64, vm->vifs[i].rx_dropped);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "rx_errors", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64, vm->vifs[i].rx_errors);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "rx_packets", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64, vm->vifs[i].rx_packets);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "rx_bps", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64, vm->vifs[i].rx_bps);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "rx_pps", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64, vm->vifs[i].rx_pps);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "tx_bytes", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64, vm->vifs[i].tx_bytes);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "tx_dropped", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64, vm->vifs[i].tx_dropped);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "tx_errors", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64, vm->vifs[i].tx_errors);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "tx_packets", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64, vm->vifs[i].tx_packets);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "tx_bps", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64, vm->vifs[i].tx_bps);
        offset += (n + 1);

        tmp = create_json(NX_JSON_STRING, "tx_pps", data);
        tmp->text_value = buf + offset;
        n = sprintf(buf + offset, "%"PRId64, vm->vifs[i].tx_pps);
        offset += (n + 1);
    }

    return 0;
}

static int lcs_vgw_vm_req_parse(api_arg_t *arg, nx_json *root, char *buf)
{
    agent_vm_vif_id_t vm_set;
    vm_result_t vm;
    int ret, i, ifindex = 0;
    int type = g_msg[arg->idx].type;
    int id = g_msg[arg->idx].id;

    memset(&vm_set, 0, sizeof(vm_set));
    memset(&vm, 0, sizeof(vm));

    strcpy(vm_set.vm_label, arg->vm_info.vm_label);
    vm_set.vm_type = type;
    vm_set.vm_id = id;
    vm_set.vdc_id = arg->vm_info.vnet_id;
    vm_set.host_id = arg->hostinfo.id;
    if (type == LC_VM_TYPE_VM) {
        vm_set.vm_res_type = VM_RES_ALL;
    } else if (type == LC_VM_TYPE_GW) {
        vm_set.vm_res_type |= VM_RES_STATE | VM_RES_CPU_NUM | VM_RES_CPU_USAGE;
    } else {
        vm_set.vm_res_type = VM_RES_STATE;
    }
    for (i=0; i<arg->nvif; i++) {
        vm_set.vif_num++;
        ifindex = arg->vintf_infos[i].if_index;
        memcpy((char *)&(vm_set.vifs[ifindex]), (char *)&(arg->vintf_infos[i]), sizeof(agent_interface_id_t));
        if (vm_set.vif_num >= LC_VM_MAX_VIF) {
            break;
        }
    }

    if (arg->hostinfo.host_htype == LC_POOL_TYPE_XEN) {
        ret = lcs_vm_vif_req_usage_rt(&vm_set, arg->hostinfo.host_address, MSG_VM_INFO_REQUEST, &vm);
        if (ret < 0) {
            pack_err_response_json_msg(root, "get vm info failed");
            return -1;
        }
        pack_vm_response_json_msg(root, &arg->vm_info, &vm, buf);
    }

    return 0;
}

static int lcs_vgateway_req_parse(api_arg_t *arg, nx_json *root, char *buf)
{
    agent_vm_vif_id_t vm_set;
    vm_result_t vm;
    int ret;
    int type = g_msg[arg->idx].type;
    int id = g_msg[arg->idx].id;

    memset(&vm_set, 0, sizeof(vm_set));
    memset(&vm, 0, sizeof(vm));

    vm_set.vm_type = type;
    vm_set.vm_id = id;

    ret = lcs_vgw_vif_req_usage_rt(&vm_set, arg->hostinfo.host_address, &vm);
    if (ret < 0) {
        pack_err_response_json_msg(root, "get vgateway info failed");
        return -1;
    }
    pack_vgw_response_json_msg(root, &arg->vm_info, &vm, buf);

    return 0;
}

static int handle_api_request(char *p_res, int res_buf_len, api_arg_t *arg)
{
    nx_json *res_json = NULL;
    char buf[MAX_DATA_BUF_LEN];
    int res_len;

    if (!p_res) {
        return -1;
    }

    res_json = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == res_json) {
        return -1;
    }
    memset((void *)res_json, 0, sizeof(nx_json));
    res_json->type = NX_JSON_OBJECT;
    switch (g_msg[arg->idx].type) {
        case LC_VM_TYPE_VM:
            SNF_SYSLOG(LOG_INFO, "%s: request vm info\n", __FUNCTION__);
            lcs_vgw_vm_req_parse(arg, res_json, buf);
            break;
        case LC_VM_TYPE_GW:
            SNF_SYSLOG(LOG_INFO, "%s: request vgw info\n", __FUNCTION__);
            lcs_vgw_vm_req_parse(arg, res_json, buf);
            break;
        case LC_VM_TYPE_VGATEWAY:
            SNF_SYSLOG(LOG_INFO, "%s: request vgateway info\n", __FUNCTION__);
            lcs_vgateway_req_parse(arg, res_json, buf);
            break;
        default:
            break;
    }

    res_len = dump_json_msg(res_json, p_res, res_buf_len);
    nx_json_free(res_json);
    return res_len;
}

static int get_valid_msg(int type, int id)
{
    int i, idx = 0, flag = 0;
    time_t now = time(NULL);

    for (i = 0; i < MAX_ID_COUNT; i++) {
        if (g_msg[i].time && (now - g_msg[i].time) > SECONDS_PER_MINUTES) {
            api_msg_buf_destroy(&g_msg[i]);
        }

        if (!g_msg[i].time && !flag) {
            idx = i;
            flag = 1;
        }

        if (g_msg[i].type == type && g_msg[i].id == id) {
            idx = i;
            flag = 1;
            break;
        }
    }

    if (!flag) {
        return -1;
    }

    if (!g_msg[idx].time) {
        if (api_msg_buf_init(&g_msg[idx], type, id) < 0) {
            return -1;
        }
    } else {
        g_msg[idx].time = now;
    }
    SNF_SYSLOG(LOG_INFO, "%s: get valid msg buf %d\n", __FUNCTION__, idx);

    return idx;
}


void *api_handle_thread(void *data)
{
    api_arg_t *arg = (api_arg_t *)data;
    int idx = arg->idx;
    int location = arg->location;
    int i;

    pthread_detach(pthread_self());

    pthread_mutex_lock(&g_msg[idx].msg[location].lock);
    if (!g_msg[idx].msg[location].len) {
        g_msg[idx].msg[location].len = handle_api_request(g_msg[idx].msg[location].buf, MAX_BUFFER_LENGTH, arg);
    }
    SNF_SYSLOG(LOG_INFO, "%s: send %s\n", __FUNCTION__, g_msg[idx].msg[location].buf);
    send(arg->fd, g_msg[idx].msg[location].buf, g_msg[idx].msg[location].len, 0);
    close(arg->fd);
    g_msg[idx].msg[location].len = 0;
    memset(g_msg[idx].msg[location].buf, 0, MAX_BUFFER_LENGTH);
    pthread_mutex_unlock(&g_msg[idx].msg[location].lock);

    for (i = (location + 1) % MAX_BUF_COUNT; i != location; i = (i + 1) % MAX_BUF_COUNT) {
        if (!pthread_mutex_trylock(&g_msg[idx].msg[i].lock)) {
            g_msg[idx].msg[i].len = handle_api_request(g_msg[idx].msg[i].buf, MAX_BUFFER_LENGTH, arg);
            pthread_mutex_unlock(&g_msg[idx].msg[i].lock);
        }
    }

    free(data);
    return NULL;
}

static int api_msg_process(int fd)
{
    int sk_fd = -1, nrecv = 0;
    struct sockaddr_un sk_addr;
    socklen_t sk_len = sizeof (sk_addr);
    int offset = 0;
    char *p_req = NULL;
    int nbrace = 0, nquote = 0;
    const nx_json *req_json = NULL;
    const nx_json *req_type = NULL;
    const nx_json *req_id = NULL;
    int type, id;
    int idx = 0;
    api_arg_t *arg;
    pthread_t t;

    p_req = (char *)malloc(MAX_BUFFER_LENGTH);
    if (!p_req) {
        return -1;
    }

    sk_fd = accept(fd, (struct sockaddr *)&sk_addr, &sk_len);
    if (sk_fd < 0) {
        SNF_SYSLOG(LOG_INFO, " error comes when call accept!%s",
                strerror(errno));
        free(p_req);
        return -1;
    }

    do {
        nrecv = recv(sk_fd, (void *)p_req + offset, MAX_BUFFER_LENGTH - offset, 0);
        if (nrecv <= 0 || offset + nrecv >= MAX_BUFFER_LENGTH) {
            SNF_SYSLOG(LOG_ERR, "%s : recv error: nrecv=%d, %s\n",
                    __FUNCTION__, nrecv, p_req);
            close(sk_fd);
            free(p_req);
            return -1;
        }
        offset += nrecv;

        if (is_json_complete(p_req, offset, &nquote, &nbrace)) {
            break;
        }
    } while (1);
    SNF_SYSLOG(LOG_DEBUG, "%s: recv: len=%u %s\n", __FUNCTION__, offset, p_req);

    if (parse_lcsnf_json_msg(p_req, &req_json)) {
        SNF_SYSLOG(LOG_ERR, "%s: recv invalid msg\n", __FUNCTION__);
        close(sk_fd);
        free(p_req);
        return -1;
    }

    req_type = nx_json_get(req_json, LCSNF_JSON_TYPE_KEY);
    type = atoi(req_type->text_value);
    req_id = nx_json_get(req_json, LCSNF_JSON_ID_KEY);
    id = atoi(req_id->text_value);

    if ((idx = get_valid_msg(type, id)) < 0) {
        SNF_SYSLOG(LOG_INFO, "%s: no valid msg buf\n", __FUNCTION__);
        close(sk_fd);
        free(p_req);
        return -1;
    }

    arg = (api_arg_t *)malloc(sizeof(api_arg_t));
    if (!arg) {
        close(sk_fd);
        free(p_req);
        return -1;
    }
    memset(arg, 0, sizeof(api_arg_t));
    arg->fd = sk_fd;
    arg->idx = idx;
    arg->location = g_msg[idx].location;
    g_msg[idx].location = (g_msg[idx].location + 1) % MAX_BUF_COUNT;
    if (type == LC_VM_TYPE_VM) {
        if (lc_get_vm_from_db_by_id(&arg->vm_info, id) != LC_DB_VM_FOUND) {
            goto API_MSG_PROCESS_FAILED;
        }
    } else {
        if (lc_get_vedge_from_db_by_id(&arg->vm_info, id) != LC_DB_VM_FOUND) {
            goto API_MSG_PROCESS_FAILED;
        }
    }

    if (lc_vmdb_host_device_load(&arg->hostinfo, arg->vm_info.host_name, "*") != LC_DB_HOST_FOUND) {
        goto API_MSG_PROCESS_FAILED;
    }

    if ((arg->nvif = lcs_traffic_db_vif_load_by_vm(arg->vintf_infos, type, id)) < 0){
        goto API_MSG_PROCESS_FAILED;
    }

    if (pthread_create(&t, NULL, api_handle_thread, (void *)arg) != 0) {
        SNF_SYSLOG(LOG_DEBUG, "%s: create api handle thread error.\n",
                __FUNCTION__);
        goto API_MSG_PROCESS_FAILED;
    }

    free(p_req);
    return 0;

API_MSG_PROCESS_FAILED:
    free(arg);
    free(p_req);
    close(sk_fd);
    api_msg_buf_destroy(&g_msg[idx]);
    return -1;
}

void *api_thread(void *msg)
{
    int max_fd = 0;
    fd_set fds;

    pthread_detach(pthread_self());

    /* db */
    if (lc_db_thread_init() < 0) {
        SNF_SYSLOG(LOG_ERR, "%s: mysqldb thread init error\n", __FUNCTION__);
        exit(1);
    }

    /* socket */
    max_fd = lc_sock.sock_rcv_api + 1;

    for (;;) {
        FD_ZERO(&fds);
        FD_SET(lc_sock.sock_rcv_api, &fds);

        if (select(max_fd, &fds, NULL, NULL, NULL) > 0) {
            if (FD_ISSET(lc_sock.sock_rcv_api, &fds)) {
                api_msg_process(lc_sock.sock_rcv_api);
            }
        }
    }

    return NULL;
}
