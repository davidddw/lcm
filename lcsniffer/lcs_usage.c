/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Xiang Yang
 * Finish Date: 2013-08-09
 *
 */

#include "lcs_socket.h"

#include <assert.h>
#include <signal.h>
#include <sys/time.h>

#include "lc_snf.h"
#include "lc_mongo_db.h"
#include "lc_agent_msg.h"

#include "lc_db.h"
#include "lcs_utils.h"
#include "lcs_socket.h"
#include "lc_snf_qctrl.h"
#include "lcs_usage_db.h"
#include "lcs_usage_xen.h"
#include "lcs_usage_vmware.h"
#include "lc_db_sys.h"
#include "cloudmessage.pb-c.h"
#include "agexec_inner.h"
#include "lc_bus.h"
#include "lc_header.h"
#include "nxjson.h"
#include "lc_livegate_api.h"
#include "lc_lcmg_api.h"
#include "framework/list.h"

#define INSERT_COLS "vif_id, \
rx_bytes, rx_errors, rx_dropped, rx_packets, rx_bps, rx_pps, \
tx_bytes, tx_errors, tx_dropped, tx_packets, tx_bps, tx_pps"

static int agent_data_msg_handler(u32 ori_time, int type, char *data, char *addr, int data_len);
static int agent_data_msg_enque(const char *agent_ip, const char *buf, int size);
static int agent_data_msg_recv(int fd);
static void usage_timer_handler(int sig);
static void nsp_timer_handler(int sig);

#define MONGO_VM_SERVER_BATCH_INSERT_NUM 80
#define MONGO_VM_BATCH_INSERT_NUM 80

#define MIN(A, B) ((A) < (B) ? (A) : (B))

int lc_vmdb_get_sr_disk_info(sr_disk_info_t *sr_disk_info, char *ip);
int lc_bus_lcsnf_publish_unicast(char *msg, int len, uint32_t queue_id);

struct list_head head_vm_server_load;
struct list_head head_vm_load;

pthread_mutex_t mutex_vm = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond_vm = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_vmser = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond_vmser = PTHREAD_COND_INITIALIZER;

pthread_mutex_t lc_socket_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lc_socket_queue_cond = PTHREAD_COND_INITIALIZER;

typedef struct vm_server_load_list
{
    struct list_head list;
    vm_server_load_t vm_server_load;
}vm_server_load_list_t;

typedef struct vm_server_list
{
    struct list_head list;
    vm_load_t vm_load;
}vm_load_list_t;

static const char *vm_lookup_table[] =
{
    "Temp",
    "Creating",
    "Complete",
    "Starting",
    "Running",
    "Suspending",
    "Paused",
    "Resuming",
    "Stopping",
    "Halted",
    "Modify",
    "Error",
    "Destroying",
    "Destroyed"
};

static const char *gw_lookup_table[] =
{
    "Temp",
    "Creating",
    "Complete",
    "Error",
    "Modify",
    "Destroying",
    "Starting",
    "Running",
    "Stopping",
    "Halted",
    "Destroyed"
};

static inline
int lc_monitor_care_vmstate(int state)
{
    if ((state == LC_VM_STATE_RUNNING) ||
        (state == LC_VM_STATE_TMP) ||
        (state == LC_VM_STATE_DESTROYED)  ||
        (state == LC_VM_STATE_STOPPED)) {
        return 1;
    }
    return 0;
}

static inline
int lc_monitor_care_gwstate(int state)
{
    if ((state == LC_VEDGE_STATE_RUNNING) ||
        (state == LC_VEDGE_STATE_STOPPED) ||
        (state == LC_VEDGE_STATE_TMP) ||
        (state == LC_VEDGE_STATE_DESTROYED)) {
        return 1;
    }
    return 0;
}

uint64_t ntohl64(uint64_t arg64)
{
  uint64_t res64;

#if __BYTE_ORDER==__LITTLE_ENDIAN
  uint32_t low = (uint32_t) (arg64 & 0x00000000FFFFFFFFLL);
  uint32_t high = (uint32_t) ((arg64 & 0xFFFFFFFF00000000LL) >> 32);

  low = ntohl(low);
  high = ntohl(high);

  res64 = (uint64_t) high + (((uint64_t) low) << 32);
#else
  res64 = arg64;
#endif

  return res64;
}

uint64_t htonl64(uint64_t arg64)
{
  uint64_t res64;

#if __BYTE_ORDER==__LITTLE_ENDIAN
  uint32_t low = (uint32_t) (arg64 & 0x00000000FFFFFFFFLL);
  uint32_t high = (uint32_t) ((arg64 & 0xFFFFFFFF00000000LL) >> 32);

  low = htonl(low);
  high = htonl(high);

  res64 = (uint64_t) high + (((uint64_t) low) << 32);
#else
  res64 = arg64;
#endif

  return res64;
}

int lcs_interface_result_ntoh(interface_result_t *msg)
{
    if (msg) {
        msg->vif_id     = ntohl(msg->vif_id);
        msg->rx_bytes   = ntohl64(msg->rx_bytes);
        msg->rx_dropped = ntohl64(msg->rx_dropped);
        msg->rx_errors  = ntohl64(msg->rx_errors);
        msg->rx_packets = ntohl64(msg->rx_packets);
        msg->rx_bps     = ntohl64(msg->rx_bps);
        msg->rx_pps     = ntohl64(msg->rx_pps);
        msg->tx_bytes   = ntohl64(msg->tx_bytes);
        msg->tx_dropped = ntohl64(msg->tx_dropped);
        msg->tx_errors  = ntohl64(msg->tx_errors);
        msg->tx_packets = ntohl64(msg->tx_packets);
        msg->tx_bps     = ntohl64(msg->tx_bps);
        msg->tx_pps     = ntohl64(msg->tx_pps);
    }

    return 0;
}

/* TODO: */
int lcs_hwdev_result_ntoh(hwdev_result_t *msg)
{
    int j;
    msg->curr_time = ntohl(msg->curr_time);
    msg->cpu_num = ntohl(msg->cpu_num);
    msg->dsk_num = ntohl(msg->dsk_num);
    msg->mem_data.used = ntohl(msg->mem_data.used);
    msg->mem_data.size = ntohl(msg->mem_data.size);
    for (j = 0; j < msg->dsk_num; j++) {
        msg->dsk_data[j].size = ntohl(msg->dsk_data[j].size);
        msg->dsk_data[j].used = ntohl(msg->dsk_data[j].used);
    }
    SNF_SYSLOG(LOG_DEBUG, "msg->curr_time=%lu,"
                   "msg->hwdev_ip=%s,"
                   "msg->mac=%s,"
                   "msg->sys_uptime=%s,"
                   "msg->sys_os=%s,"
                   "msg->cpu_num=%d,"
                   "msg->dsk_num=%d,"
                   "msg->cpu_type=%s,",
                   msg->curr_time,
                   msg->hwdev_ip,
                   msg->mac,
                   msg->sys_uptime,
                   msg->sys_os,
                   msg->cpu_num,
                   msg->dsk_num,
                   msg->cpu_type);
    for (j = 0; j < msg->cpu_num; j++)
    {
        SNF_SYSLOG(LOG_DEBUG, "msg->cpu_data[%d].usage=%s",
                      j,msg->cpu_data[j].usage);
    }
    for (j = 0; j < msg->dsk_num; j++)
    {
        SNF_SYSLOG(LOG_DEBUG, "msg->dsk_data[%d].size=%d,"
                        "msg->dsk_data[%d].used=%d,"
                        "msg->dsk_data[%d].usage=%s,"
                        "msg->dsk_data[%d].type=%s,",
                        j,msg->dsk_data[j].size,
                        j,msg->dsk_data[j].used,
                        j,msg->dsk_data[j].usage,
                        j,msg->dsk_data[j].type);
    }

    return 0;
}

int lcs_vm_vif_result_ntoh(vm_result_t *msg, int vif_num, u32 ori_time)
{
    int i;
    if (msg) {
        msg->vm_type    = ntohl(msg->vm_type);
        msg->vm_id      = ntohl(msg->vm_id);
        msg->vdc_id     = ntohl(msg->vdc_id);
        msg->host_id    = ntohl(msg->host_id);
        msg->curr_time  = ori_time;
        for(i = 0;i < vif_num;i++){
            lcs_interface_result_ntoh(&msg->vifs[i]);
        }
    }

    return 0;
}

int lcs_host_result_ntoh(msg_host_info_response_t *msg, u32 ori_time)
{
    int i;
    msg->curr_time = ori_time;
    msg->cpu_count = ntohl(msg->cpu_count);
    msg->role = ntohl(msg->role);
    msg->state = ntohl(msg->state);
    msg->io_read = ntohl64(msg->io_read);
    msg->io_write = ntohl64(msg->io_write);
    msg->n_sr = ntohl(msg->n_sr);
    msg->dom0_mem_total  = ntohl(msg->dom0_mem_total);
    msg->dom0_mem_usage  = ntohl(msg->dom0_mem_usage);
    msg->dom0_disk_total = ntohl(msg->dom0_disk_total);
    msg->dom0_disk_usage = ntohl(msg->dom0_disk_usage);

    for(i = 0;i< MAX_SR_NUM;i++){
        msg->sr_usage[i].is_shared = ntohl(msg->sr_usage[i].is_shared);
        msg->sr_usage[i].type = ntohl(msg->sr_usage[i].type);
        msg->sr_usage[i].disk_total = ntohl(msg->sr_usage[i].disk_total);
        msg->sr_usage[i].disk_used = ntohl(msg->sr_usage[i].disk_used);
    }

    for (i = 0; i < LC_N_BR_ID; i++) {

        msg->net_usage[i].iftype = ntohl64(msg->net_usage[i].iftype);
        msg->net_usage[i].rx_bytes = ntohl64(msg->net_usage[i].rx_bytes);
        msg->net_usage[i].rx_packets = ntohl64(msg->net_usage[i].rx_packets);
        msg->net_usage[i].rx_bps = ntohl64(msg->net_usage[i].rx_bps);
        msg->net_usage[i].rx_pps = ntohl64(msg->net_usage[i].rx_pps);
        msg->net_usage[i].tx_bytes = ntohl64(msg->net_usage[i].tx_bytes);
        msg->net_usage[i].tx_packets = ntohl64(msg->net_usage[i].tx_packets);
        msg->net_usage[i].tx_bps = ntohl64(msg->net_usage[i].tx_bps);
        msg->net_usage[i].tx_pps = ntohl64(msg->net_usage[i].tx_pps);
    }
    return 0;
}

int lc_get_vm_current_state(char *state, int type)
{
    if (strcmp(state, "running") == 0) {
        return ((type == LC_VM_TYPE_VM)?
               LC_VM_STATE_RUNNING:LC_VEDGE_STATE_RUNNING);
    } else if (strcmp(state, "halted") == 0 ) {
        return ((type == LC_VM_TYPE_VM)?
               LC_VM_STATE_STOPPED:LC_VEDGE_STATE_STOPPED);
    } else if (strcmp(state, "nonexistence") == 0 ) {
        return ((type == LC_VM_TYPE_VM)?
               LC_VM_STATE_ERROR:LC_VEDGE_STATE_ERROR);
    }
    return LC_VM_STATE_UNDEFINED;
}

int usage_timer_init(void)
{
    struct itimerval val;

    val.it_value.tv_sec = SNF_VIF_TRAFFIC_TIMER;
    val.it_value.tv_usec = 0;
    val.it_interval.tv_sec = SNF_VIF_TRAFFIC_TIMER;
    val.it_interval.tv_usec = 0;

    setitimer(ITIMER_REAL, &val, NULL);
    signal(SIGALRM, usage_timer_handler);

    return 0;
}

int lc_snf_init_bus()
{
    int res;

    res = lc_bus_init();
    if (res) {
        SNF_SYSLOG(LOG_ERR, "init lc_bus failed");
        return -1;
    }

    res = lc_bus_init_consumer(0, LC_BUS_QUEUE_LCSNFD);
    if (res) {
        SNF_SYSLOG(LOG_ERR, "init lc_bus consumer failed");
        lc_bus_exit();
        return -1;
    }

    return 0;
}

static void vmware_msg_handler(char *buf, int len)
{
    Cloudmessage__Message *msg = NULL;

    if (!buf) {
        return;
    }

    msg = cloudmessage__message__unpack(NULL, len, (uint8_t *)buf);
    if (!msg) {
        return;
    }

    if (msg->vm_vif_usage_response) {
        agent_data_msg_handler(time(NULL), LC_MSG_VCD_REPLY_VMD_VM_VIF_USAGE,
                  (char *)msg->vm_vif_usage_response, NULL, len);
    } else if (msg->host_info_response) {
        agent_data_msg_handler(time(NULL), LC_MSG_VCD_REPLY_VMD_HOST_STATS,
                   (char *)msg->host_info_response, NULL, len);
    }

    cloudmessage__message__free_unpacked(msg, NULL);

    return;
}

int lc_bus_msg_process(amqp_connection_state_t *conn)
{
    amqp_envelope_t envelope;
    amqp_frame_t frame;
    amqp_message_t message;
    char buf[LC_BUS_BUFFER_SIZE] = {0};
    int buf_len;
    header_t new_hdr;
    int pb_hdr_len = get_message_header_pb_len();

    buf_len = lc_bus_consume_message(conn,
            0, &envelope, &frame, &message, buf, sizeof(buf));
    if (buf_len >= pb_hdr_len &&
            unpack_message_header((uint8_t *)buf, pb_hdr_len, &new_hdr) == 0 &&
            pb_hdr_len + new_hdr.length == buf_len) {

        vmware_msg_handler((char *)(buf + pb_hdr_len), new_hdr.length);
        /* this is a protobuf message */
        SNF_SYSLOG(LOG_DEBUG, "%s: msg in sender=%d type=%d len=%d\n",
                __FUNCTION__, new_hdr.sender, new_hdr.type, new_hdr.length);

        return 0;
    }

    return -1;
}

void *usage_handle_thread(void *msg)
{
    /* db */
    if (lc_db_thread_init() < 0) {
        SNF_SYSLOG(LOG_ERR, "%s: mysqldb thread init error\n", __FUNCTION__);
        exit(1);
    }
    if (lc_db_master_thread_init() < 0) {
        SNF_SYSLOG(LOG_ERR, "%s: master mysqldb thread init error\n", __FUNCTION__);
        exit(1);
    }
    if (mongo_handler_init() < 0) {
        SNF_SYSLOG(LOG_ERR, "%s: mongo_handler_init error %d\n", __FUNCTION__, __LINE__);
        exit(1);
    }
    sleep(2);

    lc_socket_queue_init(&lc_socket_queue);

    while (1) {
        lc_socket_queue_data_t *pdata = NULL;

        pthread_mutex_lock(&lc_socket_queue_mutex);
        while (1) {
            if (!lc_is_socket_queue_empty(&lc_socket_queue)) {
                pdata = lc_socket_queue_dq(&lc_socket_queue);
                break;
            }
            pthread_cond_wait(&lc_socket_queue_cond, &lc_socket_queue_mutex);
        }
        pthread_mutex_unlock(&lc_socket_queue_mutex);

        /* process recevied message here */
        if (pdata) {
            lc_mbuf_hdr_t *hdr = NULL;
            hdr = (lc_mbuf_hdr_t *)pdata->buf;
            agent_data_msg_handler(hdr->time, hdr->type,
                    (char *)(hdr + 1), pdata->agent_ip, pdata->size);
            free(pdata);
        }
    }

    mongo_handler_exit();
    return NULL;
}

void *usage_socket_thread(void *msg)
{
    int max_fd = 0;
    fd_set fds;
    amqp_connection_state_t *conn = lc_bus_get_connection();
    int lc_snf_conn_fd = -1;

    /* request timer */
    usage_timer_init();

    /* lc bus */
    if (lc_snf_init_bus() != 0) {
        exit(-1);
    }

    /* socket */
    max_fd = lc_sock.sock_rcv_agent;
    max_fd = MAX(lc_sock.sock_rcv_chk, max_fd);
    ++max_fd;

    lc_snf_conn_fd = amqp_get_sockfd(*conn);
    while (lc_bus_data_in_buffer(conn)) {
        if (lc_bus_msg_process(conn)) {
            break;
        }
    }

    for (;;) {
        /* in case of bus connection reset */
        conn = lc_bus_get_connection();
        lc_snf_conn_fd = amqp_get_sockfd(*conn);
        if (lc_snf_conn_fd + 1 > max_fd) {
            max_fd = lc_snf_conn_fd + 1;
        }

        FD_ZERO(&fds);
        FD_SET(lc_snf_conn_fd, &fds);
        FD_SET(lc_sock.sock_rcv_agent, &fds);

        if (lc_sock.sock_rcv_chk > 0) {
            FD_SET(lc_sock.sock_rcv_chk, &fds);
        }

        if (select(max_fd, &fds, NULL, NULL, NULL) > 0) {
            if (FD_ISSET(lc_snf_conn_fd, &fds)) {
                do {
                    if (lc_bus_msg_process(conn)) {
                        break;
                    }
                } while (lc_bus_data_in_buffer(conn));
            }

            if (FD_ISSET(lc_sock.sock_rcv_agent, &fds)) {
                agent_data_msg_recv(lc_sock.sock_rcv_agent);
            }
        }
    }

    return NULL;
}

int lc_vcd_vm_stats_msg(agent_vm_id_t *vm, int count, void *buf)
{
    Cloudmessage__VMStatsReq msg = CLOUDMESSAGE__VMSTATS_REQ__INIT;
    int size;
    int i;

    msg.n_vm_name = count;
    msg.vm_name = (char **)malloc(count * sizeof(char *));
    if (!msg.vm_name) {
        return -1;
    }
    for (i = 0; i < count; ++i) {
        msg.vm_name[i] = vm[i].vm_label;
    }

    cloudmessage__vmstats_req__pack(&msg, buf);

    size = cloudmessage__vmstats_req__get_packed_size(&msg);

    free(msg.vm_name);

    return size;
}

int host_timer_handler(host_device_t *hostinfo)
{
    host_device_t *host_info = hostinfo;

    if (host_info->htype == LC_POOL_TYPE_XEN) {
        SNF_SYSLOG(LOG_INFO,
                "Will request %d xen hosts' usage MSG_HOST_INFO_REQUEST "
                "from %s time:%ld.\n",
                1, host_info->ip, time(NULL));
        lc_agent_request_host_usage(host_info->ip);
    }

    if (host_info->htype == LC_POOL_TYPE_KVM && host_info->type == LC_SERVER_TYPE_VM) {
        SNF_SYSLOG(LOG_INFO,
                "Will request %d kvm hosts' usage MSG_HOST_INFO_REQUEST "
                "from %s time:%ld.\n",
                1, host_info->ip, time(NULL));
        lc_agent_request_host_usage(host_info->ip);
    }
    return 0;
}

int vm_attachs_vifs(agent_vm_vif_id_t *vm_set,
                    agent_interface_id_t *vintf_infos,
                    int host_vif_num,
                    host_device_t *host_info)
{
    int i = 0, ifindex = 0;
    for (i = 0;i < host_vif_num;i++){
        if(0 == strncmp(vm_set->vm_label, vintf_infos[i].vm_name,
            AGENT_NAME_LEN))
        {
            ifindex = vintf_infos[i].if_index;
            if(ifindex < LC_VM_MAX_VIF){
                vm_set->vif_num++;
                memcpy((char *)&(vm_set->vifs[ifindex]),
                       (char *)&(vintf_infos[i]),
                       sizeof(agent_interface_id_t));
            }
            if(vm_set->vif_num >= LC_VM_MAX_VIF){
                break;
            }
        }
    }
    return 0;
}

static int log_agent_vm_vif_id_t(agent_vm_vif_id_t *vm_set, int vm_count, const char *fun_name)
{
#ifdef DEBUG_LCSNFD
    int i = 0;
    int j = 0;
    for(j = 0;j < vm_count;j++){
        SNF_SYSLOG(LOG_DEBUG,
                   "%s: , "
                   "vm_label %s , "
                   "vm_type %u , "
                   "vm_id %u , "
                   "vdc_id %u , "
                   "host_id %u , "
                   "vm_res_type %u , "
                   "vif_num %u\n",
                   fun_name,
                   vm_set->vm_label,
                   vm_set->vm_type,
                   vm_set->vm_id,
                   vm_set->vdc_id,
                   vm_set->host_id,
                   vm_set->vm_res_type,
                   vm_set->vif_num
                    );
        for(i = 0;i < vm_set->vif_num; i++){
            SNF_SYSLOG(LOG_DEBUG,
                "%s: , "
                "vm_label %s , "
                "vif %d , "
                "vif_id %u , "
                "mac %s , "
                "vm_uuid %s , "
                "vm_name %s , "
                "vlan_tag %u , "
                "ofport %u , "
                "if_index %u , "
                "if_subnetid %u , "
                "device_type %u , "
                "device_id %u , ",
                fun_name,
                vm_set->vm_label,
                i,
                vm_set->vifs[i].vif_id,
                vm_set->vifs[i].mac,
                vm_set->vifs[i].vm_uuid,
                vm_set->vifs[i].vm_name,
                vm_set->vifs[i].vlan_tag,
                vm_set->vifs[i].ofport,
                vm_set->vifs[i].if_index,
                vm_set->vifs[i].if_subnetid,
                vm_set->vifs[i].device_type,
                vm_set->vifs[i].device_id
                );
        }
        vm_set++;
    }
#endif
    return 0;
}

static int log_agent_hwdev_t(agent_hwdev_t *hwdev_set, const char *fun_name)
{
    SNF_SYSLOG(LOG_DEBUG,
               "%s:"
               "hwdev_set->hwdev_ip: %s , "
               "hwdev_set->community %s , "
               "hwdev_set->username %s , "
               "hwdev_set->dpassword %s \n ",
               fun_name,
               hwdev_set->hwdev_ip,
               hwdev_set->community,
               hwdev_set->username,
               hwdev_set->password);

    return 0;
}

int lc_get_compute_resource(host_t *host_info, char *ip)
{
    int code;

    if (!ip) {
        return LC_DB_COMMON_ERROR;
    }

    code = lc_vmdb_compute_resource_load(host_info, ip);

    if (code == LC_DB_HOST_NOT_FOUND) {
        SNF_SYSLOG(LOG_ERR, "%s host %s not found\n", __FUNCTION__, ip);
    }

    return code;
}

static int log_nsp_host_stat_t(void *entry, const char *fun_name, int type)
{
#ifdef DEBUG_LCSNFD
    vgw_server_load_t *vgw_server_load = (vgw_server_load_t *)entry;

    SNF_SYSLOG(LOG_DEBUG,
            "%s: %s, "
            "name %s, "
            "server_ip %s, "
            "htype %d, "
            "timestamp %lu, "
            "cpu_usage %s , "
            "mem_total %lf, "
            "mem_usage %lf, "
            "disk_total %lf, "
            "disk_usage %lf, "
            "ha_disk_usage %lf, ",
            fun_name, __FUNCTION__,
            vgw_server_load->object.name,
            vgw_server_load->object.server_ip,
            vgw_server_load->object.htype,
            vgw_server_load->timestamp,
            vgw_server_load->system.cpu_usage,
            vgw_server_load->system.mem_total,
            vgw_server_load->system.mem_usage,
            vgw_server_load->system.disk_total,
            vgw_server_load->system.disk_usage,
            vgw_server_load->system.ha_disk_usage);
#endif
    return 0;
}

static int get_nsp_host_res_usage(monitor_host_t *phost, nsp_host_stat_t *host_stat)
{
    vgw_server_load_t vgw_server_load;

    memset(&vgw_server_load, 0, sizeof(vgw_server_load_t));

    strcpy(vgw_server_load.object.name, phost->host_name);
    strcpy(vgw_server_load.object.server_ip, phost->host_address);
    vgw_server_load.object.htype = LC_HOST_HTYPE_KVM;

    vgw_server_load.timestamp = time(NULL);

    strcpy(phost->cpu_usage, host_stat->cpu_usage);
    phost->mem_total = host_stat->mem_total;
    phost->mem_usage = host_stat->mem_usage;

    strcpy(vgw_server_load.system.cpu_usage, host_stat->cpu_usage);
    vgw_server_load.system.mem_usage = host_stat->mem_usage;
    vgw_server_load.system.mem_total = host_stat->mem_total;
    vgw_server_load.system.disk_usage = phost->disk_usage;
    vgw_server_load.system.disk_total = phost->disk_total;

    memcpy(&vgw_server_load.traffic, &host_stat->traffic, sizeof(traffic_t) * LC_N_BR_ID);

    log_nsp_host_stat_t(&vgw_server_load, __FUNCTION__, 1);

    if (mongo_db_entry_insert(MONGO_APP_VGW_SERVER_LOAD, &vgw_server_load) < 0) {
        SNF_SYSLOG(LOG_ERR, "Insert resource usage db errno on line:%d", __LINE__);
        return -1;
    }

    return 0;
}

static int nsp_host_stat_parse(host_device_t *host_info, nsp_host_stat_t *host_stat)
{
    monitor_host_t mon_host;
    host_t host;

    memset(&mon_host, 0, sizeof(monitor_host_t));
    memset(&host, 0, sizeof(host_t));

    if (lc_get_compute_resource(&host, host_info->ip)
            != LC_DB_HOST_FOUND) {
        return -1;
    }

    mon_host.cr_id = host.cr_id;
    strcpy(mon_host.cpu_info, host.cpu_info);
    strcpy(mon_host.host_address, host.host_address);
    strncpy(mon_host.host_name, host.host_name, MAX_HOST_NAME_LEN);

    get_nsp_host_res_usage(&mon_host, host_stat);

    lc_vmdb_compute_resource_usage_update_v2(mon_host.cpu_usage,
                                              mon_host.cpu_used,
                                             mon_host.mem_total,
                                             mon_host.mem_alloc,
                                            mon_host.disk_alloc,
                                         mon_host.ha_disk_usage,
                                         mon_host.host_address);
    return 0;
}

static int log_nsp_vgw_load_t(vgw_load_t *entry, int j)
{
#ifdef DEBUG_LCSNFD
    int i;
    SNF_SYSLOG(LOG_DEBUG,
               "%s, "
               "vgw_id %d, ",
               __FUNCTION__,
               entry->object.vgw_id);

    for (i = 0; i < j; i++){
        SNF_SYSLOG(LOG_DEBUG,
                "ifindex %d, "
                "rx_bps %"PRIu64", "
                "rx_pps %"PRIu64", "
                "tx_bps %"PRIu64", "
                "tx_pps %"PRIu64", ",
                entry->vif_info[i].ifindex,
                entry->vif_info[i].traffic.rx_bps,
                entry->vif_info[i].traffic.rx_pps,
                entry->vif_info[i].traffic.tx_bps,
                entry->vif_info[i].traffic.tx_pps);
    }
#endif
    return 0;
}


static int nsp_stat_process(host_device_t *host_info, char *result)
{
    const nx_json *data = NULL, *datum = NULL, *item = NULL, *json = NULL;
    const nx_json *item_j = NULL, *json_j = NULL;
    vgw_load_t vgw_load;    // for nsp_vgw_load
    nsp_host_stat_t host_stat;       // for nsp_host_usage, nsp_vgw_server_load
    nsp_traffic_stat_t traffic_stat; // for nsp_traffic_usage
    int cpu_id, vif_id;
    double cpu_usage_i;
    char cpu_usage_a[LC_HOST_CPU_USAGE];
    int i, j, k;
    int rv;
    char sql_query[512];
    int ret = 0;
    vm_info_t vm_info;
    long vif_rx_bps, vif_tx_bps, vif_rx_pps, vif_tx_pps;

    data = nx_json_get(*((const nx_json **)result), LCSNF_JSON_DATA_KEY);
    if (data->type == NX_JSON_NULL) {
        SNF_SYSLOG(LOG_ERR, "invalid json recvived, no data");
        return -1;
    }

    memset(&host_stat, 0, sizeof(nsp_host_stat_t));
    memset(&traffic_stat, 0, sizeof(nsp_traffic_stat_t));

    /* CPU */
    datum = nx_json_get(data, "HOST_CPUS");
    if (datum->type == NX_JSON_NULL) {
        SNF_SYSLOG(LOG_ERR, "invalid json recvived, no datum");
        return -1;
    }
    i = 0;
    do {
        item = nx_json_item(datum, i);
        if (item->type == NX_JSON_NULL) {
            break;
        }

        json = nx_json_get(item, "CPU_ID");
        cpu_id = json->int_value;
        json = nx_json_get(item, "LOAD");
        cpu_usage_i = json->int_value;
        memset(cpu_usage_a, 0, sizeof(cpu_usage_a));
        sprintf(cpu_usage_a, "%d,%.3f#", cpu_id, (cpu_usage_i*1.0/100));
        strcat(host_stat.cpu_usage, cpu_usage_a);
        if (strlen(host_stat.cpu_usage) > sizeof(host_stat.cpu_usage)) {
            SNF_SYSLOG(LOG_ERR, "excessive cpu info");
            return -1;
        }
        i++;
    } while (1);
    host_stat.cpu_num = i;

    /* MEM */
    datum = nx_json_get(data, "HOST_MEMORY");
    if (datum->type == NX_JSON_NULL) {
        SNF_SYSLOG(LOG_ERR, "invalid json recvived, no datum");
        return -1;
    }
    json = nx_json_get(datum, "TOTAL");
    host_stat.mem_total = json->int_value;
    json = nx_json_get(datum, "FREE");
    host_stat.mem_usage = host_stat.mem_total - json->int_value;

    host_stat.mem_total = (int64_t)(host_stat.mem_total * 1.0 / 1024);
    host_stat.mem_usage = (int64_t)(host_stat.mem_usage * 1.0 / 1024);

    /* HOST TRAFFIC */
    datum = nx_json_get(data, "HOST_TRAFFICS");
    if (datum->type == NX_JSON_NULL) {
        SNF_SYSLOG(LOG_ERR, "invalid json recvived, no datum");
        return -1;
    }
    i = 0;
    do {
        item = nx_json_item(datum, i);
        if (item->type == NX_JSON_NULL) {
            break;
        }

        json = nx_json_get(item, "IF_INDEX");
        vif_id = json->int_value;
        json = nx_json_get(item, "RX_BPS");
        vif_rx_bps = json->int_value;
        json = nx_json_get(item, "RX_PPS");
        vif_rx_pps = json->int_value;
        json = nx_json_get(item, "TX_BPS");
        vif_tx_bps = json->int_value;
        json = nx_json_get(item, "TX_PPS");
        vif_tx_pps = json->int_value;

        if (vif_id >= LC_N_BR_ID) {

            SNF_SYSLOG(LOG_ERR, "get vgw host stats error %d", vif_id);
            continue;
        }
        host_stat.traffic[vif_id].iftype = vif_id;
        host_stat.traffic[vif_id].rx_bps = vif_rx_bps;
        host_stat.traffic[vif_id].rx_pps = vif_rx_pps;
        host_stat.traffic[vif_id].tx_bps = vif_tx_bps;
        host_stat.traffic[vif_id].tx_pps = vif_tx_pps;
        i++;
    } while (1);
    SNF_SYSLOG(LOG_DEBUG, "nsp_host_stat %s", host_info->ip);
    nsp_host_stat_parse(host_info, &host_stat);

    /* VGW TRAFFIC */
    datum = nx_json_get(data, "ROUTER_TRAFFICS");
    if (datum->type == NX_JSON_NULL) {
        SNF_SYSLOG(LOG_ERR, "invalid json recvived, no datum");
        return -1;
    }
    i = 0;
    do {
        j = 0;
        memset(&vgw_load, 0, sizeof(vgw_load_t));
        vgw_load.timestamp = time(NULL);
        item = nx_json_item(datum, i);
        if (item->type == NX_JSON_NULL) {
            break;
        }

        k = 0;
        json = nx_json_get(item, "LANS");
        do {
            item_j = nx_json_item(json, k);
            if (item_j->type == NX_JSON_NULL) {
                break;
            }

            json_j = nx_json_get(item_j, "IF_INDEX");
            vgw_load.vif_info[j].ifindex = json_j->int_value;

            json_j = nx_json_get(item_j, "RX_BPS");
            vgw_load.vif_info[j].traffic.rx_bps = json_j->int_value;
            json_j = nx_json_get(item_j, "TX_BPS");
            vgw_load.vif_info[j].traffic.tx_bps = json_j->int_value;
            json_j = nx_json_get(item_j, "RX_PPS");
            vgw_load.vif_info[j].traffic.rx_pps = json_j->int_value;
            json_j = nx_json_get(item_j, "TX_PPS");
            vgw_load.vif_info[j].traffic.tx_pps = json_j->int_value;
            j++;
            k++;
        } while (1);

        json = nx_json_get(item, "ROUTER_ID");
        vgw_load.object.vgw_id = json->int_value;
        SNF_SYSLOG(LOG_DEBUG, "parse vgw %d\n", vgw_load.object.vgw_id);
        json = nx_json_get(item, "WANS");
        k = 0;
        do {
            item_j = nx_json_item(json, k);
            if (item_j->type == NX_JSON_NULL) {
                break;
            }

            json_j = nx_json_get(item_j, "IF_INDEX");
            vgw_load.vif_info[j].ifindex = json_j->int_value;

            json_j = nx_json_get(item_j, "RX_BPS");
            vgw_load.vif_info[j].traffic.rx_bps = json_j->int_value;
            json_j = nx_json_get(item_j, "TX_BPS");
            vgw_load.vif_info[j].traffic.tx_bps = json_j->int_value;
            json_j = nx_json_get(item_j, "RX_PPS");
            vgw_load.vif_info[j].traffic.rx_pps = json_j->int_value;
            json_j = nx_json_get(item_j, "TX_PPS");
            vgw_load.vif_info[j].traffic.tx_pps = json_j->int_value;

            j++;
            k++;
        } while (1);

        i++;
        memset(&vm_info, 0, sizeof(vm_info_t));
        memset(sql_query, 0 , sizeof(sql_query));
        sprintf(sql_query, "id=%d and gw_launch_server='%s'", \
                vgw_load.object.vgw_id, host_info->ip);
        if (lc_get_vedge_from_db_by_str(&vm_info, sql_query)
                != LC_DB_VM_FOUND) {
            SNF_SYSLOG(LOG_DEBUG, "MySQL no DATA");
            continue;
        }

        log_nsp_vgw_load_t(&vgw_load, j);

        rv = mongo_db_entry_insert(MONGO_APP_VGW_LOAD, &vgw_load);
        if (rv < 0) {
            SNF_SYSLOG(LOG_ERR,
                    "Insert resource usage db errno LINE %d", __LINE__);
            return -1;
        }
    } while (1);

    return ret;
}

static int nsp_stat_timer_handler(host_device_t *host_info, char *result)
{
    if (host_info->type == LC_SERVER_TYPE_NSP
            && host_info->htype == LC_POOL_TYPE_KVM) {
        SNF_SYSLOG(LOG_INFO, "%s: Will request %d nsp vgws' usage from %s.\n",
                __FUNCTION__, 1, host_info->ip);
        if (call_curl_get_nsp_stat(host_info->ip, lcsnf_domain.master_ctrl_ip, result) < 0) {
            return -1;
        }
    }

    return 0;
}

static void nsp_timer_handler(int sig)
{
    int i = 0, ret = 0;
    uint32_t hostid_offset = 1;
    host_device_t host_infos[MAX_HOST_NUM];
    char result[LCMG_API_SUPER_BIG_RESULT_SIZE];

    memset(host_infos, 0, sizeof(host_infos));

    /*
     * STEP1: Search the UP NSPs from db
     *
     * STEP2: Call API to get the statistics from each NSP
     *
     * STEP3: Analysis the result returned from the API
     *
     * STEP4: Pack into the given data structure
     *
     * STEP5: Insert into host_usage, vgw_server_load, vgw_usage, vgw_load,
     *        and traffic_usage
     *
     */

    if (handle_lookup(pthread_self()) < 0 && lc_db_thread_init() < 0) {
        SNF_SYSLOG(LOG_ERR, "%s: mysqldb thread init error\n", __FUNCTION__);
        exit(1);
    }

    if (handle_master_lookup(pthread_self()) < 0 && lc_db_master_thread_init() < 0) {
        SNF_SYSLOG(LOG_ERR, "%s: mysqldb master thread init error\n", __FUNCTION__);
        exit(1);
    }

    for (;;) {
        ret = lcs_usage_db_host_loadn_by_type(host_infos,
                hostid_offset, MAX_HOST_NUM, LC_SERVER_TYPE_NSP);
        if (ret < 0) {
            break;
        }
        if (ret > 0) {
            hostid_offset = host_infos[ret - 1].id + 1;
        }

        for (i = 0; i < ret; ++i) {
            memset(result, 0, sizeof(result));
            if (!nsp_stat_timer_handler(&host_infos[i], result)) {
                nsp_stat_process(&host_infos[i], result);
            }
        }

        if (ret < MAX_HOST_NUM) {
            break;
        }
    }

    return;
}

static int log_lb_listener_stat_t(listener_load_t *entry, const char *fun_name)
{
    SNF_SYSLOG(LOG_DEBUG,
               "%s: %s, "
               "timestamp %lu, "
               "lcuuid %s, "
               "lb_id %d, "
               "conn_num %"PRIu64"",
               fun_name, __FUNCTION__,
               entry->timestamp,
               entry->lcuuid,
               entry->lb_id,
               entry->conn_num);
    return 0;
}

static int lb_listener_stat_parse(char *lb_label, lb_listener_stat_t *listener_stat)
{
    listener_load_t entry;
    vm_info_t lb_info;
    lb_listener_t listener_info;
    int i;
    int rv;

    memset(&entry, 0, sizeof(listener_load_t));
    memset(&lb_info, 0, sizeof(vm_info_t));
    memset(&listener_info, 0, sizeof(lb_listener_t));

    if (LC_DB_VM_FOUND != lc_vmdb_lb_load_by_namelable(&lb_info, lb_label)) {
        return -1;
    }

    if (LC_DB_VM_FOUND != lc_vmdb_lb_listener_load_by_name(&listener_info,
                                                           listener_stat->name,
                                                           lb_info.vm_lcuuid)) {
        return -1;
    }

    entry.timestamp = time(NULL);
    entry.conn_num = listener_stat->conn_num;
    entry.lcuuid = listener_info.lcuuid;
    entry.lb_id = lb_info.vm_id;
    entry.bk_vm_num = listener_stat->bk_vm_num;
    for (i = 0; i < MONGO_BK_VM_NUM && i < listener_stat->bk_vm_num; i++) {
        entry.bk_vm[i].name = listener_stat->bk_vm[i].name;
        entry.bk_vm[i].conn_num = listener_stat->bk_vm[i].conn_num;
    }

    log_lb_listener_stat_t(&entry, __FUNCTION__);

    rv = mongo_db_entry_insert(MONGO_APP_LISTENER_CONN, &entry);
    if (rv < 0) {
        SNF_SYSLOG(LOG_ERR, "Insert resource usage db errno %d", __LINE__);
        return -1;
    }

    return 0;
}

static int lb_stat_process(char *result)
{
    const nx_json *data = NULL, *item = NULL, *json = NULL;
    const nx_json *item_j = NULL, *json_j = NULL, *item_k = NULL, *json_k = NULL;
    lb_listener_stat_t listener_stat;
    char lb_label[LC_NAMESIZE];
    int i, j, k;

    data = nx_json_get(*((const nx_json **)result), LCSNF_JSON_DATA_KEY);
    if (data->type == NX_JSON_NULL) {
        SNF_SYSLOG(LOG_ERR, "%s: invalid json recvived, no data\n",
                __FUNCTION__);
        return -1;
    }

    memset(&listener_stat, 0, sizeof(lb_listener_stat_t));

    i = 0;
    do {
        item = nx_json_item(data, i);
        if (item->type == NX_JSON_NULL) {
            break;
        }
        json = nx_json_get(item, "NAME");
        strcpy(lb_label, json->text_value);

        json = nx_json_get(item, "LISTENERS");
        SNF_SYSLOG(LOG_DEBUG, "json->type: %d", json->type);
        if (json->type == NX_JSON_NULL) {
            break;
        }

        j = 0;
        do {
            item_j = nx_json_item(json, j);
            if (item_j->type == NX_JSON_NULL) {
                break;
            }

            json_j = nx_json_get(item_j, "NAME");
            strcpy(listener_stat.name, json_j->text_value);
            json_j = nx_json_get(item_j, "CONN_NUM");
            listener_stat.conn_num = json_j->int_value;

            json_j = nx_json_get(item_j, "BK_VMS");
            if (json_j->type == NX_JSON_NULL) {
                break;
            }

            k = 0;
            do {
                item_k = nx_json_item(json_j, k);
                if (item_k->type == NX_JSON_NULL) {
                    break;
                }

                json_k = nx_json_get(item_k, "NAME");
                strcpy(listener_stat.bk_vm[k].name, json_k->text_value);
                json_k = nx_json_get(item_k, "CONN_NUM");
                listener_stat.bk_vm[k].conn_num = json_k->int_value;

                k++;
            } while(1);
            listener_stat.bk_vm_num = k;
            lb_listener_stat_parse(lb_label, &listener_stat);
            j++;
        } while(1);
        i++;
    } while(1);

    return 0;
}

static void lb_timer_handler(int sig)
{
    int i = 0, ret = 0;
    uint32_t hostid_offset = 1;
    host_device_t host_infos[MAX_HOST_NUM];
    char result[LCMG_API_SUPER_BIG_RESULT_SIZE];

    memset(host_infos, 0, sizeof(host_infos));

    if (handle_lookup(pthread_self()) < 0 && lc_db_thread_init() < 0) {
        SNF_SYSLOG(LOG_ERR, "%s: mysqldb thread init error\n", __FUNCTION__);
        exit(1);
    }

    for (;;) {
        ret = lcs_usage_db_host_loadn_by_type(host_infos,
                hostid_offset, MAX_HOST_NUM, LC_SERVER_TYPE_VM);
        if (ret < 0) {
            break;
        }
        if (ret > 0) {
            hostid_offset = host_infos[ret - 1].id + 1;
        }

        for (i = 0; i < ret; ++i) {
            memset(result, 0, sizeof(result));
            if (!call_curl_get_lb_stat(host_infos[i].ip, result)) {
                lb_stat_process(result);
            }
        }

        if (ret < MAX_HOST_NUM) {
            break;
        }
    }

    return;
}

static int log_switch_load_t(switch_load_t *entry)
{
#ifdef DEBUG_LCSNFD
    int i;
    SNF_SYSLOG(LOG_DEBUG,
               "%s, switch_id: %d, switch_name: %s, "
               "switch_mip: %s, timestamp: %lu, port_num: %d",
               __FUNCTION__, entry->switch_id, entry->switch_name,
               entry->switch_mip, entry->timestamp, entry->port_num);

    for (i = 0; i < entry->port_num; i++){
        SNF_SYSLOG(LOG_DEBUG,
                "switch_mip: %s, "
                "port_index: %d, "
                "port_name: %s, "
                "rx_bps %"PRIu64", "
                "rx_pps %"PRIu64", "
                "tx_bps %"PRIu64", "
                "tx_pps %"PRIu64", ",
                entry->switch_mip,
                entry->port[i].port_index,
                entry->port[i].port_name,
                entry->port[i].rx_bps,
                entry->port[i].rx_pps,
                entry->port[i].tx_bps,
                entry->port[i].tx_pps);
    }
#endif
    return 0;
}

static int switch_load_process(network_device_t *switch_info,
        char *result, switch_load_t *switch_load)
{
    const nx_json *data = NULL, *datum = NULL, *item = NULL, *json = NULL;
    int i, rv;

    if (!switch_load) {
        SNF_SYSLOG(LOG_ERR, "%s: none switch_load buffer",
                __FUNCTION__);
        return -1;
    }
    memset(switch_load, 0, sizeof(switch_load_t));

    data = nx_json_get(*((const nx_json **)result), LCSNF_JSON_DATA_KEY);
    if (data->type == NX_JSON_NULL) {
        SNF_SYSLOG(LOG_ERR, "%s: invalid json received, no data",
                __FUNCTION__);
        return -1;
    }
    item = nx_json_item(data, 0);
    if (item->type == NX_JSON_NULL) {
        SNF_SYSLOG(LOG_ERR, "%s: invalid json received, no item",
                __FUNCTION__);
        return -1;
    }
    datum = nx_json_get(item, "OUTPUT");
    if (datum->type == NX_JSON_NULL) {
        SNF_SYSLOG(LOG_ERR, "%s: invalid json received, no datum",
                __FUNCTION__);
        return -1;
    }
    i = 0;
    do {
        item = nx_json_item(datum, i);
        if (item->type == NX_JSON_NULL) {
            break;
        }

        json = nx_json_get(item, "PORT");
        switch_load->port[i].port_index = json->int_value;
        json = nx_json_get(item, "PORT_NAME");
        strncpy(switch_load->port[i].port_name, json->text_value,
                MONGO_NAME_LEN);
        json = nx_json_get(item, "RX_BPS");
        sscanf(json->text_value, "%ld", &switch_load->port[i].rx_bps);
        json = nx_json_get(item, "RX_PPS");
        sscanf(json->text_value, "%ld", &switch_load->port[i].rx_pps);
        json = nx_json_get(item, "TX_BPS");
        sscanf(json->text_value, "%ld", &switch_load->port[i].tx_bps);
        json = nx_json_get(item, "TX_PPS");
        sscanf(json->text_value, "%ld", &switch_load->port[i].tx_pps);
        i++;
    } while (1);
    switch_load->port_num = i;
    switch_load->switch_id = switch_info->id;
    strcpy(switch_load->switch_name, switch_info->name);
    strcpy(switch_load->switch_mip, switch_info->mip);
    switch_load->timestamp = time(NULL);

    log_switch_load_t(switch_load);
    rv = mongo_db_entry_insert(MONGO_APP_SWITCH_LOAD, switch_load);
    if (rv < 0) {
        SNF_SYSLOG(LOG_ERR,
                "Insert resource usage db errno LINE %d", __LINE__);
        return -1;
    }

    return 0;
}

static int switch_load_timer_handler(network_device_t *switch_info,
        char *result)
{
    SNF_SYSLOG(LOG_INFO, "%s: Will request %d switch ports' usage from %s.\n",
            __FUNCTION__, 1, switch_info->mip);
    if (call_curl_get_switch_load(switch_info->mip, result) < 0) {
        return -1;
    }

    return 0;
}

static void switch_timer_handler(int sig)
{
    int i = 0, ret = 0;
    uint32_t switchid_offset = 1;
    network_device_t switch_infos[MAX_SWITCH_NUM];
    switch_load_t switch_load;
    char result[LCMG_API_SUPER_BIG_RESULT_SIZE];

    memset(switch_infos, 0, sizeof(switch_infos));

    /*
     * STEP1: Search the UP ToR switches from db
     *
     * STEP2: Call API to get the statistics from each ToR switch
     *
     * STEP3: Analysis the result returned from the API
     *
     * STEP4: Pack into the given data structure
     *
     * STEP5: Insert into switch_load
     *
     */

    if (handle_lookup(pthread_self()) < 0 && lc_db_thread_init() < 0) {
        SNF_SYSLOG(LOG_ERR, "%s: mysqldb thread init error\n", __FUNCTION__);
        exit(1);
    }

    if (handle_master_lookup(pthread_self()) < 0 && lc_db_master_thread_init() < 0) {
        SNF_SYSLOG(LOG_ERR, "%s: mysqldb master thread init error\n", __FUNCTION__);
        exit(1);
    }

    for (;;) {
        ret = lcs_usage_db_switch_loadn(switch_infos,
                switchid_offset, MAX_SWITCH_NUM);
        if (ret < 0) {
            break;
        }
        if (ret > 0) {
            switchid_offset = switch_infos[ret - 1].id + 1;
        }

        for (i = 0; i < ret; ++i) {
            memset(result, 0, sizeof(result));
            if (!switch_load_timer_handler(&switch_infos[i], result)) {
                switch_load_process(&switch_infos[i], result, &switch_load);
            }
        }

        if (ret < MAX_SWITCH_NUM) {
            break;
        }
    }

    return;
}


int vm_timer_handler(host_device_t *hostinfo)
{
    u32 timestamp;
    host_device_t *host_info = hostinfo;
    char *server = host_info->ip;
    uint32_t htype = host_info->htype;

    int vm_number = 0, vm_count = 0;
    int i = 0;
    vm_query_info_t vm_query_set[MAX_VM_PER_HOST];
    vnet_query_info_t vnet_query_set[MAX_VM_PER_HOST];
    agent_vm_vif_id_t *vm_set = NULL, *free_vm_set = NULL;

    timestamp = (u32)time(NULL);
    int nvif = 0;
    agent_interface_id_t vintf_infos[MAX_VIF_PER_HOST];
    memset(vintf_infos, 0, sizeof(vintf_infos));
    nvif = lcs_traffic_db_vif_load_by_hostip(vintf_infos,
            MAX_VIF_PER_HOST, host_info->ip);
    if (nvif < 0) {
        SNF_SYSLOG(LOG_ERR, "Failed to load vif in host %s.\n",
                host_info->ip);
        return 1;
    }
    SNF_SYSLOG(LOG_INFO,
            "Will request vms' (%d vif) usage MSG_VM_INFO_REQUEST "
            "from %s time:%d.\n",
            nvif, server, timestamp);

    memset(vm_query_set, 0, sizeof(vm_query_info_t)*MAX_VM_PER_HOST);
    memset(vnet_query_set, 0, sizeof(vnet_query_info_t)*MAX_VM_PER_HOST);

    if(host_info->type == LC_SERVER_TYPE_VM) {
        lc_get_vm_by_server(vm_query_set, sizeof(vm_query_info_t),
                          &vm_number, server, 0, MAX_VM_PER_HOST);
        if (vm_number) {
            vm_set = malloc(sizeof(agent_vm_vif_id_t)*vm_number);
            if (!vm_set) {
                SNF_SYSLOG(LOG_ERR, "Failed to malloc agent_vm_vif_id_t");
                return -1;
            }
            memset(vm_set, 0, sizeof(agent_vm_vif_id_t)*vm_number);
            free_vm_set = vm_set;

            for (i = 0; i < vm_number; i ++) {
                 vm_query_info_t *pvm_query = &vm_query_set[i];
                 /*vm/vgw's name label MUST not be overlapped*/
                 if (!strlen(pvm_query->vm_label)) {
                     continue;
                 }
                 strcpy(vm_set->vm_label, pvm_query->vm_label);

                 vm_set->vm_type = LC_VM_TYPE_VM;
                 vm_set->vm_id = pvm_query->vm_id;
                 vm_set->vdc_id = pvm_query->vdc_id;
                 vm_set->host_id = host_info->id;
                 vm_set->vm_res_type = VM_RES_ALL;

                 vm_attachs_vifs(vm_set, vintf_infos, nvif, host_info);

                 vm_set ++;
                 vm_count ++;
            }
            log_agent_vm_vif_id_t(free_vm_set, vm_count, __FUNCTION__);

            if (htype == LC_POOL_TYPE_XEN || htype == LC_POOL_TYPE_KVM) {
                lc_agent_request_vm_vif_usage(free_vm_set, vm_count, server, timestamp);
            }
            if (free_vm_set) {
                free(free_vm_set);
            }
        }
    }
#if 0
    else if(host_info->type == LC_SERVER_TYPE_VGW) {
        lc_get_vnet_by_server(vnet_query_set, sizeof(vnet_query_info_t),
                              &vnet_number, server, 0, MAX_VM_PER_HOST);
        if (vnet_number) {
            vm_set = malloc(sizeof(agent_vm_vif_id_t)*vnet_number);
            if (!vm_set) {
                return -1;
            }
            memset(vm_set, 0, sizeof(agent_vm_vif_id_t)*vnet_number);
            free_vm_set = vm_set;

            for (i = 0; i < vnet_number; i ++) {
                 vnet_query_info_t *pvm_query = &vnet_query_set[i];
                 /*vm/vgw's name label MUST not be overlapped*/
                 if (!strlen(pvm_query->vm_label)) {
                     continue;
                 }
                 strcpy(vm_set->vm_label, pvm_query->vm_label);
                 SNF_SYSLOG(LOG_INFO, "%s: vm name lable %s.\n",
                        __FUNCTION__, pvm_query->vm_label);
                 vm_set->vm_type = LC_VM_TYPE_GW;
                 vm_set->vm_id = pvm_query->vm_id;
                 vm_set->vdc_id = pvm_query->vdc_id;
                 vm_set->host_id = host_info->id;
                 vm_set->vm_res_type |= VM_RES_STATE | VM_RES_CPU_NUM | VM_RES_CPU_USAGE;
                 vm_attachs_vifs(vm_set, vintf_infos, nvif, host_info);
                 vm_set ++;
                 vnet_count ++;
            }
            log_agent_vm_vif_id_t(free_vm_set, vnet_count, __FUNCTION__);

            if (htype == LC_POOL_TYPE_XEN) {
                SNF_SYSLOG(LOG_INFO, "Will request %d vgws' usage from %s.\n",
                        vnet_number, server);
                lc_agent_request_vm_vif_usage(free_vm_set, vnet_count, server, timestamp);
            }
            if (free_vm_set) {
                free(free_vm_set);
            }
        }
    }
#endif

    return 0;
}

/* TODO: Implement the function for handling the request--kzs
*/
int hwdev_timer_handler(lcg_device_t *lcginfo)
{
    lcg_device_t *lcg_info = lcginfo;
    char *server = lcg_info->ip;
    int hwdev_number = 0;
    int i = 0;
    hwdev_query_info_t hwdev_query_set[MAX_HWDEV_PER_HOST];
    agent_hwdev_t *phwdev_query = NULL;


    SNF_SYSLOG(LOG_INFO, "%s: Will request hwdev's usage from %s.\n",
            __FUNCTION__, server);

    memset(hwdev_query_set, 0, sizeof(hwdev_query_info_t)*MAX_HWDEV_PER_HOST);

    lc_get_hwdev_by_server(hwdev_query_set, sizeof(hwdev_query_info_t), &hwdev_number, server, MAX_HWDEV_PER_HOST);
    SNF_SYSLOG(LOG_DEBUG, "%s: hwdev_number= %d.\n",__FUNCTION__,hwdev_number);
    if (hwdev_number)
    {
        SNF_SYSLOG(LOG_INFO, "%s: Will request %d hwdev's usage from %s.\n",
        __FUNCTION__, hwdev_number, server);
        phwdev_query = malloc(sizeof(agent_hwdev_t));
        for (i = 0; i < hwdev_number; i ++)
        {
            strcpy(phwdev_query->hwdev_ip, hwdev_query_set[i].hwdev_ip);
            strcpy(phwdev_query->community, hwdev_query_set[i].community);
            strcpy(phwdev_query->username, hwdev_query_set[i].username);
            strcpy(phwdev_query->password, hwdev_query_set[i].password);
            log_agent_hwdev_t(phwdev_query, __FUNCTION__);
            lc_agent_request_hwdev_usage(phwdev_query,server);
        }
    }

    return 0;
}

static void usage_timer_handler(int sig)
{
    int i = 0, ret = 0;
    uint32_t hostid_offset = 1;
    host_device_t host_infos[MAX_HOST_NUM];
    lcg_device_t lcg_infos[MAX_HOST_NUM];

    memset(host_infos, 0, sizeof(host_infos));
    memset(lcg_infos, 0, sizeof(lcg_infos));

    if (handle_lookup(pthread_self()) < 0 && lc_db_thread_init() < 0) {
        SNF_SYSLOG(LOG_ERR, "mysqldb thread init error");
        exit(1);
    }

    if (handle_master_lookup(pthread_self()) < 0 && lc_db_master_thread_init() < 0) {
        SNF_SYSLOG(LOG_ERR, "mysqldb master thread init error");
        exit(1);
    }

    if (mongo_handler_init() < 0) {
        SNF_SYSLOG(LOG_WARNING,
                "mongo_handler_init failed, another timer handler is "
                "still running now, terminate current timer handler.");
        return;
    }

    nsp_timer_handler(0);
    lb_timer_handler(0);
    switch_timer_handler(0);

    for (;;) {
        ret = lcs_usage_db_host_loadn(
                host_infos, hostid_offset, MAX_HOST_NUM);
        if (ret <= 0) {
            break;
        }
        if (ret > 0) {
            hostid_offset = host_infos[ret - 1].id + 1;
        }

        for (i = 0; i < ret; ++i) {
            if (host_infos[i].type == LC_SERVER_TYPE_NSP) {
                continue;
            }
            host_timer_handler(&host_infos[i]);
            vm_timer_handler(&host_infos[i]);
        }

#if 0
        /* TODO: Load the LCG host info from mysql db--kzs */
        lcgret = lcs_usage_db_lcg_loadn(lcg_infos, lcgid_offset, MAX_HOST_NUM);
        if (lcgret < 0)
        {
            ;
        }
        else
        {
            /* TODO: For each LCG host, find the info of the added hwdevs
             * in corresponding rack from the vm table of mysql db,
             * and send the request msg of these hwdevs to corresponding LCG host.--kzs */
            for (i = 0; i < lcgret; ++i)
            {
                hwdev_timer_handler(&lcg_infos[i]);
            }
        }
#endif

        if (ret < MAX_HOST_NUM) {
            break;
        }
    }

    mongo_handler_exit();
    return;
}

void list_vm_server_insert(void *msg)
{
    int i = 0;
    int rv;
    struct list_head *pos;
    vm_server_load_list_t *vs_list;
    vm_server_load_t *vm_server_load;

    if (mongo_handler_init() < 0) {
        SNF_SYSLOG(LOG_ERR, "mongo_handler_init error, LINE %d\n", __LINE__);
        exit(1);
    }

    vm_server_load = (vm_server_load_t *)malloc(sizeof(vm_server_load_t) * MONGO_VM_SERVER_BATCH_INSERT_NUM);
    memset(vm_server_load, 0, sizeof(vm_server_load_t) * MONGO_VM_SERVER_BATCH_INSERT_NUM);

    while(1){
        pthread_mutex_lock(&mutex_vmser);
        if(list_is_empty(&head_vm_server_load)){
            pthread_mutex_unlock(&mutex_vmser);

            if (i) {
                SNF_SYSLOG(LOG_INFO, "LINE %d: insert i: %d", __LINE__, i);
                if ((rv = mongo_db_entrys_batch_insert(
                                MONGO_APP_VM_SERVER_LOAD,
                                (void *)vm_server_load, i)) < 0 ) {
                    SNF_SYSLOG(LOG_ERR, "LINE %d: batch insert error: %d",
                            __LINE__, rv);
                }
                i = 0;
                memset(vm_server_load, 0,
                        sizeof(vm_server_load_t) * MONGO_VM_SERVER_BATCH_INSERT_NUM);
            }
            SNF_SYSLOG(LOG_INFO, "head_vm_server_load is empty");

            pthread_mutex_lock(&mutex_vmser);
            pthread_cond_wait(&cond_vmser, &mutex_vmser);
            pthread_mutex_unlock(&mutex_vmser);
            continue;

        } else {
            pos = head_vm_server_load.next;
            vs_list = container_of(pos, vm_server_load_list_t, list);
            list_del(pos);
            pthread_mutex_unlock(&mutex_vmser);

            memcpy(&vm_server_load[i], &vs_list->vm_server_load, sizeof(vm_server_load_t));
            i++;
            free(vs_list);
            if (i == 1) {
                sleep(1); // waiting for the producer
            }
        }

        if (i >= MONGO_VM_SERVER_BATCH_INSERT_NUM) {
            SNF_SYSLOG(LOG_INFO, "LINE %d: insert i: %d", __LINE__, i);
            if ((rv = mongo_db_entrys_batch_insert(
                            MONGO_APP_VM_SERVER_LOAD,
                            (void *)vm_server_load, i)) < 0 ) {
                SNF_SYSLOG(LOG_ERR, "LINE %d: batch insert error: %d",
                        __LINE__, rv);
            }
            i = 0;
            memset(vm_server_load, 0,
                    sizeof(vm_server_load_t) * MONGO_VM_SERVER_BATCH_INSERT_NUM);
        }
    }

    free(vm_server_load);
    mongo_handler_exit();
    return;
}

void list_vm_insert(void *msg)
{
    int i = 0;
    int rv;
    struct list_head *pos;
    vm_load_list_t *v_list;
    vm_load_t *vm_load;

    if (mongo_handler_init() < 0) {
        SNF_SYSLOG(LOG_ERR, "%s: mongo_handler_init error %d\n", __FUNCTION__, __LINE__);
        exit(1);
    }

    vm_load = (vm_load_t *)malloc(sizeof(vm_load_t) * MONGO_VM_BATCH_INSERT_NUM);
    memset(vm_load, 0, sizeof(vm_load_t) * MONGO_VM_BATCH_INSERT_NUM);

    while(1){
        pthread_mutex_lock(&mutex_vm);
        if (list_is_empty(&head_vm_load)) {
            pthread_mutex_unlock(&mutex_vm);

            if (i) {
                SNF_SYSLOG(LOG_INFO, "LINE %d: insert i: %d", __LINE__, i);
                if ((rv = mongo_db_entrys_batch_insert(
                                MONGO_APP_VM_LOAD, (void *)vm_load, i)) < 0 ) {
                    SNF_SYSLOG(LOG_ERR, "LINE %d: batch insert error: %d",
                            __LINE__, rv);
                }
                i = 0;
                memset(vm_load, 0, sizeof(vm_load_t) * MONGO_VM_BATCH_INSERT_NUM);
            }
            SNF_SYSLOG(LOG_INFO, "head_vm_load is empty");

            pthread_mutex_lock(&mutex_vm);
            pthread_cond_wait(&cond_vm, &mutex_vm);
            pthread_mutex_unlock(&mutex_vm);
            continue;

        } else {
            pos = head_vm_load.next;
            v_list = container_of(pos, vm_load_list_t, list);
            list_del(pos);
            pthread_mutex_unlock(&mutex_vm);

            memcpy(&vm_load[i], &v_list->vm_load, sizeof(vm_load_t));
            i++;
            free(v_list);
            if (i == 1) {
                sleep(1); // waiting for the producer
            }
        }

        if (i >= MONGO_VM_BATCH_INSERT_NUM) {
            SNF_SYSLOG(LOG_INFO, "LINE %d: insert i: %d", __LINE__, i);
            if ((rv = mongo_db_entrys_batch_insert(
                            MONGO_APP_VM_LOAD, (void *)vm_load, i)) < 0 ) {
                SNF_SYSLOG(LOG_ERR, "LINE %d: batch insert error: %d",
                        __LINE__, rv);
            }
            i = 0;
            memset(vm_load, 0, sizeof(vm_load_t) * MONGO_VM_BATCH_INSERT_NUM);
        }
    }

    free(vm_load);
    mongo_handler_exit();
    return;
}

int lc_monitor_get_host_resource_usage(monitor_host_t *phost, msg_host_info_response_t *pdata, int htype)
{
    host_t host_t_type;
    int hosttype;
    int mem_alloc = 0;
    int disk_alloc = 0;
    int disk_total = 0;
    char warning_email[LC_HOST_CPU_USAGE];
    char str[100];
    int cpu_allocated = 0;
    int return_val = 0;
    int i = 0, vm_number = 0, ii = 0, j;
    vm_query_info_t vm_query_set[MAX_VM_PER_HOST];
    vnet_query_info_t vnet_query_set[MAX_VM_PER_HOST];
    vm_server_load_list_t *vm_server_load_list;
    vmwaf_query_info_lcs_t vmwaf_info;
    sr_disk_info_t sr_disk_info;

    vm_server_load_list = (vm_server_load_list_t *)malloc(sizeof(vm_server_load_list_t));
    if (!vm_server_load_list){
        SNF_SYSLOG(LOG_ERR, " %s %d malloc error\n", __FUNCTION__, __LINE__);
        return -1;
    }

    memset(&sr_disk_info, 0, sizeof(sr_disk_info_t));
    memset(&vmwaf_info, 0, sizeof(vmwaf_query_info_lcs_t));
    memset(warning_email, 0, LC_HOST_CPU_USAGE);
    memset(str, 0, 100);
    memset(vm_query_set, 0, sizeof(vm_query_info_t)*MAX_VM_PER_HOST);
    memset(vnet_query_set, 0, sizeof(vnet_query_info_t)*MAX_VM_PER_HOST);
    memset(vm_server_load_list, 0, sizeof(vm_server_load_list_t));

    snprintf(warning_email, LC_HOST_CPU_USAGE, "Alert: Host %s ", phost->host_address);
    SNF_SYSLOG(LOG_INFO, "%s: Get host %s resource usage.\n", __FUNCTION__, phost->host_address);

    /*TODO: Unknown reason to cause it [Workaround]*/
    if (strstr(pdata->cpu_usage, "nan")) {
        SNF_SYSLOG(LOG_ERR, "%s: cpu_usage: %s", __FUNCTION__, pdata->cpu_usage);
        return return_val;
    }

    lc_get_vm_by_server(vm_query_set, sizeof(vm_query_info_t), &vm_number, phost->host_address, 1, MAX_VM_PER_HOST);
    if (vm_number) {
        for (i = 0; i < vm_number; i ++) {
             cpu_allocated += vm_query_set[i].cpu_number;
             mem_alloc += vm_query_set[i].mem_total;
        }
    }

    disk_total = 0;
    disk_alloc = 0;
    for(ii = 0;ii < MAX_SR_NUM;ii++){
        if(0 == pdata->sr_usage[i].is_shared){
            if(SR_DISK_TYPE_HA != pdata->sr_usage[i].type){
                disk_total += pdata->sr_usage[ii].disk_total;
                disk_alloc += pdata->sr_usage[ii].disk_used;
            }
        }
    }

    lc_vmdb_get_sr_disk_info(&sr_disk_info, phost->host_address);

    strcpy(phost->cpu_usage, pdata->cpu_usage);
    phost->mem_alloc = mem_alloc;
    phost->disk_total = disk_total;
    phost->disk_alloc = disk_alloc;
    phost->cpu_used   = cpu_allocated;

    lc_vmdb_host_device_load(&host_t_type, phost->host_address, "*");
    hosttype = host_t_type.host_role;
    if(hosttype == LC_HOST_ROLE_SERVER){
        strcpy(vm_server_load_list->vm_server_load.object.name, host_t_type.host_name);
        strcpy(vm_server_load_list->vm_server_load.object.server_ip, phost->host_address);
        vm_server_load_list->vm_server_load.object.htype = htype;
        vm_server_load_list->vm_server_load.timestamp = pdata->curr_time;
        strcpy(vm_server_load_list->vm_server_load.system.cpu_usage, pdata->cpu_usage);

        //dom0 mem disk
        vm_server_load_list->vm_server_load.system.dom0.mem_usage =  pdata->dom0_mem_usage;
        vm_server_load_list->vm_server_load.system.dom0.mem_total =  pdata->dom0_mem_total;
        vm_server_load_list->vm_server_load.system.dom0.disk_usage = pdata->dom0_disk_usage;
        vm_server_load_list->vm_server_load.system.dom0.disk_total = pdata->dom0_disk_total;

        //domu mem disk
        vm_server_load_list->vm_server_load.system.domu.mem_usage = 0; // FIXME: no use
        vm_server_load_list->vm_server_load.system.domu.mem_alloc = phost->mem_alloc;
        vm_server_load_list->vm_server_load.system.domu.mem_total = phost->mem_total;
        vm_server_load_list->vm_server_load.system.domu.disk_usage = 0; // FIXME: no use
        vm_server_load_list->vm_server_load.system.domu.disk_alloc = phost->disk_alloc;
        vm_server_load_list->vm_server_load.system.domu.disk_total = phost->disk_total;
        vm_server_load_list->vm_server_load.system.domu.io_read = pdata->io_read;
        vm_server_load_list->vm_server_load.system.domu.io_write = pdata->io_write;

        for (j = 0; j < LC_N_BR_ID; j++) {
            vm_server_load_list->vm_server_load.traffic[j].iftype = pdata->net_usage[j].iftype;
            vm_server_load_list->vm_server_load.traffic[j].rx_bps = pdata->net_usage[j].rx_bps;
            vm_server_load_list->vm_server_load.traffic[j].tx_bps = pdata->net_usage[j].tx_bps;
            vm_server_load_list->vm_server_load.traffic[j].rx_pps = pdata->net_usage[j].rx_pps;
            vm_server_load_list->vm_server_load.traffic[j].tx_pps = pdata->net_usage[j].tx_pps;
        }

#ifdef DEBUG_LCSNFD
        SNF_SYSLOG(LOG_DEBUG,
                   "%s: Insert vm server usage into mongo db ip:%s, name: %s"
                   "htype %d, "
                   "time %u, "
                   "cpu %s , "
                   "mem total %lf, "
                   "mem usage %lf, "
                   "mem alloc %lf, "
                   "disk total %lf "
                   "disk usage %lf "
                   "disk alloc %lf "
                   "io read: %lf "
                   "io write: %lf "
                   "dom0.mem_usage   %lf"
                   "dom0.mem_total   %lf"
                   "dom0.disk_usage  %lf"
                   "dom0.disk_total  %lf",
                   __FUNCTION__,
                   vm_server_load_list->vm_server_load.object.server_ip,
                   vm_server_load_list->vm_server_load.object.name,
                   vm_server_load_list->vm_server_load.object.htype,
                   vm_server_load_list->vm_server_load.timestamp,
                   vm_server_load_list->vm_server_load.system.cpu_usage,
                   vm_server_load_list->vm_server_load.system.domu.mem_total,
                   vm_server_load_list->vm_server_load.system.domu.mem_usage,
                   vm_server_load_list->vm_server_load.system.domu.mem_alloc,
                   vm_server_load_list->vm_server_load.system.domu.disk_total,
                   vm_server_load_list->vm_server_load.system.domu.disk_usage,
                   vm_server_load_list->vm_server_load.system.domu.disk_alloc,
                   vm_server_load_list->vm_server_load.system.domu.io_read,
                   vm_server_load_list->vm_server_load.system.domu.io_write,
                   vm_server_load_list->vm_server_load.system.dom0.mem_usage,
                   vm_server_load_list->vm_server_load.system.dom0.mem_total,
                   vm_server_load_list->vm_server_load.system.dom0.disk_usage,
                   vm_server_load_list->vm_server_load.system.dom0.disk_total
                   );
        for (j = 0; j < LC_N_BR_ID; j++) {
            if ( j == LC_ULNK_BR_ID ) {
                j++;
            }
            SNF_SYSLOG(LOG_DEBUG,
                   " %s [%d]: "
                   "iftype %"PRIu64" "
                   "rx_bps %"PRIu64" "
                   "tx_bps %"PRIu64" "
                   "rx_pps %"PRIu64" "
                   "tx_pps %"PRIu64" ",
                    __FUNCTION__, j,
                    vm_server_load_list->vm_server_load.traffic[j].iftype,
                    vm_server_load_list->vm_server_load.traffic[j].rx_bps,
                    vm_server_load_list->vm_server_load.traffic[j].tx_bps,
                    vm_server_load_list->vm_server_load.traffic[j].rx_pps,
                    vm_server_load_list->vm_server_load.traffic[j].tx_pps);
        }
#endif
        pthread_mutex_lock(&mutex_vmser);
        list_add_tail(&(vm_server_load_list->list), &head_vm_server_load);
        pthread_cond_signal(&cond_vmser);
        pthread_mutex_unlock(&mutex_vmser);
    }

    return return_val;
}

int log_msg_host_info_response_t(msg_host_info_response_t *msg, char* hostaddr, const char *fun_name)
{
#ifdef DEBUG_LCSNFD
    int i;
    SNF_SYSLOG(LOG_DEBUG,
               "%s: msg_host_info_response_t, %s, "
               "curr_time %u, "
               "memory_free %s, "
               "cpu_vendor %s, "
               "cpu_speed %s, "
               "cpu_modelname %s, "
               "cpu_count %u, "
               "cpu_usage %s, "
               "role %u, "
               "state %u, "
               "n_sr %u, "
               "sr_usage[0].uuid %s, "
               "sr_usage[0].disk_total %u, "
               "sr_usage[0].disk_used %u, "
               ,fun_name,
               hostaddr,
               msg->curr_time,
               msg->memory_free,
               msg->cpu_vendor,
               msg->cpu_speed,
               msg->cpu_modelname,
               msg->cpu_count,
               msg->cpu_usage,
               msg->role,
               msg->state,
               msg->n_sr,
               msg->sr_usage[0].uuid,
               msg->sr_usage[0].disk_total,
               msg->sr_usage[0].disk_used
               );
    for (i = 0; i < LC_N_BR_ID; i++) {
        if (i == LC_ULNK_BR_ID) {
            continue;
        }
        SNF_SYSLOG(LOG_DEBUG,
               "[%"PRIu64"]:"
               "net_usage.rx_bytes %"PRIu64", "
               "net_usage.rx_packets %"PRIu64", "
               "net_usage.rx_bps %"PRIu64", "
               "net_usage.rx_pps %"PRIu64", "
               "net_usage.tx_bytes %"PRIu64", "
               "net_usage.tx_packets %"PRIu64", "
               "net_usage.tx_bps %"PRIu64", "
               "net_usage.tx_pps %"PRIu64", "
               , msg->net_usage[i].iftype,
               msg->net_usage[i].rx_bytes,
               msg->net_usage[i].rx_packets,
               msg->net_usage[i].rx_bps,
               msg->net_usage[i].rx_pps,
               msg->net_usage[i].tx_bytes,
               msg->net_usage[i].tx_packets,
               msg->net_usage[i].tx_bps,
               msg->net_usage[i].tx_pps);
    }
#endif

    return 0;
}
int log_msg_hwdev_info_response_t(hwdev_result_t *msg, const char *fun_name)
{
    int i =0;
    SNF_SYSLOG(LOG_DEBUG,
               "%s: hwdev_result_t,"
               "curr_time %lu, "
               "hwdev_ip %s, "
               "mac %s, "
               "sys_uptime %s, "
               "sys_os %s, "
               "cpu_num %d, "
               "dsk_num %d, "
               "cpu_type %s, "
               "mem_data.size %d, "
               "mem_data.used %d, "
               "mem_data.usage %s, "
               ,fun_name,
               msg->curr_time,
               msg->hwdev_ip,
               msg->mac,
               msg->sys_uptime,
               msg->sys_os,
               msg->cpu_num,
               msg->dsk_num,
               msg->cpu_type,
               msg->mem_data.size,
               msg->mem_data.used,
               msg->mem_data.usage);

    for (i = 0; i < msg->cpu_num; i++)
    {
        SNF_SYSLOG(LOG_DEBUG,"%s:hwdev_result_t,cpu_data[%d]:%s",fun_name,i,msg->cpu_data[i].usage);
    }
    for (i = 0; i < msg->dsk_num; i++)
    {
        SNF_SYSLOG(LOG_DEBUG,"%s:hwdev_result_t,dsk_data[%d].size:%d,"
                    "dsk_data[%d].used:%d,"
                    "dsk_data[%d].usage:%s,"
                    "dsk_data[%d].type:%s,",
                    fun_name,
                    i,msg->dsk_data[i].size,
                    i,msg->dsk_data[i].used,
                    i,msg->dsk_data[i].usage,
                    i,msg->dsk_data[i].type);
    }

    return 0;
}

int lc_vmdb_get_sr_disk_info(sr_disk_info_t *sr_disk_info, char *ip)
{
    int i;
    int num;
    int host_num;
    int ret;
    db_storage_t storage;
    storage_connection_t *storage_connection;
    lc_vmdb_storage_connection_get_num_by_ip(&num, ip);

    SNF_SYSLOG(LOG_DEBUG, "%s: ip %s storage_connection num is:%d\n", __FUNCTION__, ip, num);

    storage_connection = malloc(sizeof(storage_connection_t) * num);
    memset(storage_connection, 0, sizeof(storage_connection_t) * num);
    ret = lc_vmdb_storage_connection_load_all(storage_connection, ip, &host_num);
    if (ret) {
        goto err;
    }
    for (i = 0; i < num; i++) {
        memset(&storage, 0, sizeof(storage_t));
        lc_vmdb_storage_load(&storage, storage_connection[i].storage_id);
        SNF_SYSLOG(LOG_DEBUG, "%s: storage id is:%d\n", \
                __FUNCTION__,storage_connection[i].storage_id);
        sr_disk_info->disk_usage += 0; // FIXME: deprecated
        sr_disk_info->disk_total += storage.disk_total;
        sr_disk_info->disk_alloc += storage.disk_used;
    }
    SNF_SYSLOG(LOG_DEBUG, "%s: disk usage:%d total :%d  alloc:%d\n", \
            __FUNCTION__, sr_disk_info->disk_usage,
            sr_disk_info->disk_total, sr_disk_info->disk_alloc);


err:
    free(storage_connection);
    return ret;
}

int lcs_host_response_parse(char *pdata,char *host_addr,int htype,int data_len)
{
    monitor_config_t mon_cfg;
    monitor_host_t mon_host;
    host_t host;
    msg_host_info_response_t *phost = NULL;
    char domain[LC_NAMESIZE];
    char mode[LC_NAMESIZE];
    char lcm_ip[LC_NAMESIZE];

    memset(&mon_cfg, 0, sizeof(monitor_config_t));
    memset(&mon_host, 0, sizeof(monitor_host_t));
    memset(&host, 0, sizeof(host_t));
    memset(domain, 0, sizeof(domain));
    memset(mode, 0, sizeof(mode));
    memset(lcm_ip, 0, sizeof(lcm_ip));

    lc_mon_get_config_info(&mon_cfg, &mon_host, 1);
    if (htype == LC_POOL_TYPE_XEN) {
        phost = (msg_host_info_response_t *)pdata;
        if (lc_get_compute_resource(&host, host_addr) != LC_DB_HOST_FOUND) {
            return -1;
        }
    }

    mon_host.mem_total = host.mem_total;
    mon_host.mem_alloc = host.mem_usage;
    mon_host.mem_usage = 0; // FIXME deprecated
    mon_host.disk_usage = 0; // FIXME deprecated
    mon_host.cr_id = host.cr_id;
    strcpy(mon_host.cpu_info, host.cpu_info);
    strcpy(mon_host.host_address, host.host_address);
    strncpy(mon_host.host_name, host.host_name, MAX_HOST_NAME_LEN);
    mon_host.pool_type = host.host_role;

    log_msg_host_info_response_t(phost, host.host_address, __FUNCTION__);
    lc_monitor_get_host_resource_usage(&mon_host, phost, htype);

    lc_vmdb_compute_resource_usage_update(mon_host.cpu_usage,
                                          mon_host.cpu_used,
                                          mon_host.mem_alloc,
                                         mon_host.disk_alloc,
                                      mon_host.ha_disk_usage,
                                      mon_host.host_address);
    return 0;
}

int lcs_hwdev_response_parse(char *pdata,int htype)
{
    hwdev_result_t *phwdev = NULL;
    hwdev_usage_t hwdevInfo;
    char diskinfobuffer[LC_DB_BUF_LEN];
    char cpuinfobuffer[LC_DB_BUF_LEN];
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];
    int i;


    memset(&hwdevInfo, 0, sizeof(hwdev_usage_t));
    memset(buffer, 0, LC_DB_BUF_LEN);
    memset(diskinfobuffer, 0, LC_DB_BUF_LEN);
    memset(cpuinfobuffer, 0, LC_DB_BUF_LEN);
    phwdev = (hwdev_result_t *)pdata;
    log_msg_hwdev_info_response_t(phwdev, __FUNCTION__);
    sprintf(diskinfobuffer,"[");
    for (i = 0; i < phwdev->dsk_num; i ++)
    {
    sprintf(buffer,"{\"name\":\"%d\",\"size\":\"%d\",\"used\":\"%d\",\"usage\":\"%s\",\"type\":\"%s\"}",
        i,phwdev->dsk_data[i].size,
        phwdev->dsk_data[i].used,
        phwdev->dsk_data[i].usage,
        phwdev->dsk_data[i].type);
    strcat(diskinfobuffer,buffer);
        if(i != (phwdev->dsk_num - 1))
        {
            strcat(diskinfobuffer,",");
        }
    memset(buffer,0,LC_DB_BUF_LEN);
    }
    strcat(diskinfobuffer,"]");
    SNF_SYSLOG(LOG_DEBUG, "%s: diskinfobuffer is:%s\n", __FUNCTION__,diskinfobuffer);
    for (i = 0; i < phwdev->cpu_num; i ++)
    {
    sprintf(buffer,"cpu%d_usage:%s,",
            i,phwdev->cpu_data[i].usage);
    strcat(cpuinfobuffer,buffer);
    memset(buffer,0,LC_DB_BUF_LEN);
    }
    SNF_SYSLOG(LOG_DEBUG, "%s: cpuinfobuffer is:%s\n", __FUNCTION__,cpuinfobuffer);

    SNF_SYSLOG(LOG_DEBUG, "%s: buffer is:%s\n", __FUNCTION__,buffer);

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ctrl_ip='%s'", phwdev->hwdev_ip);
    sprintf(buffer, "ctrl_mac=\'%s\', sys_os=\'%s\', mem_size=\'%d\', mem_used=\'%d\', mem_usage=\'%s\',"
            "cpu_type=\'%s\', cpu_num=\'%d\', dsk_num=\'%d\', disk_info=\'%s\',cpu_data=\'%s\'",
            phwdev->mac, phwdev->sys_os, phwdev->mem_data.size, phwdev->mem_data.used,phwdev->mem_data.usage,
            phwdev->cpu_type,phwdev->cpu_num,phwdev->dsk_num,diskinfobuffer,cpuinfobuffer);
    SNF_SYSLOG(LOG_DEBUG, "%s: buffer is:%s\r condition is:%s\n", __FUNCTION__,buffer,condition);


    if (lc_db_table_multi_update("third_party_device", buffer, condition) != 0) {
        SNF_SYSLOG(LOG_ERR, "%s: update third party hardware error\n", __FUNCTION__);
        return -1;
    }
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

int get_vm_actual_vdi_size(uint64_t *number, char *num)
{
    uint64_t size;
    size = strtoull(num, NULL, 0);
    //use KiB
    //*number = (size/LC_GB);
    *number = (size/LC_KB);
    return 0;
}

int lc_monitor_get_vm_res_usg_fields(
    vm_result_t *pvm, monitor_host_t *phost, vm_info_t *vm_info, int htype)
{
    uint32_t mem_total =0;
    uint32_t mem_free =0;
    uint64_t system_disk_usage = 0;
    uint64_t user_disk_usage = 0;
    double   sdisk_write_rate = 0.0;
    double   sdisk_read_rate = 0.0;
    double   udisk_write_rate = 0.0;
    double   udisk_read_rate = 0.0;
    int cpu_number = 0;
    char warning_email[LC_HOST_CPU_USAGE];
    host_t host;
    char *cpu_usage = NULL;
    double mem_usage = 0;
    int j;
    vm_load_list_t *vm_load_list;

    vm_load_list = (vm_load_list_t *)malloc(sizeof(vm_load_list_t));
    if (!vm_load_list){
        SNF_SYSLOG(LOG_ERR, "%d malloc error\n", __LINE__);
        return -1;
    }
    memset(vm_load_list, 0, sizeof(vm_load_list_t));

    cpu_usage = (char *)malloc(sizeof(char)*LC_HOST_CPU_USAGE);
    if (!cpu_usage) {
        SNF_SYSLOG(LOG_ERR, "%d malloc error\n", __LINE__);
        return -1;
    }
    memset(warning_email, 0, LC_HOST_CPU_USAGE);
    memset(&host, 0, sizeof(host_t));
    memset(cpu_usage, 0, sizeof(char)*LC_HOST_CPU_USAGE);

    snprintf(warning_email, LC_HOST_CPU_USAGE, "Alert: VM %s on Host %s: ",
             vm_info->vm_name, vm_info->host_name);
    SNF_SYSLOG(LOG_INFO,
            "Get the monitor result of VM %s (%s) on host %s\n",
            vm_info->vm_name, vm_info->vm_label, vm_info->host_name);
    if (htype == LC_POOL_TYPE_XEN) {
        get_vm_actual_memory(&mem_total, pvm->memory_total);
        get_vm_actual_memory(&mem_free, pvm->memory_free);
    }
    get_cpu_num(&cpu_number, pvm->cpu_number);
    get_vm_actual_vdi_size(&system_disk_usage, pvm->used_sys_disk);
    get_vm_actual_vdi_size(&user_disk_usage, pvm->used_user_disk);

    sscanf(pvm->sys_disk_write_rate, "%lf", &sdisk_write_rate);
    sscanf(pvm->sys_disk_read_rate, "%lf", &sdisk_read_rate);
    sscanf(pvm->user_disk_write_rate, "%lf", &udisk_write_rate);
    sscanf(pvm->user_disk_read_rate, "%lf", &udisk_read_rate);

    get_vm_cpu_usage(cpu_usage, pvm->cpu_usage);

    /*TODO: To ignore the special characters once reading it through
        * XAPI, the possible reason is the nonsense will be get through XAPI
        * while VM is booting but not be ready yet. [Workaround]*/
    if (strstr(cpu_usage,"nan")) {
        SNF_SYSLOG(LOG_ERR, "%s: cpu_usage: %s", __FUNCTION__, cpu_usage);
        free(cpu_usage);
        return 0;
    }

    if (lc_get_compute_resource(&host, vm_info->host_name) != LC_DB_HOST_FOUND) {
        if (cpu_usage) {
            free(cpu_usage);
        }
        return -1;
    }

    if(mem_total != 0){
        mem_usage = ((double)(mem_total - mem_free))/mem_total;
    }else{
        mem_usage = 0;
    }
    //kvm agent may get an empty memory_free from qga leading to 100
    //percent mem usage. Generally speaking, it's impossible to exhaust
    //all mem without leaving one byte.
    if (atoll(pvm->memory_free) == 0) {
        mem_usage = 0;
    }

    if (vm_info->vm_type == LC_VM_TYPE_VM) {
        vm_load_list->vm_load.object.vm_id = vm_info->vm_id;
        vm_load_list->vm_load.timestamp = pvm->curr_time;
        strcpy(vm_load_list->vm_load.system.cpu_usage, cpu_usage);
        vm_load_list->vm_load.system.mem_usage = mem_usage;
        vm_load_list->vm_load.system.sys_disk_total = vm_info->vdi_sys_size * 1024 * 1024;
        vm_load_list->vm_load.system.sys_disk_usage = system_disk_usage;
        vm_load_list->vm_load.system.user_disk_total = vm_info->vdi_user_size * 1024 * 1024;
        vm_load_list->vm_load.system.user_disk_usage = user_disk_usage;
        vm_load_list->vm_load.system.sys_disk_read_rate = sdisk_read_rate;
        vm_load_list->vm_load.system.sys_disk_write_rate = sdisk_write_rate;
        vm_load_list->vm_load.system.user_disk_read_rate = udisk_read_rate;
        vm_load_list->vm_load.system.user_disk_write_rate = udisk_write_rate;

        for (j = 0;j < MONGO_VM_VIF_NUM; j++) {
            vm_load_list->vm_load.vif_info[j].vif_id = pvm->vifs[j].vif_id;
            vm_load_list->vm_load.vif_info[j].traffic.rx_bps = pvm->vifs[j].rx_bps;
            vm_load_list->vm_load.vif_info[j].traffic.rx_pps = pvm->vifs[j].rx_pps;
            vm_load_list->vm_load.vif_info[j].traffic.tx_bps = pvm->vifs[j].tx_bps;
            vm_load_list->vm_load.vif_info[j].traffic.tx_pps = pvm->vifs[j].tx_pps;
        }

        vm_load_list->vm_load.disk_num = MIN(pvm->disk_num, MONGO_VM_DISK_NUM);
        for (j = 0; j < vm_load_list->vm_load.disk_num; ++j) {
            strncpy(vm_load_list->vm_load.disk_info[j].device,
                    pvm->disks[j].device, MONGO_NAME_LEN - 1);
            vm_load_list->vm_load.disk_info[j].io.read_rate =
                atof(pvm->disks[j].read_rate);
            vm_load_list->vm_load.disk_info[j].io.write_rate =
                atof(pvm->disks[j].write_rate);
        }

#ifdef DEBUG_LCSNFD
        SNF_SYSLOG(LOG_DEBUG,
                   "%s: time %u, Insert vm usage to mongo db.\n"
                   "vmid %d, "
                   "vm mem total %u "
                   "mem usage %lf "
                   "vm_load sys_disk_read_rate   %lf, "
                   "vm_load sys_disk_write_rate  %lf, "
                   "vm_load user_disk_read_rate  %lf, "
                   "vm_load user_disk_write_rate %lf, ",
                   __FUNCTION__,
                   vm_load_list->vm_load.timestamp,
                   vm_load_list->vm_load.object.vm_id,
                   mem_total,
                   mem_usage,
                   vm_load_list->vm_load.system.sys_disk_read_rate,
                   vm_load_list->vm_load.system.sys_disk_write_rate,
                   vm_load_list->vm_load.system.user_disk_read_rate,
                   vm_load_list->vm_load.system.user_disk_write_rate);
        for (j = 0; j < AGENT_VM_MAX_VIF; j++) {
            SNF_SYSLOG(LOG_DEBUG,
                       "%s: Insert vm usage to mongo db.\n"
                       "vifid[%d] %d, "
                       "vif[%d].rx_bps %"PRId64", "
                       "vif[%d].tx_bps %"PRId64", "
                       "vif[%d].rx_pps %"PRId64", "
                       "vif[%d].tx_pps %"PRId64", ",
                       __FUNCTION__,
                       j, vm_load_list->vm_load.vif_info[j].vif_id,
                       j, vm_load_list->vm_load.vif_info[j].traffic.rx_bps,
                       j, vm_load_list->vm_load.vif_info[j].traffic.tx_bps,
                       j, vm_load_list->vm_load.vif_info[j].traffic.rx_pps,
                       j, vm_load_list->vm_load.vif_info[j].traffic.tx_pps
                       );
        }
        for (j = 0; j < vm_load_list->vm_load.disk_num; ++j) {
            SNF_SYSLOG(LOG_DEBUG,
                       "%s: Insert vm usage to mongo db.\n"
                       "disk [%s], "
                       "read_rate [%f], "
                       "write_rate [%f]\n",
                       __FUNCTION__,
                       vm_load_list->vm_load.disk_info[j].device,
                       vm_load_list->vm_load.disk_info[j].io.read_rate,
                       vm_load_list->vm_load.disk_info[j].io.write_rate
                       );
        }
#endif

        pthread_mutex_lock(&mutex_vm);
        list_add_tail(&(vm_load_list->list), &head_vm_load);
        pthread_cond_signal(&cond_vm);
        pthread_mutex_unlock(&mutex_vm);
    }

    if (cpu_usage) {
        free(cpu_usage);
    }
    return 0;
}

const char *
lc_vm_state_to_string(int val)
{
    return vm_lookup_table[val];
}

const char *
lc_gw_state_to_string(int val)
{
    return gw_lookup_table[val];
}

const char *
lc_state_to_string(int val, int type)
{
    if (type == LC_VM_TYPE_VM) {
        return lc_vm_state_to_string(val);
    } else {
        return lc_gw_state_to_string(val);
    }
}

int lc_vcd_vm_stats_reply_msg(vcd_vm_stats_t *vm, int *count, void *buf, int len, int max)
{
    Cloudmessage__VMStatsResp *resp = cloudmessage__vmstats_resp__unpack(NULL, len, buf);
    int r, i;

    if (!resp) {
        return 1;
    }
    r = resp->result;
    if (!r) {
        *count = resp->n_vm_stats;
        if (resp->n_vm_stats >= max) {
            *count = max;
            resp->n_vm_stats = max;
        }
        for (i = 0; i < resp->n_vm_stats; ++i) {
            strncpy(vm[i].vm_name, resp->vm_stats[i]->vm_name,
                    MAX_VM_NAME_LEN - 1);

            switch (resp->vm_stats[i]->vm_state) {
            case CLOUDMESSAGE__VMSTATES__VM_STATS_ON:
                vm[i].vm_state = LC_VM_STATE_RUNNING;
                break;
            case CLOUDMESSAGE__VMSTATES__VM_STATS_OFF:
                vm[i].vm_state = LC_VM_STATE_STOPPED;
                break;
            case CLOUDMESSAGE__VMSTATES__VM_STATS_SUSPEND:
                vm[i].vm_state = LC_VM_STATE_SUSPENDED;
                break;
            default:
                break;
            }

            strncpy(vm[i].cpu_usage, resp->vm_stats[i]->cpu_usage,
                    MAX_VM_CPU_USAGE_LEN - 1);
        }
    }

    return r;
}

int log_vm_reply_info(vm_result_t* pvm, const char *fun_name)
{
    SNF_SYSLOG(LOG_DEBUG, "%s: Get vm reply info "\
               "namelabel=%s; "\
               "curr_time=%u; "\
               "state=%s; "\
               "cpu_number=%s; "\
               "cpu_usage=%s; "\
               "memory_total=%s; "\
               "used_sys_disk=%s; "\
               "sys_disk_write_rate=%s; "\
               "used_user_disk=%s; "\
               "user_disk_write_rate=%s; ", \
               fun_name,\
               pvm->vm_label,\
               pvm->curr_time,\
               pvm->state,\
               pvm->cpu_number,\
               pvm->cpu_usage,\
               pvm->memory_total,\
               pvm->used_sys_disk,\
               pvm->sys_disk_write_rate,\
               pvm->used_user_disk,\
               pvm->user_disk_write_rate\
               );
    return 0;
}

static int agent_data_msg_handler(u32 ori_time, int type, char *data, char *addr, int data_len)
{
    int i, r, nvm = 0;
    int nhwdev = 0;
    int htype;
    int struct_n = 0;
    msg_vm_info_response_t *vm_vif_info = NULL;
    vcd_vm_intf_stats_t vm_intf_stats[MAX_VM_PER_HOST];
    char host_addr[AGENT_IPV4_ADDR_LEN];
    msg_hwdev_info_response_t *phwdev_info = NULL;
    memset(vm_intf_stats, 0, sizeof(vm_intf_stats));

    switch (type) {

    case MSG_HOST_INFO_RESPONSE:
        SNF_SYSLOG(LOG_INFO, "MSG_HOST_INFO_RESPONSE from %s, datalen:%d, size:%ld",
                addr, data_len, sizeof(msg_host_info_response_t));
        if(data_len < sizeof(msg_host_info_response_t)){
            break;
        }
        htype = LC_POOL_TYPE_XEN;
        strncpy(host_addr, addr, AGENT_IPV4_ADDR_LEN);
        lcs_host_result_ntoh((msg_host_info_response_t*)data, ori_time);
        lcs_host_response_parse(data, host_addr, htype, data_len);
        break;
    /* TODO: Implement the lcs_hwdev_response_parse function--kzs*/
    case MSG_HWDEV_INFO_RESPONSE:
        htype = LC_POOL_TYPE_XEN;
        phwdev_info = (msg_hwdev_info_response_t *)data;
        nhwdev = ntohl(phwdev_info->n);
        SNF_SYSLOG(LOG_DEBUG, "MSG_HWDEV_INFO_RESPONSE from %s.", addr);

        struct_n = (data_len - sizeof(msg_hwdev_info_response_t)) / sizeof(hwdev_result_t);
        if(nhwdev > struct_n){
            nhwdev = struct_n;
        }
        for (i = 0; i < nhwdev; ++i) {
            lcs_hwdev_result_ntoh(phwdev_info->data + i);
            lcs_hwdev_response_parse((char *)phwdev_info->data + i, htype);
        }
        break;
    case MSG_VM_INFO_RESPONSE:
        htype = LC_POOL_TYPE_XEN;
        vm_vif_info = (msg_vm_info_response_t *)data;
        nvm = ntohl(vm_vif_info->n);
        struct_n = (data_len - sizeof(msg_vm_info_response_t)) / sizeof(vm_result_t);
        if(nvm > struct_n){
            nvm = struct_n;
        }
        SNF_SYSLOG(LOG_INFO, "MSG_VM_INFO_RESPONSE from %s, NUM:%d.", addr, nvm);
        for (i = 0; i < nvm; ++i) {
            lcs_vm_vif_result_ntoh(vm_vif_info->data + i, LCS_VIF_NUM_XEN, ori_time);
            lcs_vm_vif_response_parse(vm_vif_info->data + i, htype);
        }
       break;
    case LC_MSG_VCD_REPLY_VMD_HOST_STATS:
        SNF_SYSLOG(LOG_DEBUG, "LC_MSG_VCD_REPLY_VMD_HOST_STATS data_len %d",\
            data_len);
        htype = LC_POOL_TYPE_VMWARE;
        lcs_host_response_parse(data, host_addr, htype, data_len);
       break;
    case LC_MSG_VCD_REPLY_VMD_VM_VIF_USAGE:
        htype = LC_POOL_TYPE_VMWARE;
        SNF_SYSLOG(LOG_DEBUG, "LC_MSG_VCD_REPLY_VMD_VM_VIF_USAGE datalen %d",\
            data_len);

        vm_vif_info = (msg_vm_info_response_t *)malloc(\
            sizeof(msg_vm_info_response_t) + MAX_VM_PER_HOST * sizeof(vm_result_t));
        if(!vm_vif_info){
            SNF_SYSLOG(LOG_ERR, "Error malloc for vm_vif_info");
            return -1;
        }
        memset(vm_vif_info, 0, sizeof(vm_intf_stats));

        r = lc_vcd_vm_vif_reply_msg(vm_vif_info, data, data_len);
        if (r) {
            SNF_SYSLOG(LOG_ERR, "Get vm_vif reply info error");
            free(vm_vif_info);
            return -1;
        }
        for (i = 0; i < vm_vif_info->n; ++i) {
            lcs_vm_vif_response_parse(vm_vif_info->data + i, htype);
        }
        free(vm_vif_info);
        break;

    default:
        SNF_SYSLOG(LOG_ERR, "Error message type = %d", type);
        return -1;
    }

    return 0;
}

static int agent_data_msg_enque(const char *agent_ip, const char *buf, int size)
{
    lc_socket_queue_data_t *pdata = malloc(sizeof(lc_socket_queue_data_t));
    if (pdata == NULL) {
        SNF_SYSLOG(LOG_ERR, "malloc lc_socket_queue_data_t error: %s",
                strerror(errno));
        return 1;
    }

    assert(size <= sizeof(pdata->buf));
    snprintf(pdata->agent_ip, sizeof(pdata->agent_ip), "%s", agent_ip);
    memcpy(pdata->buf, buf, size + sizeof(lc_mbuf_hdr_t));
    pdata->size = size;

    pthread_mutex_lock(&lc_socket_queue_mutex);
    lc_socket_queue_eq(&lc_socket_queue, pdata);
    pthread_cond_signal(&lc_socket_queue_cond);
    pthread_mutex_unlock(&lc_socket_queue_mutex);

    return 0;
}

void set_nonblock(int socket) {
    int flags;
    flags = fcntl(socket,F_GETFL,0);
    assert(flags != -1);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);
}

static int agent_data_msg_recv(int fd)
{
    int sk_fd = -1, n = 0, i = 0;
    char *peer_ip = NULL;
    struct sockaddr_in sk_addr;
    char buf[MAX_BUFFER_LENGTH];
    lc_mbuf_hdr_t *hdr = NULL;
    socklen_t length = sizeof (sk_addr);
    int offset = 0;

    set_nonblock(fd);

    while (1) {
        sk_fd = accept(fd, (struct sockaddr *)&sk_addr, &length);
        if (sk_fd < 0) {
            if (i == 0) {
                SNF_SYSLOG(LOG_ERR, "error comes when call accept!%s",
                        strerror(errno));
                return -1;
            } else {
                break;
            }
        }
        ++i;

        peer_ip = inet_ntoa(sk_addr.sin_addr);
        SNF_SYSLOG(LOG_INFO, "%d, accept %s agent socket", i, peer_ip);

        offset = 0;
        memset(buf, 0, MAX_BUFFER_LENGTH);
        if ((n = recv(sk_fd, buf, MAX_BUFFER_LENGTH, 0)) > 0) {
            if(n >= sizeof(lc_mbuf_hdr_t)){
                hdr = (lc_mbuf_hdr_t *)buf;
                hdr->magic = ntohl(hdr->magic);
                hdr->direction = ntohl(hdr->direction);
                hdr->type = ntohl(hdr->type);
                hdr->length = ntohl(hdr->length);
                hdr->seq = ntohl(hdr->seq);
                hdr->time = ntohl(hdr->time);
                offset += n;
                if(hdr->magic == MSG_MG_AGN2SNF){
                    while(offset < hdr->length + sizeof(hdr)) {
                        if ((n = recv(sk_fd, buf + offset, MAX_BUFFER_LENGTH - offset, 0)) > 0) {
                            offset += n;
                        }else{
                            break;
                        }
                    }
                    offset -= sizeof(lc_mbuf_hdr_t);
                    if(offset != hdr->length){
                        SNF_SYSLOG(LOG_ERR, "Warning %s: offset=%d, hdr->length=%d",\
                            __FUNCTION__,offset, hdr->length);
                    }
                    SNF_SYSLOG(LOG_INFO,
                        "enque %s agent message magic=%x type=%d len=%d",
                        peer_ip, hdr->magic, hdr->type, hdr->length);
                    agent_data_msg_enque(peer_ip, buf, offset);
                }
            }
        }
        close(sk_fd);
    }

    return 0;
}

