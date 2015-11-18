#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <arpa/inet.h>
#include <assert.h>

#include "lc_db.h"
#include "lc_db_net.h"
#include "lc_db_log.h"
#include "lc_db_errno.h"
#include "lc_global.h"
#include "lc_kernel.h"
#include "lc_vnet.h"
#include "framework/types.h"
#include "framework/utils.h"
#include "message/msg.h"
#include "message/msg_ker2vmdrv.h"
#include "message/msg_vm.h"
#include "lc_netcomp.h"
#include "lc_agexec_msg.h"
#include "lc_livegate_api.h"
#include "vl2/ext_vl2_handler.h"
#include "vl2/ext_vl2_db.h"
#include "lc_lcmg_api.h"

#include "cloudmessage.pb-c.h"
#include "lc_vnet_worker.h"
#include "talker.pb-c.h"
#include "lc_header.h"
#include "lc_postman.h"

static int lc_host_join(struct lc_msg_head *m_head);
static int lc_host_eject(struct lc_msg_head *m_head);
static int lc_host_boot(struct lc_msg_head *m_head);
static int request_kernel_add_vm(struct lc_msg_head *m_head);
static int request_kernel_del_vm(struct lc_msg_head *m_head);
static int request_kernel_start_vm(struct lc_msg_head *m_head);
static int request_kernel_stop_vm(struct lc_msg_head *m_head);
static int request_kernel_change_vm(struct lc_msg_head *m_head);
static int request_kernel_isolate_vm(struct lc_msg_head *m_head);
static int request_kernel_release_vm(struct lc_msg_head *m_head);
static int request_kernel_migrate_vm(struct lc_msg_head *m_head);
static int request_kernel_replace_vm(struct lc_msg_head *m_head);

static int request_kernel_change_vgateway(struct lc_msg_head *m_head, bool send_response);
static int request_kernel_delete_vgateway(struct lc_msg_head *m_head, bool send_response);
static int request_kernel_change_valve(struct lc_msg_head *m_head, bool send_response);
static int request_kernel_delete_valve(struct lc_msg_head *m_head, bool send_response);
static int request_kernel_boot_vdevice(struct lc_msg_head *m_head);
static int request_kernel_down_vdevice(struct lc_msg_head *m_head);

static int request_kernel_down_nsp(struct lc_msg_head *m_head);

static int request_kernel_detach_interface(struct lc_msg_head *m_head);
int lc_torswitch_boot(struct lc_msg_head *m_head);
static int request_kernel_del_hwdevice(struct lc_msg_head *m_head);
static int request_kernel_vinterfaces_config(struct lc_msg_head *m_head);
int iprange_to_prefix(char *start, char *end, ip_prefix_t *prefixes);
static int nx_json_switch_conf_info(nx_json *confswitch,
                         char *operation,
                         int sw_port,
                         char *vlan_tag,
                         char *bandwidth,
                         char *trunk,
                         char *src_mac,
                         char *src_nets,
                         char *dst_nets
                         );
#define SWITCH_BOOT_SECONDS      10

#define NOTIFY_ONE_OBJ 1
int lc_network_notify(uint32_t id, int type, char *server)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__NotifyProactive *notify = NULL;
    Talker__NotifyBundleProactive notify_bundle = TALKER__NOTIFY_BUNDLE_PROACTIVE__INIT;
    int hdr_len = get_message_header_pb_len();
    int msg_len = 0, i = 0, count = NOTIFY_ONE_OBJ;
    char buf[MAX_MSG_BUFFER];

    memset(buf, 0, MAX_MSG_BUFFER);

    notify_bundle.n_bundle = count;
    notify_bundle.bundle =
            (Talker__NotifyProactive **)malloc(count * sizeof(Talker__NotifyProactive *));
    if (!notify_bundle.bundle) {
        return -1;
    }
    notify = (Talker__NotifyProactive *)malloc(count * sizeof(Talker__NotifyProactive));
    if (!notify) {
        free(notify_bundle.bundle);
        return -1;
    }
    memset(notify, 0, count * sizeof(Talker__NotifyProactive));
    for (i = 0; i < count; i ++) {
         talker__notify_proactive__init(&notify[i]);
         notify[i].id = id;
         notify[i].type = type;
         notify[i].launch_server = server;
         notify[i].has_id = 1;
         notify[i].has_type = 1;
         notify_bundle.bundle[i] = &notify[i];
    }

    msg.notify_bundle = &notify_bundle;
    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__LCPD;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    fill_message_header(&hdr, buf);

    free(notify);
    free(notify_bundle.bundle);
    lc_bus_lcpd_publish_unicast(buf, (hdr_len + msg_len), LC_BUS_QUEUE_TALKER);
    return 0;
}


static int lc_lcpd_vgateway_msg_reply(uint32_t id, uint32_t result,
                                      uint32_t seq, int ops)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__VgatewayOpsReply vgateway_ops = TALKER__VGATEWAY_OPS_REPLY__INIT;
    int hdr_len = get_message_header_pb_len();
    int msg_len;
    char buf[MAX_MSG_BUFFER];
    int res = 0;
    LCPD_LOG(LOG_DEBUG, "enter");
    memset(buf, 0, MAX_MSG_BUFFER);

    vgateway_ops.id = id;
    vgateway_ops.type = TALKER__VM_TYPE__VGATEWAY;
    vgateway_ops.result = result;
    vgateway_ops.ops = ops;
    vgateway_ops.has_id = 1;
    vgateway_ops.has_type = vgateway_ops.has_result = 1;
    vgateway_ops.has_ops = 1;

    msg.vgateway_ops_reply = &vgateway_ops;

    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__LCPD;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seq;
    fill_message_header(&hdr, buf);

    LCPD_LOG(LOG_DEBUG, "send message ");
    res = lc_bus_lcpd_publish_unicast(buf, (hdr_len + msg_len), LC_BUS_QUEUE_TALKER);
    if ( res != 0 ){
        LCPD_LOG( LOG_DEBUG, "send message fail, res is:%d", res );
    }
    return 0;
}

static int lc_lcpd_valve_msg_reply(uint32_t id, uint32_t result,
                                   uint32_t seq, int ops)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__ValveOpsReply valve_ops = TALKER__VALVE_OPS_REPLY__INIT;
    int hdr_len = get_message_header_pb_len();
    int msg_len;
    char buf[MAX_MSG_BUFFER];
    int res = 0;
    LCPD_LOG(LOG_DEBUG, "enter");
    memset(buf, 0, MAX_MSG_BUFFER);

    valve_ops.id = id;
    valve_ops.type = TALKER__VM_TYPE__VALVE;
    valve_ops.result = result;
    valve_ops.ops = ops;
    valve_ops.has_id = 1;
    valve_ops.has_type = valve_ops.has_result = 1;
    valve_ops.has_ops = 1;

    msg.valve_ops_reply = &valve_ops;

    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__LCPD;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seq;
    fill_message_header(&hdr, buf);

    LCPD_LOG(LOG_DEBUG, "send message ");
    res = lc_bus_lcpd_publish_unicast(buf, (hdr_len + msg_len), LC_BUS_QUEUE_TALKER);
    if ( res != 0 ){
        LCPD_LOG( LOG_DEBUG, "send message fail, res is:%d", res );
    }
    return 0;
}

static int lc_lcpd_vinterface_msg_reply(char *vinterface_ids, uint32_t result,
                                        uint32_t seq, int ops)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__InterfacesOpsReply interfaces_ops = TALKER__INTERFACES_OPS_REPLY__INIT;
    int hdr_len = get_message_header_pb_len();
    int msg_len;
    char buf[MAX_MSG_BUFFER];
    int res = 0;
    LCPD_LOG(LOG_DEBUG, "enter");
    memset(buf, 0, MAX_MSG_BUFFER);

    interfaces_ops.if_ids = vinterface_ids;
    interfaces_ops.result = result;
    interfaces_ops.ops = ops;
    interfaces_ops.has_result = 1;
    interfaces_ops.has_ops = 1;

    msg.interfaces_ops_reply = &interfaces_ops;

    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__LCPD;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seq;
    fill_message_header(&hdr, buf);

    LCPD_LOG(LOG_DEBUG, "send message ");
    res = lc_bus_lcpd_publish_unicast(buf, (hdr_len + msg_len), LC_BUS_QUEUE_TALKER);
    if ( res != 0 ){
        LCPD_LOG( LOG_DEBUG, "send message fail, res is:%d", res );
    }
    return 0;
}

static int lc_lcpd_host_msg_reply(char *host_ip, uint32_t result, uint32_t seq)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__HostOpsReply host_ops = TALKER__HOST_OPS_REPLY__INIT;
    int hdr_len = get_message_header_pb_len();
    int msg_len;
    char buf[MAX_MSG_BUFFER];
    int res = 0;
    LCPD_LOG(LOG_DEBUG, "enter");
    memset(buf, 0, MAX_MSG_BUFFER);

    host_ops.host_ip = host_ip;
    host_ops.result = result;
    host_ops.has_result = 1;

    msg.host_ops_reply = &host_ops;

    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__LCPD;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seq;
    fill_message_header(&hdr, buf);

    LCPD_LOG(LOG_DEBUG, "send message ");
    res = lc_bus_lcpd_publish_unicast(buf, (hdr_len + msg_len), LC_BUS_QUEUE_TALKER);
    if ( res != 0 ){
        LCPD_LOG( LOG_DEBUG, "send message fail, res is:%d", res );
    }
    return 0;
}

static int lc_lcpd_hwdev_msg_reply(uint32_t hwdev_id, uint32_t if_id,
    uint32_t ops, uint32_t err, uint32_t result, uint32_t seq)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__HWDevOpsReply hwdev_ops = TALKER__HWDEV_OPS_REPLY__INIT;
    int hdr_len = get_message_header_pb_len();
    int msg_len;
    char buf[MAX_MSG_BUFFER];
    int res = 0;
    LCPD_LOG(LOG_DEBUG, "enter");
    memset(buf, 0, MAX_MSG_BUFFER);

    hwdev_ops.has_hwdev_id = 1;
    hwdev_ops.hwdev_id = hwdev_id;
    hwdev_ops.has_if_id = 1;
    hwdev_ops.if_id = if_id;
    hwdev_ops.has_ops = 1;
    hwdev_ops.ops = ops;
    hwdev_ops.has_err = 1;
    hwdev_ops.err = err;
    hwdev_ops.has_result = 1;
    hwdev_ops.result = result;

    msg.hwdev_ops_reply = &hwdev_ops;

    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__LCPD;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seq;
    fill_message_header(&hdr, buf);

    LCPD_LOG(LOG_DEBUG, "send message ");
    res = lc_bus_lcpd_publish_unicast(buf, (hdr_len + msg_len), LC_BUS_QUEUE_TALKER);
    if ( res != 0 ){
        LCPD_LOG( LOG_DEBUG, "send message fail, res is:%d", res );
    }
    return 0;
}

static int lc_convert_protobuf_msg(struct lc_msg_head *m_head)
{
    Talker__Message *msg = NULL;
    int ret = 0;
    struct msg_vm_opt_start *mvos = NULL;
    struct msg_vm_opt vmops;
    struct msg_torswitch_opt *tor = NULL;
    struct msg_vm_opt *mvm = NULL;
    struct msg_vgateway_opt *vgateway_opt;
    struct msg_valve_opt *valve_opt;
    struct msg_vl2_opt *vl2 = NULL;
    struct msg_hwdev_opt *mhw = NULL;
    struct msg_hwdev_opt_start *mhw_start = NULL;
    struct msg_vinterfaces_opt *mvifs = NULL;
    struct msg_host_ip_opt *mhost = NULL;
    struct msg_vm_migrate_opt *vm_migrate = NULL;
    msg = talker__message__unpack(NULL, m_head->length,
                              (uint8_t *)(m_head + 1));

    if (!msg) {
        return -1;
    }
    memset(&vmops, 0, sizeof(struct msg_vm_opt));

    if (msg->vm_isolate_req) {
        m_head->type = LC_MSG_VM_ISOLATE;
        vmops.vm_id = msg->vm_isolate_req->id;
        vmops.vnet_id = msg->vm_isolate_req->vnet_id;
        vmops.vl2_id = msg->vm_isolate_req->vl2_id;
    } else if (msg->vm_release_req) {
        m_head->type = LC_MSG_VM_RELEASE;
        vmops.vm_id = msg->vm_release_req->id;
        vmops.vnet_id = msg->vm_release_req->vnet_id;
        vmops.vl2_id = msg->vm_release_req->vl2_id;
    } else if (msg->vm_delete_req) {
        m_head->type = LC_MSG_VM_DEL;
        vmops.vm_id = msg->vm_delete_req->id;
        vmops.vnet_id = msg->vm_delete_req->vnet_id;
        vmops.vl2_id = msg->vm_delete_req->vl2_id;
    } else if (msg->vm_migrate_req) {
        LCPD_LOG(LOG_DEBUG, "vm migrate magic=%x type=%d in\n",
                 m_head->magic, m_head->type);
        m_head->type = LC_MSG_VM_MIGRATE;
        vm_migrate = (struct msg_vm_migrate_opt *) (m_head + 1);
        memset((uint8_t *)(m_head + 1), 0, m_head->length);
        vm_migrate->vm_id = msg->vm_migrate_req->vm_id;
        snprintf(vm_migrate->des_server, MAX_HOST_ADDRESS_LEN,
                 msg->vm_migrate_req->launch_server);
        m_head->length = sizeof(struct msg_vm_migrate_opt);
        talker__message__free_unpacked(msg, NULL);
        LCPD_LOG(LOG_DEBUG, "vm migrate vm_migrate.vm_id is: %d "
                 "des_server is: %s.",
                 vm_migrate->vm_id, vm_migrate->des_server);
        return ret;
    } else if (msg->vgateway_modify_req) {
        LCPD_LOG(LOG_DEBUG, "vgateway modify magic=%x type=%d in\n",
                 m_head->magic, m_head->type);
        m_head->type = LC_MSG_VGATEWAY_CHG;
        vgateway_opt = (struct msg_vgateway_opt *) (m_head + 1);
        memset((uint8_t *)(m_head + 1), 0, m_head->length);
        snprintf(vgateway_opt->wans, LC_VGATEWAY_IDS_LEN,
                msg->vgateway_modify_req->wanslist);
        snprintf(vgateway_opt->lans, LC_VGATEWAY_IDS_LEN,
                msg->vgateway_modify_req->lanslist);
        vgateway_opt->vnet_id = msg->vgateway_modify_req->id;
        m_head->length = sizeof(struct msg_vgateway_opt);
        talker__message__free_unpacked(msg, NULL);
        LCPD_LOG(LOG_DEBUG, "vgateway modify vgatewayops.vnet_id is:%d",
                 vgateway_opt->vnet_id)
        return ret;
    } else if (msg->vgateway_migrate_req) {
        LCPD_LOG(LOG_DEBUG, "vgateway migrate magic=%x type=%d in\n",
                 m_head->magic, m_head->type);
        m_head->type = LC_MSG_VGATEWAY_MIGRATE;
        vgateway_opt = (struct msg_vgateway_opt *) (m_head + 1);
        memset((uint8_t *)(m_head + 1), 0, m_head->length);
        vgateway_opt->vnet_id = msg->vgateway_migrate_req->id;
        snprintf(vgateway_opt->gw_launch_server, MAX_HOST_ADDRESS_LEN,
                 msg->vgateway_migrate_req->gw_launch_server);
        m_head->length = sizeof(struct msg_vgateway_opt);
        talker__message__free_unpacked(msg, NULL);
        LCPD_LOG(LOG_DEBUG, "vgateway migrate vgatewayops.vnet_id is: %d "
                "gw launch server is: %s.",
                 vgateway_opt->vnet_id, vgateway_opt->gw_launch_server)
        return ret;
    } else if (msg->vgateway_del_req) {
        LCPD_LOG(LOG_DEBUG, "vgateway delete magic=%x type=%d in\n",
                 m_head->magic, m_head->type);
        m_head->type = LC_MSG_VGATEWAY_DEL;
        vgateway_opt = (struct msg_vgateway_opt *) (m_head + 1);
        vgateway_opt->vnet_id = msg->vgateway_del_req->id;
        m_head->length = sizeof(struct msg_vgateway_opt);
        talker__message__free_unpacked(msg, NULL);
        LCPD_LOG(LOG_DEBUG, "vgateway delete vgatewayops.vnet_id is:%d",
                 vgateway_opt->vnet_id)
        return ret;
    } else if (msg->valve_modify_req) {
        LCPD_LOG(LOG_DEBUG, "valve modify magic=%x type=%d in\n",
                 m_head->magic, m_head->type);
        m_head->type = LC_MSG_VALVE_CHG;
        valve_opt = (struct msg_valve_opt *) (m_head + 1);
        memset((uint8_t *)(m_head + 1), 0, m_head->length);
        snprintf(valve_opt->wans, LC_VALVE_IDS_LEN,
                msg->valve_modify_req->wanslist);
        snprintf(valve_opt->lans, LC_VALVE_IDS_LEN,
                msg->valve_modify_req->lanslist);
        valve_opt->vnet_id = msg->valve_modify_req->id;
        m_head->length = sizeof(struct msg_valve_opt);
        talker__message__free_unpacked(msg, NULL);
        LCPD_LOG(LOG_DEBUG, "valve modify valveops.vnet_id is:%d",
                 valve_opt->vnet_id)
        return ret;
    } else if (msg->valve_migrate_req) {
        LCPD_LOG(LOG_DEBUG, "valve migrate magic=%x type=%d in\n",
                 m_head->magic, m_head->type);
        m_head->type = LC_MSG_VALVE_MIGRATE;
        valve_opt = (struct msg_valve_opt *) (m_head + 1);
        memset((uint8_t *)(m_head + 1), 0, m_head->length);
        valve_opt->vnet_id = msg->valve_migrate_req->id;
        snprintf(valve_opt->gw_launch_server, MAX_HOST_ADDRESS_LEN,
                 msg->valve_migrate_req->gw_launch_server);
        m_head->length = sizeof(struct msg_valve_opt);
        talker__message__free_unpacked(msg, NULL);
        LCPD_LOG(LOG_DEBUG, "valve migrate valveops.vnet_id is: %d "
                "gw launch server is: %s.",
                 valve_opt->vnet_id, valve_opt->gw_launch_server)
        return ret;
    } else if (msg->valve_del_req) {
        LCPD_LOG(LOG_DEBUG, "valve delete magic=%x type=%d in\n",
                 m_head->magic, m_head->type);
        m_head->type = LC_MSG_VALVE_DEL;
        valve_opt = (struct msg_valve_opt *) (m_head + 1);
        valve_opt->vnet_id = msg->valve_del_req->id;
        m_head->length = sizeof(struct msg_valve_opt);
        talker__message__free_unpacked(msg, NULL);
        LCPD_LOG(LOG_DEBUG, "valve delete valveops.vnet_id is:%d",
                 valve_opt->vnet_id)
        return ret;
    } else if (msg->vm_start_req) {
        /* this is for KVM VM */
        m_head->type = LC_MSG_VM_START;
        vmops.vm_id = msg->vm_start_req->id;
    } else if (msg->vm_stop_req) {
        m_head->type = LC_MSG_VM_STOP;
        vmops.vm_id = msg->vm_stop_req->id;
        vmops.vnet_id = msg->vm_stop_req->vnet_id;
        vmops.vl2_id = msg->vm_stop_req->vl2_id;
    } else if (msg->vm_replace_req) {
        m_head->type = LC_MSG_VM_REPLACE;
        m_head->magic = MSG_MG_UI2KER;
        vmops.vm_id = msg->vm_replace_req->id;
        vmops.vnet_id = msg->vm_replace_req->vnet_id;
        vmops.vl2_id = msg->vm_replace_req->vl2_id;
    } else if (msg->vl2_add_req) {
        m_head->type = LC_MSG_VL2_ADD;
        vl2 = (struct msg_vl2_opt *)(m_head + 1);
        memset((uint8_t *)(m_head + 1), 0, m_head->length);
        vl2->vnet_id = msg->vl2_add_req->vdc_id;
        vl2->vl2_id = msg->vl2_add_req->vl2_id;
        m_head->length = sizeof(struct msg_vl2_opt);
        talker__message__free_unpacked(msg, NULL);
        return ret;
    } else if (msg->vl2_del_req) {
        m_head->type = LC_MSG_VL2_DEL;
        vl2 = (struct msg_vl2_opt *)(m_head + 1);
        memset((uint8_t *)(m_head + 1), 0, m_head->length);
        vl2->vnet_id = msg->vl2_del_req->vdc_id;
        vl2->vl2_id = msg->vl2_del_req->vl2_id;
        m_head->length = sizeof(struct msg_vl2_opt);
        talker__message__free_unpacked(msg, NULL);
        return ret;
    } else if (msg->vl2_modify_req) {
        m_head->type = LC_MSG_VL2_CHANGE;
        vl2 = (struct msg_vl2_opt *)(m_head + 1);
        memset((uint8_t *)(m_head + 1), 0, m_head->length);
        vl2->vl2_id = msg->vl2_modify_req->vl2_id;
        vl2->rack_id = msg->vl2_modify_req->rack_id;
        vl2->vlantag = msg->vl2_modify_req->vlantag;
        m_head->length = sizeof(struct msg_vl2_opt);
        talker__message__free_unpacked(msg, NULL);
        return ret;
    } else if (msg->torswitch_modify_req) {
        m_head->type = LC_MSG_TORSWITCH_MODIFY;
        tor = (struct msg_torswitch_opt *) (m_head + 1);
        memset((uint8_t *)(m_head + 1), 0, m_head->length);
        snprintf(tor->ip, VL2_MAX_INTERF_NW_LEN, msg->torswitch_modify_req->ip);
        tor->state = msg->torswitch_modify_req->state;
        m_head->length = sizeof(struct msg_torswitch_opt);
        talker__message__free_unpacked(msg, NULL);
        return ret;
    } else if (msg->interface_attach_req) {
        m_head->type = LC_MSG_VINTERFACE_ATTACH;
        mvm = (struct msg_vm_opt *)(m_head + 1);
        mvos = (struct msg_vm_opt_start *)(mvm + 1);
        memset((uint8_t *)(m_head + 1), 0, m_head->length);
        mvm->vm_id = msg->interface_attach_req->id;
        mvos->vm_ifaces[0].if_id = msg->interface_attach_req->if_id;
        mvos->vm_ifaces[0].if_index = msg->interface_attach_req->if_index;
        mvos->vm_ifaces[0].if_subnetid =
            msg->interface_attach_req->if_subnetid;
        mvos->vm_ifaces[0].if_vlan = msg->interface_attach_req->if_vlantag;
        mvos->vm_ifaces[0].if_bandw = msg->interface_attach_req->if_bandwidth;
        strncpy(mvos->vm_server, msg->interface_attach_req->launch_server,
                MAX_HOST_ADDRESS_LEN - 1);
        m_head->length = sizeof(struct msg_vm_opt);
        m_head->length += sizeof(struct msg_vm_opt_start);
        talker__message__free_unpacked(msg, NULL);
        return ret;
    } else if (msg->interface_detach_req) {
        m_head->type = LC_MSG_VINTERFACE_DETACH;
        mvm = (struct msg_vm_opt *)(m_head + 1);
        mvos = (struct msg_vm_opt_start *)(mvm + 1);
        memset((uint8_t *)(m_head + 1), 0, m_head->length);
        mvm->vm_id = msg->interface_detach_req->id;
        mvos->vm_ifaces[0].if_id = msg->interface_detach_req->if_id;
        mvos->vm_ifaces[0].if_index = msg->interface_detach_req->if_index;
        mvos->vm_ifaces[0].if_subnetid =
            msg->interface_detach_req->if_subnetid;
        strncpy(mvos->vm_server, msg->interface_detach_req->launch_server,
                MAX_HOST_ADDRESS_LEN - 1);
        m_head->length = sizeof(struct msg_vm_opt);
        m_head->length += sizeof(struct msg_vm_opt_start);
        talker__message__free_unpacked(msg, NULL);
        return ret;
    } else if (msg->interfaces_config_req) {
        m_head->type = LC_MSG_VINTERFACES_CONFIG;
        mvifs = (struct msg_vinterfaces_opt *)(m_head + 1);
        memset((uint8_t *)(m_head + 1), 0, m_head->length);
        strncpy(mvifs->vinterface_ids, msg->interfaces_config_req->if_ids, MAX_VIF_IDS_LEN);
        m_head->length = sizeof(struct msg_vinterfaces_opt);
        talker__message__free_unpacked(msg, NULL);
        return ret;
    } else if (msg->hwdev_interface_attach_req) {
        m_head->type = LC_MSG_HWDEV_IF_ATTACH;
        mhw = (struct msg_hwdev_opt*)(m_head + 1);
        mhw_start = (struct msg_hwdev_opt_start *)(mhw + 1);
        memset((uint8_t *)(m_head + 1), 0, m_head->length);
        mhw->vnet_id = msg->hwdev_interface_attach_req->vnet_id;
        mhw->vl2_id = msg->hwdev_interface_attach_req->if_subnetid;
        mhw->vl2_type = msg->hwdev_interface_attach_req->subnet_type;
        mhw->hwdev_id = msg->hwdev_interface_attach_req->hwdev_id;
        mhw_start->switch_id = msg->hwdev_interface_attach_req->switch_id;
        mhw_start->switch_port = msg->hwdev_interface_attach_req->switch_port;
        mhw_start->hwdev_iface.if_id = msg->hwdev_interface_attach_req->if_id;
        mhw_start->hwdev_iface.if_bandw = msg->hwdev_interface_attach_req->if_bandwidth;
        mhw_start->hwdev_iface.if_index = msg->hwdev_interface_attach_req->if_index;
        m_head->length = sizeof(struct msg_hwdev_opt);
        m_head->length += sizeof(struct msg_hwdev_opt_start);
        talker__message__free_unpacked(msg, NULL);
        return ret;
    } else if (msg->hwdev_interface_detach_req) {
        m_head->type = LC_MSG_HWDEV_IF_DETACH;
        mhw = (struct msg_hwdev_opt*)(m_head + 1);
        mhw_start = (struct msg_hwdev_opt_start *)(mhw + 1);
        memset((uint8_t *)(m_head + 1), 0, m_head->length);
        mhw->vnet_id = msg->hwdev_interface_detach_req->vnet_id;
        mhw->vl2_id = msg->hwdev_interface_detach_req->if_subnetid;
        mhw->vl2_type = msg->hwdev_interface_detach_req->subnet_type;
        mhw->hwdev_id = msg->hwdev_interface_detach_req->hwdev_id;
        mhw_start->switch_id = msg->hwdev_interface_detach_req->switch_id;
        mhw_start->switch_port = msg->hwdev_interface_detach_req->switch_port;
        mhw_start->hwdev_iface.if_id = msg->hwdev_interface_detach_req->if_id;
        mhw_start->hwdev_iface.if_index = msg->hwdev_interface_detach_req->if_index;
        m_head->length = sizeof(struct msg_hwdev_opt);
        m_head->length += sizeof(struct msg_hwdev_opt_start);
        talker__message__free_unpacked(msg, NULL);
        return ret;
    } else if (msg->host_boot_req) {
        m_head->type = LC_MSG_HOST_BOOT;
        mhost = (struct msg_host_ip_opt *)(m_head + 1);
        memset((uint8_t *)(m_head + 1), 0, m_head->length);
        strncpy(mhost->host_ip, msg->host_boot_req->host_ip, MAX_HOST_ADDRESS_LEN);
        m_head->length = sizeof(struct msg_host_ip_opt);
        talker__message__free_unpacked(msg, NULL);
        return ret;
    } else if (msg->vm_add_req) {
        m_head->type = LC_MSG_VM_ADD;
        mvm = (struct msg_vm_opt *)(m_head + 1);
        memset((uint8_t *)(m_head + 1), 0, m_head->length);
        mvm->vm_id = atoi(msg->vm_add_req->vm_ids);
        m_head->length = sizeof(struct msg_vm_opt);
        talker__message__free_unpacked(msg, NULL);
        return ret;
    } else {
        return -1;
    }
    mvm = (struct msg_vm_opt *) (m_head + 1);
    /*make sure it's safe*/
    talker__message__free_unpacked(msg, NULL);
    memset((uint8_t *)(m_head + 1), 0, m_head->length);
    mvm->vm_id = vmops.vm_id;
    mvm->vnet_id = vmops.vnet_id;
    mvm->vl2_id = vmops.vl2_id;
    m_head->length = sizeof(struct msg_vm_opt);
    return ret;
}

int kernel_msg_handler(struct lc_msg_head *m_head)
{
    int ret;

    if (!is_lc_msg(m_head)) {
        return -1;
    }

    if (!is_msg_to_ker(m_head)) {
        return -1;
    }

    if (m_head->type == LC_MSG_NOTHING) {
        lc_convert_protobuf_msg(m_head);
    }
    LCPD_LOG(LOG_INFO, "magic=%x type=%d in\n", m_head->magic, m_head->type);
    switch (m_head->type) {
    case LC_MSG_HOST_JOIN:
        ret = lc_host_join(m_head);
        return ret;

    case LC_MSG_HOST_PIF_CONFIG:
        LCPD_LOG(LOG_ERR, "switchtype OPENFLOW is deprecated");
        assert(0);
        return _VL2_FALSE_;
    case LC_MSG_OFS_PORT_CONFIG:
        LCPD_LOG(LOG_ERR, "switchtype OPENFLOW is deprecated");
        assert(0);
        return _VL2_FALSE_;

    case LC_MSG_HOST_EJECT:
        ret = lc_host_eject(m_head);
        return ret;

    case LC_MSG_HOST_BOOT:
        ret = lc_host_boot(m_head);
        return ret;

    case LC_MSG_VM_ADD:
        ret = request_kernel_add_vm(m_head);
        return ret;

    case LC_MSG_VM_DEL:
        ret = request_kernel_del_vm(m_head);
        return ret;

    case LC_MSG_VM_START:
        ret = request_kernel_start_vm(m_head);
        return ret;

    case LC_MSG_VM_STOP:
        ret = request_kernel_stop_vm(m_head);
        return ret;

    case LC_MSG_VM_CHG:
        ret = request_kernel_change_vm(m_head);
        return ret;

    case LC_MSG_VM_ISOLATE:
        ret = request_kernel_isolate_vm(m_head);
        return ret;

    case LC_MSG_VM_RELEASE:
        ret = request_kernel_release_vm(m_head);
        return ret;

    case LC_MSG_VM_MIGRATE:
        ret = request_kernel_migrate_vm(m_head);
        return ret;

    case LC_MSG_VM_REPLACE:
        ret = request_kernel_replace_vm(m_head);
        return ret;

    case LC_MSG_VGATEWAY_CHG:
        ret = request_kernel_change_vgateway(m_head, true);
        return ret;

    case LC_MSG_VGATEWAY_MIGRATE:
        ret = request_kernel_migrate_vgateway(m_head);
        return ret;

    case LC_MSG_VGATEWAY_DEL:
        ret = request_kernel_delete_vgateway(m_head, true);
        return ret;

    case LC_MSG_VALVE_CHG:
        ret = request_kernel_change_valve(m_head, true);
        return ret;

    case LC_MSG_VALVE_MIGRATE:
        ret = request_kernel_migrate_valve(m_head);
        return ret;

    case LC_MSG_VALVE_DEL:
        ret = request_kernel_delete_valve(m_head, true);
        return ret;

    case LC_MSG_VDEVICE_BOOT:
        ret = request_kernel_boot_vdevice(m_head);
        return ret;

    case LC_MSG_VDEVICE_DOWN:
        ret = request_kernel_down_vdevice(m_head);
        return ret;

    case LC_MSG_NSP_DOWN:
        ret = request_kernel_down_nsp(m_head);
        return ret;

    case LC_MSG_VL2_ADD:
    case LC_MSG_VL2_DEL:
    case LC_MSG_VL2_CHANGE:
        return application_msg_handler(m_head);

    case LC_MSG_TORSWITCH_MODIFY:
        return lc_torswitch_boot(m_head);

    case LC_MSG_VINTERFACE_ATTACH:
        return request_kernel_attach_interface(m_head, 0);

    case LC_MSG_VINTERFACE_DETACH:
        return request_kernel_detach_interface(m_head);

    case LC_MSG_VINTERFACES_CONFIG:
        return request_kernel_vinterfaces_config(m_head);

    case LC_MSG_HWDEV_IF_ATTACH:
        return request_kernel_add_hwdevice(m_head);

    case LC_MSG_HWDEV_IF_DETACH:
        return request_kernel_del_hwdevice(m_head);

    case LC_MSG_DUMMY:
    default:
        return 0;
    }
}

static int lc_msg_forward(lc_mbuf_t *msg)
{
    int magic = lc_msg_get_path(msg->hdr.type, msg->hdr.magic);

    if (magic == MSG_MG_END || msg->hdr.magic == MSG_MG_END) {
        LCPD_LOG(LOG_DEBUG, "The message %d complete\n",msg->hdr.type);
        return 0;
    } else if (magic == -1) {
        LCPD_LOG(LOG_ERR, "Find next module from %x error\n", msg->hdr.magic);
        return -1;
    } else {
        LCPD_LOG(LOG_DEBUG, "The message %d forwarding\n", msg->hdr.type);
        msg->hdr.magic = magic;
        lc_publish_msg(msg);
        return 0;
    }
}

static int lc_msg_rollback(lc_mbuf_t *msg)
{
    int magics[MAX_PATH_LEN];
    int n = lc_msg_get_rollback_destination(magics, msg->hdr.type, msg->hdr.magic);
    int i;

    if (n == 0) {
        LCPD_LOG(LOG_DEBUG, "no rollback\n");
        return 0;
    } else {
        msg->hdr.type = lc_msg_get_rollback_type(msg->hdr.type);
        for (i = 0; i < n; i++) { // skipping lcm here
            LCPD_LOG(LOG_DEBUG, "rollback msg to %x\n",magics[i]);
            msg->hdr.magic = magics[i];
            lc_publish_msg(msg);
        }
        return 0;
    }
}

static int parse_json_msg_vif_config_result(char *msg, char *buf)
{
    const nx_json *root = NULL;
    const nx_json *data = NULL;
    const nx_json *vif = NULL;
    const nx_json *vif_id = NULL;
    int len, i;

    root = nx_json_parse(msg, NULL);
    if (NULL == root) {
        LCPD_LOG(LOG_ERR, "invalid msg recvived.");
        return -1;
    }

    data = nx_json_get(root, LCPD_JSON_DATA_KEY);
    if (data->type == NX_JSON_NULL) {
        LCPD_LOG(LOG_ERR, "invalid json recvived, no data.");
        return -1;
    }

    snprintf(buf, LG_API_URL_SIZE, "(-1");
    len = strlen(buf);
    i = 0;
    do {
        vif = nx_json_item(data, i);
        if (vif->type == NX_JSON_NULL) {
            break;
        }

        vif_id = nx_json_get(vif, LCPD_JSON_VIF_ID_KEY);
        snprintf(buf + len, LG_API_URL_SIZE, ",%ld", vif_id->int_value);
        len = strlen(buf);
        i++;
    } while(1);

    if (i == 0) {
        LCPD_LOG(LOG_ERR, "invalid json recvived, no vif found.");
        buf[0] = 0;
        return -1;
    }

    snprintf(buf + len, LG_API_URL_SIZE, ")");
    return 0;
}

static int agexec_br_init(char *host_ip)
{
    uint8_t agexec_msg_buf[sizeof(agexec_msg_t)] = {0};
    agexec_msg_t *p_req = (agexec_msg_t *)agexec_msg_buf;
    agexec_msg_t *p_res = NULL;
    int  res;

    if (!host_ip) {
        return -1;
    }

    p_req->hdr.action_type = AGEXEC_ACTION_INI;
    p_req->hdr.object_type = AGEXEC_OBJ_BR;
    p_req->hdr.length = 0;

    VL2_SYSLOG(LOG_DEBUG, "%s: init network-uuid of ovs-br in %s\n",
            __FUNCTION__, host_ip);
    res = lc_agexec_exec(p_req, &p_res, host_ip);

    if (res != AGEXEC_TRUE || !p_res ||
            p_res->hdr.err_code != AGEXEC_ERR_SUCCESS) {
        if (p_res && p_res->hdr.err_code != AGEXEC_ERR_SUCCESS) {
            res = p_res->hdr.err_code;
        } else {
            res = -1;
        }

        LCPD_LOG(LOG_ERR, "init network-uuid of ovs-br in %s error, "
                "err_code=%d\n",host_ip, res);
        res = -1;
    } else {
        res = 0;
    }

    if (p_res) {
        free(p_res);
    }
    p_res = NULL;

    return res;
}

static int lc_get_host_network_info(network_resource_t *nr, host_t *host)
{
    if (agexec_br_init(nr->ip) != 0) {
        nr->err |= LC_NR_ERRNO_GET_INFO_ERROR;
        return -1;
    }

    return 0;
}

static bool host_sanity_check(host_t *host)
{
    if (host->if0_type == LC_BR_NONE || host->if1_type == LC_BR_NONE) {
        LCPD_LOG(LOG_ERR, "vm host has no control/data link.\n");
        return false;
    }
    if (host->host_role == LC_HOST_ROLE_GATEWAY &&
            host->if2_type == LC_BR_NONE) {
        LCPD_LOG(LOG_ERR, "vgw host has no uplink.\n");
        return false;
    }

    return true;
}

static int lc_host_join(struct lc_msg_head *m_head)
{
    struct msg_compute_resource_opt *host_join;
    network_resource_t nr, peer_nr;
    host_t host, peer;
    int pool_type;

    host_join = (struct msg_compute_resource_opt *)(m_head + 1);

    memset(&nr, 0, sizeof(nr));
    memset(&peer_nr, 0, sizeof(peer_nr));
    memset(&host, 0, sizeof(host));
    memset(&peer, 0, sizeof(peer));

    if (lc_vmdb_network_resource_load(&nr, host_join->network_id)) {
        LCPD_LOG(LOG_ERR, "LC ovs pushing failure: network not found\n");
        nr.err |= LC_NR_ERRNO_DB_ERROR;
        goto error;
    }

    if (nr.state == LC_NR_STATE_COMPLETE) {
        LCPD_LOG(LOG_DEBUG, "LC ovs pushing skipped: network poolified\n");
        lc_msg_forward((lc_mbuf_t *)m_head);
        return 0;
    }

    if (nr.state != LC_NR_STATE_PUSHING) {
        LCPD_LOG(LOG_ERR, "LC ovs pushing failure: db state incorrect\n");
        nr.err |= LC_NR_ERRNO_DB_ERROR;
        goto error;
    }

    if (lc_vmdb_get_pool_htype(nr.poolid) == LC_POOL_TYPE_XEN) {
        if (lc_vmdb_host_load(&host, nr.ip) || !host_sanity_check(&host)) {
            nr.err |= LC_NR_ERRNO_DB_ERROR;
            goto error;
        }
    }

    if (lc_vmdb_compute_resource_load(&host, nr.ip)) {
        nr.err |= LC_NR_ERRNO_DB_ERROR;
        goto error;
    }

    if ((host.host_service_flag == LC_HOST_PEER) ||
        (host.host_service_flag == LC_HOST_HA)) {

        if (lc_vmdb_network_resource_load_by_ip(&peer_nr, host.peer_address)
                || peer_nr.state != LC_NR_STATE_PUSHING) {
            LCPD_LOG(LOG_ERR, "LC ovs pushing failure: network not found\n");
            peer_nr.err |= LC_NR_ERRNO_DB_ERROR;
            goto error;
        }

        if (lc_vmdb_host_load(&peer, peer_nr.ip) ||
                !host_sanity_check(&host)) {
            peer_nr.err |= LC_NR_ERRNO_DB_ERROR;
            goto error;
        }

    }

    pool_type = lc_vmdb_get_pool_htype(nr.poolid);

    if (pool_type == LC_POOL_TYPE_XEN || pool_type == LC_POOL_TYPE_KVM) {
        if (host.uplink_ip[0] != 0) {
            /* config gateway uplink qos & policies */
            if (config_gateway_network(&host)) {
                LCPD_LOG(LOG_ERR, "Failed to config gateway network, "
                        "cr=%u-%s-%s.\n",host.id, host.ip, host.uplink_ip);
                nr.err |= LC_NR_ERRNO_RACK_TUNNEL_ERROR;
                goto error;
            }
        }
    }

    if (pool_type == LC_POOL_TYPE_XEN) {
        if (lc_get_host_network_info(&nr, &host)) {
            LCPD_LOG(LOG_ERR, "Get host network info error\n");
            goto error;
        }

        if ((host.host_service_flag == LC_HOST_PEER) ||
            (host.host_service_flag == LC_HOST_HA)) {

            if (lc_get_host_network_info(&peer_nr, &peer)) {
                LCPD_LOG(LOG_ERR, "Get host network info error\n");
                goto error;
            }
        }

    }

    if ((host.host_service_flag == LC_HOST_PEER) ||
        (host.host_service_flag == LC_HOST_HA)) {
        lc_vmdb_network_resource_update_state(LC_NR_STATE_COMPLETE,
                peer_nr.id);
    }
    lc_vmdb_network_resource_update_state(LC_NR_STATE_COMPLETE,
            host_join->network_id);

    lc_msg_forward((lc_mbuf_t *)m_head);

    return 0;

error:

    if ((host.host_service_flag == LC_HOST_PEER) ||
        (host.host_service_flag == LC_HOST_HA)) {
        lc_vmdb_network_resource_update_errno(peer_nr.err,
                peer_nr.id);
        lc_vmdb_network_resource_update_state(LC_NR_STATE_EXCEPTION,
                peer_nr.id);
    }
    lc_vmdb_network_resource_update_errno(nr.err,
            host_join->network_id);
    lc_vmdb_network_resource_update_state(LC_NR_STATE_EXCEPTION,
            host_join->network_id);

    lc_msg_rollback((lc_mbuf_t *)m_head);
    return -1;
}

static int lc_host_eject(struct lc_msg_head *m_head)
{
    struct msg_compute_resource_opt *host_eject;
    network_resource_t nr;
    char buf[LC_NAMESIZE];
    host_t host;

    host_eject = (struct msg_compute_resource_opt *)(m_head + 1);

    memset(&nr, 0, sizeof(nr));
    memset(buf, 0, sizeof(buf));
    memset(&host, 0, sizeof(host));

    if (lc_vmdb_network_resource_load(&nr, host_eject->network_id)
            || nr.state != LC_NR_STATE_PULLING) {
        LCPD_LOG(LOG_ERR, "LC ovs pulling failure: network not found");
        nr.err |= LC_NR_ERRNO_DB_ERROR;
        goto error;
    }

    if (lc_vmdb_host_load(&host, nr.ip)) {
        nr.err |= LC_NR_ERRNO_DB_ERROR;
        goto error;
    }

    lc_vmdb_network_resource_update_state(LC_NR_STATE_PULLED,
            host_eject->network_id);

    lc_msg_forward((lc_mbuf_t *)m_head);

    return 0;

error:

    lc_vmdb_network_resource_update_errno(nr.err,
            host_eject->network_id);
    lc_vmdb_network_resource_update_state(LC_NR_STATE_EXCEPTION,
            host_eject->network_id);

    lc_msg_rollback((lc_mbuf_t *)m_head);
    return -1;
}

static int lc_tunnel_switch_boot(struct msg_torswitch_opt *ts)
{
    int res = 0;
    char bandwidth[VL2_MAX_INTERF_BW_LEN];
    char vlantag_c[VL2_MAX_INTERF_VD_LEN];
    char conditions[VL2_MAX_DB_BUF_LEN] = {0};
    torswitch_t torswitch;
    rack_tunnel_t racktunnel[TORSWITCH_MAX_RACKTUNNEL_NUM];
    char tunnel_protocol[TUNNEL_PROTOCOL_MAX_LEN] = {0};
    char url[LG_API_URL_SIZE] = {0};
    char url2[LG_API_URL_SIZE] = {0};
    char result[LG_API_BIG_RESULT_SIZE] = {0};
    int vl2_table_ids[RACK_MAX_VLAN_NUM] = {0};
    int vif_table_ids[RACK_MAX_VLAN_NUM] = {0};
    int i = 0, err = 0, vl2id = 0, vlantag = 0, ii = 0;
    intf_t intf_info;
    uint64_t boot_time;
    int vl2_type = 0;
    char ip_list_src[TORSWITCH_MAX_IP_SEG] = {0};
    char ip_list_dst[TORSWITCH_MAX_IP_SEG] = {0};
    char ip_buf[VIF_MAX_IP_NUM][MAX_HOST_ADDRESS_LEN];
    char *ips[VIF_MAX_IP_NUM];
    int cur_len = 0;
    char ip_min[MAX_HOST_ADDRESS_LEN];
    char ip_max[MAX_HOST_ADDRESS_LEN];
    ip_prefix_t prefixes[PREFIX_MAX];
    int ip_prefix_count = 0;
    char post[LG_API_BIG_RESULT_SIZE] = {0};
    nx_json *root = NULL;

    memset(&torswitch, 0, sizeof(torswitch_t));
    memset(racktunnel, 0 ,sizeof(racktunnel));
    for (i = 0; i < VIF_MAX_IP_NUM; ++i) {
        ips[i] = ip_buf[i];
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'", TABLE_NETWORK_DEVICE_COLUMN_MIP, ts->ip);
    err = ext_vl2_db_network_device_load(&torswitch, conditions);
    if (err){
        LCPD_LOG(LOG_ERR, "%s: failed find torswitch for %s\n", __FUNCTION__, ts->ip);
        return _VL2_FALSE_;
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
             TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME, TABLE_DOMAIN_CONFIGURATION_TUNNEL_PROTOCOL,
             TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN, torswitch.domain);
    if (ext_vl2_db_lookup(TABLE_DOMAIN_CONFIGURATION, TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE, conditions, tunnel_protocol)) {
        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s cannot find tunnel protocol\n", __FUNCTION__);
        return _VL2_FALSE_;
    }

    /* update boot_time to 0 in db */
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'", TABLE_NETWORK_DEVICE_COLUMN_MIP, ts->ip);
    snprintf(result, sizeof(result), "boot_time=0");
    err = ext_vl2_db_updates(TABLE_NETWORK_DEVICE, result, conditions);

    /* refresh switch tunnel config */
    snprintf(url, LG_API_URL_SIZE, SDN_API_PREFIX SDN_API_SWITCH_TUNNEL_ALL,
            SDN_API_DEST_IP_SDNCTRL, SDN_CTRL_LISTEN_PORT, SDN_API_VERSION,
            torswitch.ip);
    LCPD_LOG(LOG_INFO, "Send url:%s\n", url);
    result[0] = 0;
    res = lcpd_call_api(url, API_METHOD_DEL, NULL, result);
    if (res) {  // curl failed or api failed
        LCPD_LOG(LOG_ERR, "failed to clean tunnels in switch mip = %s\n", torswitch.ip);
        LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
        return _VL2_FALSE_;
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'", TABLE_RACK_TUNNEL_COLUMN_SWITCH_IP, ts->ip);
    err = ext_vl2_db_rack_tunnel_loadn(racktunnel, sizeof(rack_tunnel_t), TORSWITCH_MAX_RACKTUNNEL_NUM, conditions);
    if (!err) {
        for (i = 0; i < TORSWITCH_MAX_RACKTUNNEL_NUM && racktunnel[i].local_uplink_ip[0] != '\0'; i++) {
            snprintf(url, LG_API_URL_SIZE, SDN_API_PREFIX SDN_API_SWITCH_TUNNEL,
                    SDN_API_DEST_IP_SDNCTRL, SDN_CTRL_LISTEN_PORT, SDN_API_VERSION,
                    torswitch.ip, racktunnel[i].remote_uplink_ip);
            LCPD_LOG(LOG_INFO, "Send url:%s\n", url);
            result[0] = 0;
            res = lcpd_call_api(url, API_METHOD_POST, NULL, result);
            if (res) {  // curl failed or api failed
                LCPD_LOG(
                    LOG_ERR,
                    "failed to build tunnel in switch mip=%s, remote_ip=%s\n",
                    torswitch.ip, racktunnel[i].remote_uplink_ip);
                LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
                return _VL2_FALSE_;
            }
        }
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN,
            "%s='%s'", TABLE_VL2_TUNNEL_POLICY_COLUMN_UPLINK_IP, ts->ip);
    if (ext_vl2_db_lookup_ids_v2(TABLE_VL2_TUNNEL_POLICY, TABLE_VL2_TUNNEL_POLICY_COLUMN_ID,
                conditions, vl2_table_ids, RACK_MAX_VLAN_NUM)) {
        goto done;
    }

    for (i = 0; i < RACK_MAX_VLAN_NUM && vl2_table_ids[i]; i++) {
        snprintf(conditions, VL2_MAX_DB_BUF_LEN,
                "%s=%d", TABLE_VL2_TUNNEL_POLICY_COLUMN_ID, vl2_table_ids[i]);
        if (ext_vl2_db_lookup_ids(TABLE_VL2_TUNNEL_POLICY, TABLE_VL2_TUNNEL_POLICY_COLUMN_VL2ID,
                    conditions, &vl2id)) {
            return _VL2_FALSE_;
        }
        if (ext_vl2_db_lookup_ids(TABLE_VL2_TUNNEL_POLICY, TABLE_VL2_TUNNEL_POLICY_COLUMN_VLANTAG,
                    conditions, &vlantag)) {
            return _VL2_FALSE_;
        }
        snprintf(url, LG_API_URL_SIZE, SDN_API_PREFIX SDN_API_SWITCH_SUBNET,
                SDN_API_DEST_IP_SDNCTRL, SDN_CTRL_LISTEN_PORT, SDN_API_VERSION,
                torswitch.ip, vl2id);
        LCPD_LOG(LOG_INFO, "Send url:%s\n", url);
        result[0] = 0;
        res = lcpd_call_api(url, API_METHOD_POST, NULL, result);
        if (res) {  // curl failed or api failed
            LCPD_LOG(
                LOG_ERR,
                "failed to add subnet in switch mip=%s, vl2id=%d, vlantag=%d\n",
                torswitch.ip, vl2id, vlantag);
            LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
            return _VL2_FALSE_;
        }
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN,
            "%s='%s'", TABLE_VINTERFACE_COLUMN_NETDEVICE_LCUUID, torswitch.lcuuid);
    if (ext_vl2_db_lookup_ids_v2(TABLE_VINTERFACE, TABLE_VINTERFACE_COLUMN_ID,
                conditions, vif_table_ids, RACK_MAX_VLAN_NUM)) {
        goto done;
    }

    for (ii = 0; ii < RACK_MAX_VLAN_NUM && vif_table_ids[ii]; ii++) {
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d",
                TABLE_VINTERFACE_COLUMN_ID, vif_table_ids[ii]);
        if (lc_db_vif_load(&intf_info, "*", conditions)) {
            LCPD_LOG(LOG_ERR, "failed to load vinterface info vifid=%d\n", vif_table_ids[ii]);
            return _VL2_FALSE_;
        }
        if (VIF_STATE_ATTACHED == intf_info.if_state) {
            memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
            snprintf(conditions, VL2_MAX_DB_BUF_LEN,
                    "id=%u", intf_info.if_subnetid);
            ext_vl2_db_lookup_ids_v2(TABLE_VL2, TABLE_VL2_COLUMN_NET_TYPE,
                    conditions, &vl2_type, 1);
            if (vl2_type == VIF_TYPE_WAN) {
                memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d",
                        TABLE_IP_RESOURCE_COLUMN_VIFID, intf_info.if_id);
                ext_vl2_db_lookups_v2(TABLE_IP_RESOURCE,
                        TABLE_IP_RESOURCE_COLUMN_IP, conditions, ips,
                        sizeof(ips) / sizeof(ips[0]));
                if (ips[0][0] == 0) {
                    LCPD_LOG(LOG_ERR, "failed to get the ips of the interface");
                    return _VL2_FALSE_;
                }
                cur_len = 0;
                for (i = 0; i < VIF_MAX_IP_NUM && ips[i] && ips[i][0]; ++i) {
                    if (i != 0) {
                        cur_len += snprintf(ip_list_src + cur_len, TORSWITCH_MAX_IP_SEG - cur_len, ",");
                    }
                    cur_len += snprintf(ip_list_src + cur_len, TORSWITCH_MAX_IP_SEG - cur_len, "%s/255.255.255.255", ips[i]);
                }
                snprintf(ip_list_dst, TORSWITCH_MAX_IP_SEG, "0.0.0.0/0.0.0.0");
            } else if (vl2_type == VIF_TYPE_LAN) {
                snprintf(ip_list_src, TORSWITCH_MAX_IP_SEG, "0.0.0.0/0.0.0.0");
                snprintf(ip_list_dst, TORSWITCH_MAX_IP_SEG, "0.0.0.0/0.0.0.0");
            } else if (vl2_type == VIF_TYPE_CTRL) {
                memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                        TABLE_DOMAIN_CONFIGURATION_CTRLLER_CTRL_IP_MIN,
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                        torswitch.domain);
                ext_vl2_db_lookups_v2(TABLE_DOMAIN_CONFIGURATION,
                                      TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE, conditions, ips, 1);
                if (ips[0][0] == 0) {
                    LCPD_LOG(LOG_ERR, "failed to get the ips of the vm_ctrl_ip_min");
                    return _VL2_FALSE_;
                }
                strncpy(ip_min, ips[0], MAX_HOST_ADDRESS_LEN);

                memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
                memset(ip_buf, 0, sizeof(ip_buf));
                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                        TABLE_DOMAIN_CONFIGURATION_CTRLLER_CTRL_IP_MAX,
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                        torswitch.domain);
                ext_vl2_db_lookups_v2(TABLE_DOMAIN_CONFIGURATION,
                                      TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE, conditions, ips, 1);
                if (ips[0][0] == 0) {
                    LCPD_LOG(LOG_ERR, "failed to get the ips of the vm_ctrl_ip_max");
                    return _VL2_FALSE_;
                }
                strncpy(ip_max, ips[0], MAX_HOST_ADDRESS_LEN);
                ip_prefix_count = iprange_to_prefix(ip_min, ip_max, prefixes);

                snprintf(ip_list_src, TORSWITCH_MAX_IP_SEG, "%s/255.255.255.255", intf_info.if_ip);
                cur_len = 0;
                for (i = 0; i < ip_prefix_count; ++i) {
                    if (i != 0) {
                        cur_len += snprintf(ip_list_dst + cur_len, TORSWITCH_MAX_IP_SEG - cur_len, ",");
                    }
                    cur_len += snprintf(ip_list_dst + cur_len, TORSWITCH_MAX_IP_SEG - cur_len, "%s/%s",
                                        prefixes[i].address, prefixes[i].mask);
                }
            } else if (vl2_type == VIF_TYPE_SERVICE) {
                memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                        TABLE_DOMAIN_CONFIGURATION_SERVICE_IP_MIN,
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                        torswitch.domain);
                ext_vl2_db_lookups_v2(TABLE_DOMAIN_CONFIGURATION,
                                      TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE, conditions, ips, 1);
                if (ips[0][0] == 0) {
                    LCPD_LOG(LOG_ERR, "failed to get the ips of the service_ip_min");
                    return _VL2_FALSE_;
                }
                strncpy(ip_min, ips[0], MAX_HOST_ADDRESS_LEN);

                memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
                memset(ip_buf, 0, sizeof(ip_buf));
                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                        TABLE_DOMAIN_CONFIGURATION_SERVICE_IP_MAX,
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                        torswitch.domain);
                ext_vl2_db_lookups_v2(TABLE_DOMAIN_CONFIGURATION,
                                      TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE, conditions, ips, 1);
                if (ips[0][0] == 0) {
                    LCPD_LOG(LOG_ERR, "failed to get the ips of the service_ip_max");
                    return _VL2_FALSE_;
                }
                strncpy(ip_max, ips[0], MAX_HOST_ADDRESS_LEN);
                ip_prefix_count = iprange_to_prefix(ip_min, ip_max, prefixes);

                snprintf(ip_list_src, TORSWITCH_MAX_IP_SEG, "%s/255.255.255.255", intf_info.if_ip);
                cur_len = 0;
                for (i = 0; i < ip_prefix_count; ++i) {
                    if (i != 0) {
                        cur_len += snprintf(ip_list_dst + cur_len, TORSWITCH_MAX_IP_SEG - cur_len, ",");
                    }
                    cur_len += snprintf(ip_list_dst + cur_len, TORSWITCH_MAX_IP_SEG - cur_len, "%s/%s",
                                        prefixes[i].address, prefixes[i].mask);
                }
            } else {
                LCPD_LOG(LOG_ERR, "vl2 type error: vifid=%u, vl2id=%u, type=%d",
                        intf_info.if_id, intf_info.if_subnetid, vl2_type);
                return _VL2_FALSE_;
            }

            sprintf(vlantag_c, "%d", intf_info.if_vlan);
            sprintf(bandwidth, "%lu", intf_info.if_bandw);

            root = (nx_json *)malloc(sizeof(nx_json));
            if (NULL == root) {
                return -1;
            }
            memset(root, 0, sizeof(nx_json));
            root->type = NX_JSON_OBJECT;
            nx_json_switch_conf_info(root, LCPD_API_CONF_SWITCH_ACT_CONF,
                    intf_info.if_port, vlantag_c, bandwidth, NULL,
                    intf_info.if_mac, ip_list_src, ip_list_dst);
            dump_json_msg(root, post, LG_API_BIG_RESULT_SIZE);
            nx_json_free(root);

            snprintf(url, LG_API_URL_SIZE, SDN_API_PREFIX SDN_API_SWITCH_PORT,
                    SDN_API_DEST_IP_SDNCTRL, SDN_CTRL_LISTEN_PORT, SDN_API_VERSION,
                    torswitch.ip, intf_info.if_port);
            snprintf(url2, LG_API_URL_SIZE, SDN_API_CALLBACK SDN_API_CALLBACK_VPORTS,
                     master_ctrl_ip, LCPD_TALKER_LISTEN_PORT, SDN_API_VERSION);
            strcat(url, url2);

            LCPD_LOG(LOG_INFO, "Send url: %s ##### post:%s\n", url, post);
            result[0] = 0;
            res = lcpd_call_api(url, API_METHOD_POST, post, result);
            if (res) {  // curl failed or api failed
                LCPD_LOG(LOG_ERR, "failed to config switch port on rack mip = %s\n", torswitch.ip);
                LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
                return _VL2_FALSE_;
            }
        } else {
            root = (nx_json *)malloc(sizeof(nx_json));
            if (NULL == root) {
                return -1;
            }
            memset(root, 0, sizeof(nx_json));
            root->type = NX_JSON_OBJECT;
            if (intf_info.if_port != 0) {
                nx_json_switch_conf_info(root, LCPD_API_CONF_SWITCH_ACT_CONF,
                        intf_info.if_port, NULL, NULL, NULL, intf_info.if_mac,
                        NULL, NULL);
            } else {
                nx_json_switch_conf_info(root, LCPD_API_CONF_SWITCH_ACT_NONE,
                        intf_info.if_port, NULL, NULL, NULL, intf_info.if_mac,
                        NULL, NULL);
            }
            dump_json_msg(root, post, LG_API_BIG_RESULT_SIZE);
            nx_json_free(root);

            snprintf(url, LG_API_URL_SIZE, SDN_API_PREFIX SDN_API_SWITCH_PORT,
                    SDN_API_DEST_IP_SDNCTRL, SDN_CTRL_LISTEN_PORT, SDN_API_VERSION,
                    torswitch.ip, intf_info.if_port);
            snprintf(url2, LG_API_URL_SIZE, SDN_API_CALLBACK SDN_API_CALLBACK_VPORTS,
                     master_ctrl_ip, LCPD_TALKER_LISTEN_PORT, SDN_API_VERSION);
            strcat(url, url2);

            LCPD_LOG(LOG_INFO, "Send url: %s ##### post:%s\n", url, post);
            result[0] = 0;
            res = lcpd_call_api(url, API_METHOD_PATCH, post, result);
            if (res) {  // curl failed or api failed
                LCPD_LOG(LOG_ERR, "failed to close switch port on rack mip = %s, port=%u\n",
                        torswitch.ip, intf_info.if_port);
                LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
                res = -1;
                return _VL2_FALSE_;
            }
        }
    }

done:

    /* update boot_time in db */
    snprintf(url, LG_API_URL_SIZE, SDN_API_PREFIX SDN_API_SWITCH_BOOT_TIME,
            SDN_API_DEST_IP_SDNCTRL, SDN_CTRL_LISTEN_PORT, SDN_API_VERSION,
            torswitch.ip);

    LCPD_LOG(LOG_INFO, "Send url:%s\n", url);
    result[0] = 0;
    res = lcpd_call_api(url, API_METHOD_GET, NULL, result);
    if (res) {  // curl failed or api failed
        LCPD_LOG(LOG_ERR, "failed to get boot time from switch mip = %s\n", torswitch.ip);
        LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
        return _VL2_FALSE_;
    } else {
        parse_json_msg_get_boot_time(&boot_time, result);
    }
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'", TABLE_NETWORK_DEVICE_COLUMN_MIP, ts->ip);
    snprintf(result, sizeof(result), "boot_time=%"PRId64,
            boot_time + SWITCH_BOOT_SECONDS);
    err = ext_vl2_db_updates(TABLE_NETWORK_DEVICE, result, conditions);

    return _VL2_TRUE_;
}


int lc_torswitch_boot(struct lc_msg_head *m_head)
{
    struct msg_torswitch_opt *tor = (struct msg_torswitch_opt *)(m_head + 1);
    char conditions[VL2_MAX_DB_BUF_LEN] = {0};
    int rackid = 0;
    int torswitch_type = 0;

    LCPD_LOG(LOG_INFO, "torswitch %s is boot", tor->ip);

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'", TABLE_NETWORK_DEVICE_COLUMN_MIP, tor->ip);
    if(ext_vl2_db_lookup_ids(TABLE_NETWORK_DEVICE, TABLE_NETWORK_DEVICE_COLUMN_RACKID, conditions, &rackid)) {
        LCPD_LOG(LOG_ERR, "failed find rack for %s\n", tor->ip);
        return _VL2_FALSE_;
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d", TABLE_RACK_COLUMN_ID, rackid);
    if(ext_vl2_db_lookup_ids(TABLE_RACK, TABLE_RACK_COLUMN_TORSWITCH_TYPE, conditions, &torswitch_type)) {
        LCPD_LOG(LOG_ERR, "failed get torswitch of rack%d", rackid);
        return _VL2_FALSE_;
    }

    if (torswitch_type == TORSWITCH_TYPE_OPENFLOW) {
        LCPD_LOG(LOG_ERR, "switchtype OPENFLOW is deprecated");
        assert(0);
        return _VL2_FALSE_;
    } else if (torswitch_type == TORSWITCH_TYPE_TUNNEL) {
        return lc_tunnel_switch_boot(tor);
    }

    return _VL2_FALSE_;
}

static int lc_host_boot(struct lc_msg_head *m_head)
{
    struct msg_host_ip_opt *host_start = NULL;
    host_t host = {0};
    host_t cr = {0};
    network_resource_t nr = {0};
    int pool_type;

    host_start = (struct msg_host_ip_opt *)(m_head + 1);

    if (lc_vmdb_network_resource_load_by_ip(&nr, host_start->host_ip)) {
        LCPD_LOG(LOG_ERR, "LC ovs pushing failure: network_resource not found\n");
        nr.err |= LC_NR_ERRNO_DB_ERROR;
        goto error;
    }

    if (lc_vmdb_compute_resource_load(&cr, host_start->host_ip)) {
        LCPD_LOG(LOG_ERR, "LC ovs pushing failure: compute_resource not found\n");
        nr.err |= LC_NR_ERRNO_DB_ERROR;
        goto error;
    }

    if (lc_vmdb_host_load(&host, host_start->host_ip)) {
        nr.err |= LC_NR_ERRNO_DB_ERROR;
        goto error;
    }

    pool_type = lc_vmdb_get_pool_htype(nr.poolid);
    if (pool_type == LC_POOL_TYPE_XEN && !host_sanity_check(&host)) {
        nr.err |= LC_NR_ERRNO_DB_ERROR;
        goto error;
    }

    if (pool_type == LC_POOL_TYPE_XEN || pool_type == LC_POOL_TYPE_KVM) {

        if (cr.uplink_ip[0] != 0) {
            /* config gateway uplink qos & policies */
            if (config_gateway_uplink(&cr)) {
                LCPD_LOG(LOG_ERR, "Failed to config gateway uplink, host=%u-%s, "
                        "cr=%u-%s-%s.\n", host.id, host.ip, cr.id, cr.ip, cr.uplink_ip);
                nr.err |= LC_NR_ERRNO_RACK_TUNNEL_ERROR;
                goto error;
            }
        }

        /* refresh network_resource */
        if (pool_type == LC_POOL_TYPE_XEN) {
            if (lc_get_host_network_info(&nr, &host)) {
                LCPD_LOG(LOG_ERR, "Get host network info error\n");
                goto error;
            }
        }
    }

    lc_vmdb_network_resource_update_state(LC_NR_STATE_COMPLETE, nr.id);
    if (m_head->magic == MSG_MG_UI2KER) {
        lc_lcpd_host_msg_reply(host_start->host_ip, TALKER__RESULT__SUCCESS,
                    m_head->seq);
        lc_msg_forward((lc_mbuf_t *)m_head);
    }
    return 0;

error:

    lc_vmdb_network_resource_update_errno(nr.err, nr.id);
    lc_vmdb_network_resource_update_state(LC_NR_STATE_EXCEPTION, nr.id);
    if (m_head->magic == MSG_MG_UI2KER) {
        lc_lcpd_host_msg_reply(host_start->host_ip, TALKER__RESULT__FAIL,
                    m_head->seq);
        lc_msg_rollback((lc_mbuf_t *)m_head);
    }
    return -1;
}

int is_server_mode_(uint32_t type, uint32_t vdevice_type, uint32_t vdevice_id)
{
    char condition[VL2_MAX_APP_CD_LEN] = {0};
    char value[VL2_MAX_APP_CD_LEN] = {0};
    int htype = -1;
    int err = 0;

    memset(value, 0, sizeof(value));
    if (vdevice_type == LC_VM_TYPE_GW) {
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", vdevice_id);
        err = ext_vl2_db_lookup("vnet", "gw_launch_server", condition, value);
    } else if (vdevice_type == LC_VM_TYPE_VM) {
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", vdevice_id);
        err = ext_vl2_db_lookup("vm", "launch_server", condition, value);
    } else if (vdevice_type == LC_VM_TYPE_VGATEWAY) {
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", vdevice_id);
        err = ext_vl2_db_lookup("vnet", "gw_launch_server", condition, value);
    }

    if (err) {
        LCPD_LOG(LOG_INFO, "Get launch_server error: type=%u and id=%u.\n",
                  vdevice_type, vdevice_id);
        return 0;
    }

    snprintf(condition, VL2_MAX_APP_CD_LEN, "ip='%s'", value);
    err = ext_vl2_db_lookup("host_device", "htype", condition, value);

    if (err || sscanf(value, "%d", &htype) != 1) {
        LCPD_LOG(LOG_INFO, "Get host htype error: ip=%s.\n",value);
        return 0;
    }

    return htype == type ? 1 : 0;
}

int is_server_mode_xen(uint32_t vdevice_type, uint32_t vdevice_id)
{
    return is_server_mode_(LC_HOST_HTYPE_XEN, vdevice_type, vdevice_id);
}

int is_server_mode_vmware(uint32_t vdevice_type, uint32_t vdevice_id)
{
    return is_server_mode_(LC_HOST_HTYPE_VMWARE, vdevice_type, vdevice_id);
}

int is_server_mode_kvm(uint32_t vdevice_type, uint32_t vdevice_id)
{
    return is_server_mode_(LC_HOST_HTYPE_KVM, vdevice_type, vdevice_id);
}

static int append_instance_vif_to_ms(nx_json *vif_to_ms,
                                     char *instance_type,
                                     char *instance_lcuuid,
                                     int if_index)
{
    nx_json *tmp = NULL;

    tmp = create_json(NX_JSON_STRING, LCPD_API_MS_INSTANCE_TYPE, vif_to_ms);
    tmp->text_value = instance_type;

    tmp = create_json(NX_JSON_STRING, LCPD_API_MS_INSTANCE_LCUUID, vif_to_ms);
    tmp->text_value = instance_lcuuid;

    tmp = create_json(NX_JSON_INTEGER, LCPD_API_MS_IF_INDEX, vif_to_ms);
    tmp->int_value = if_index;

    return 0;
}

static int append_vm_vif(nx_json *vif,
                         char *state,
                         int  vif_id,
                         int  if_index,
                         char *mac,
                         int  vlantag,
                         char *vm_label,
                         uint64_t  min_bandwidth,
                         uint64_t  max_bandwidth,
                         uint64_t  broadcast_min_bandwidth,
                         uint64_t  broadcast_max_bandwidth,
                         char *acl_type,
                         char *isolate_switch,
                         char *src_ips[],
                         char *dst_ips[],
                         char *dst_netmasks[])
{
    nx_json *tmp = NULL, *tmp2 = NULL, *tmp3 = NULL, *arr = NULL;
    int i;

    tmp = create_json(NX_JSON_STRING, LCPD_API_VPORTS_STATE, vif);
    tmp->text_value = state;

    tmp = create_json(NX_JSON_INTEGER, LCPD_API_VPORTS_VIF_ID, vif);
    tmp->int_value = vif_id;

    tmp = create_json(NX_JSON_INTEGER, LCPD_API_VPORTS_IF_INDEX, vif);
    tmp->int_value = if_index;

    tmp = create_json(NX_JSON_STRING, LCPD_API_VPORTS_MAC, vif);
    tmp->text_value = mac;

    tmp = create_json(NX_JSON_INTEGER, LCPD_API_VPORTS_VLANTAG, vif);
    tmp->int_value = vlantag;

    tmp = create_json(NX_JSON_STRING, LCPD_API_VPORTS_VM_LABEL, vif);
    tmp->text_value = vm_label;

    if (!strcmp(state, LCPD_API_VPORTS_STATE_DETACH)) {
        return 0;
    }

    tmp = create_json(NX_JSON_OBJECT, LCPD_API_VPORTS_QOS, vif);
    tmp2 = create_json(NX_JSON_INTEGER, LCPD_API_VPORTS_QOS_MIN_BANDWIDTH, tmp);
    tmp2->int_value = min_bandwidth;
    tmp2 = create_json(NX_JSON_INTEGER, LCPD_API_VPORTS_QOS_MAX_BANDWIDTH, tmp);
    tmp2->int_value = max_bandwidth;
    tmp = create_json(NX_JSON_OBJECT, LCPD_API_VPORTS_BROADCAST_QOS, vif);
    tmp2 = create_json(NX_JSON_INTEGER, LCPD_API_VPORTS_QOS_MIN_BANDWIDTH, tmp);
    tmp2->int_value = broadcast_min_bandwidth;
    tmp2 = create_json(NX_JSON_INTEGER, LCPD_API_VPORTS_QOS_MAX_BANDWIDTH, tmp);
    tmp2->int_value = broadcast_max_bandwidth;

    tmp = create_json(NX_JSON_OBJECT, LCPD_API_VPORTS_ACL, vif);
    tmp2 = create_json(NX_JSON_STRING, LCPD_API_VPORTS_ACL_TYPE, tmp);
    tmp2->text_value = acl_type;
    tmp2 = create_json(NX_JSON_STRING, LCPD_API_VPORTS_ACL_ISOLATE_SWITCH, tmp);
    tmp2->text_value = isolate_switch;
    if (!strcmp(acl_type, LCPD_API_VPORTS_ACL_TYPE_SRC_IP) || !strcmp(acl_type, LCPD_API_VPORTS_ACL_TYPE_SRC_DST_IP)) {
        arr = create_json(NX_JSON_ARRAY, LCPD_API_VPORTS_ACL_SRC_IPS, tmp);
        for (i = 0; src_ips[i][0] != '\0'; i++) {
            tmp3 = create_json(NX_JSON_OBJECT, NULL, arr);
            tmp2 = create_json(NX_JSON_STRING, LCPD_API_VPORTS_ACL_IPS_ADDRESS, tmp3);
            tmp2->text_value = src_ips[i];
            tmp2 = create_json(NX_JSON_STRING, LCPD_API_VPORTS_ACL_IPS_NETMASK, tmp3);
            tmp2->text_value = VL2_MAX_POTS_SP_VALUE;
        }
    }

    if (!strcmp(acl_type, LCPD_API_VPORTS_ACL_TYPE_SRC_DST_IP)) {
        arr = create_json(NX_JSON_ARRAY, LCPD_API_VPORTS_ACL_DST_IPS, tmp);
        for (i = 0; dst_ips[i][0] != '\0'; i++) {
            tmp3 = create_json(NX_JSON_OBJECT, NULL, arr);
            tmp2 = create_json(NX_JSON_STRING, LCPD_API_VPORTS_ACL_IPS_ADDRESS, tmp3);
            tmp2->text_value = dst_ips[i];
            tmp2 = create_json(NX_JSON_STRING, LCPD_API_VPORTS_ACL_IPS_NETMASK, tmp3);
            tmp2->text_value = dst_netmasks[i];
        }
    }

    return 0;
}

static int nx_json_switch_conf_info(nx_json *confswitch,
                         char *operation,
                         int sw_port,
                         char *vlan_tag,
                         char *bandwidth,
                         char *trunk,
                         char *src_mac,
                         char *src_nets,
                         char *dst_nets
                         )
{
    nx_json *tmp = NULL, *tmp2 = NULL;

    tmp = create_json(NX_JSON_OBJECT, SDN_API_SWITCH_CONF_INFO, confswitch);
    tmp2 = create_json(NX_JSON_STRING, SDN_API_SWITCH_CONF_INFO_OP, tmp);
    tmp2->text_value = operation;

    tmp2 = create_json(NX_JSON_INTEGER, SDN_API_SWITCH_CONF_INFO_PORT, tmp);
    tmp2->int_value = sw_port;

    if (vlan_tag != NULL) {
        tmp2 = create_json(NX_JSON_INTEGER, SDN_API_SWITCH_CONF_INFO_VLAN, tmp);
        tmp2->int_value = atoi(vlan_tag);
    }

    if (bandwidth != NULL) {
        tmp2 = create_json(NX_JSON_INTEGER, SDN_API_SWITCH_CONF_INFO_BW, tmp);
        tmp2->int_value = atoi(bandwidth);
    }

    if (trunk != NULL) {
        tmp2 = create_json(NX_JSON_STRING, SDN_API_SWITCH_CONF_INFO_TRUNK, tmp);
        tmp2->text_value = trunk;
    }

    if (src_mac != NULL) {
        tmp2 = create_json(NX_JSON_STRING, SDN_API_SWITCH_CONF_INFO_SRC_MAC, tmp);
        tmp2->text_value = src_mac;
    }

    if (src_nets != NULL) {
        tmp2 = create_json(NX_JSON_STRING, SDN_API_SWITCH_CONF_INFO_SRC_NETS, tmp);
        tmp2->text_value = src_nets;
    }

    if (dst_nets != NULL) {
        tmp2 = create_json(NX_JSON_STRING, SDN_API_SWITCH_CONF_INFO_DST_NETS, tmp);
        tmp2->text_value = dst_nets;
    }

    return 0;
}

static int ip_net_to_prefix(char const * const ip_min,
        char const * const ip_max, char * const prefix, char * const netmask)
{
    uint32_t min= 0;
    uint32_t max= 0;
    uint32_t ip_prefix= 0;
    uint32_t tmp, i;

    min = inet_network(ip_min);
    max = inet_network(ip_max);
    tmp = min ^ max;
    i = (uint32_t)log2((double)(++tmp));
    tmp = htonl(0xffffffff << i);
    ip_prefix = htonl(min) & tmp;
    sprintf(prefix, "%u.%u.%u.%u", ip_prefix & 0xff, (ip_prefix >> 8) & 0xff,
            (ip_prefix >> 16) & 0xff, (ip_prefix >> 24) & 0xff);
    sprintf(netmask, "%u.%u.%u.%u", tmp & 0xff, (tmp >> 8) & 0xff,
            (tmp >> 16) & 0xff, (tmp >> 24) & 0xff);
    return 0;
}

static int vm_curl_vport_attached(int vm_id, char *acl_action, int callback_flag)
{
    /* used for vm boot_or_down(cb)/start/isolate/release */
    char server_ip[VL2_MAX_POTS_SP_LEN];
    char condition[VL2_MAX_APP_CD_LEN];
    char if_mac[LC_VM_IFACES_MAX][MAX_HOST_ADDRESS_LEN];
    char src_addr[LC_VM_IFACES_MAX][VIF_MAX_IP_NUM][MAX_HOST_ADDRESS_LEN];
    char *psrc_addr[LC_VM_IFACES_MAX][VIF_MAX_IP_NUM];
    char dst_ip_min[MAX_HOST_ADDRESS_LEN];
    char dst_ip_max[MAX_HOST_ADDRESS_LEN];
    char dst_ip[LC_VM_IFACES_MAX][LC_VM_IFACE_ACL_MAX][MAX_HOST_ADDRESS_LEN];
    char dst_netmask[LC_VM_IFACES_MAX][LC_VM_IFACE_ACL_MAX][MAX_HOST_ADDRESS_LEN];
    char *pdst_ip[LC_VM_IFACES_MAX][LC_VM_IFACE_ACL_MAX];
    char *pdst_netmask[LC_VM_IFACES_MAX][LC_VM_IFACE_ACL_MAX];
    char url[LG_API_URL_SIZE] = {0};
    char url2[LG_API_URL_SIZE] = {0};
    char post[LG_API_BIG_RESULT_SIZE] = {0};
    char vinterface_ids[LG_API_URL_SIZE] = {0};
    char resp_vinterface_ids[LG_API_URL_SIZE] = {0};
    char result[LG_API_BIG_RESULT_SIZE] = {0};
    char value[VL2_MAX_APP_CD_LEN];
    char domain[LCUUID_LEN] = {0};
    char vm_lcuuid[LCUUID_LEN] = {0};
    char ms_lcuuid[LCUUID_LEN] = {0};
    char vm_label[LC_VM_LABEL_LEN] = {0};
    char if_subtype[VL2_MAX_INTERF_SB_LEN] = {0};
    int vlantag, vl2id, interface_id, vm_flag;
    char bandwidth[VL2_MAX_INTERF_BW_LEN];
    uint64_t bandw, broadcast_bandw, broadcast_ceil_bandw;
    int state, vl2_type, len;
    uint32_t if_subtype_int = 0;
    nx_json *root = NULL;
    nx_json *vif = NULL;
    nx_json *vifs_to_ms[LC_VM_IFACES_MAX] = {NULL};
    uint32_t ms_ids[LC_VM_IFACES_MAX] = {0};
    int res = 0;
    int i, j;

    if (is_server_mode_vmware(LC_VM_TYPE_VM, vm_id)) {
        LCPD_LOG(LOG_WARNING, "VMware VM will never arrive here.");
        return 0;
    }

    memset(if_mac, 0, sizeof(if_mac));
    memset(src_addr, 0, sizeof(src_addr));
    memset(dst_ip, 0, sizeof(dst_ip));
    memset(dst_netmask, 0, sizeof(dst_netmask));

    for (i = 0; i < LC_VM_IFACES_MAX; i++) {
        for (j = 0; j < VIF_MAX_IP_NUM; j++) {
            psrc_addr[i][j] = src_addr[i][j];
        }
    }

    for (i = 0; i < LC_VM_IFACES_MAX; i++) {
        for (j = 0; j < LC_VM_IFACE_ACL_MAX; j++) {
            pdst_ip[i][j] = dst_ip[i][j];
            pdst_netmask[i][j] = dst_netmask[i][j];
        }
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_ARRAY;

    for (i = 0; i < LC_VM_IFACES_MAX; ++i) {
        vifs_to_ms[i] = (nx_json *)malloc(sizeof(nx_json));
        if (NULL == vifs_to_ms[i]) {
            return -1;
        }
        memset(vifs_to_ms[i], 0, sizeof(nx_json));
        vifs_to_ms[i]->type = NX_JSON_OBJECT;
    }

    sprintf(condition, "id=%u", vm_id);
    memset(server_ip, 0, VL2_MAX_POTS_SP_LEN);
    res = ext_vl2_db_lookup("vm", "launch_server", condition, server_ip);
    memset(domain, 0, sizeof(domain));
    res = ext_vl2_db_lookup("vm", "domain", condition, domain);
    memset(vm_lcuuid, 0, sizeof(vm_lcuuid));
    res = ext_vl2_db_lookup("vm", "lcuuid", condition, vm_lcuuid);
    res = ext_vl2_db_lookup("vm", "label", condition, vm_label);
    res = ext_vl2_db_lookup_ids_v2("vm", "flag", condition, &vm_flag, 1);

    snprintf(vinterface_ids, LG_API_URL_SIZE, "(-1");
    len = strlen(vinterface_ids);
    for (i = 0; i < LC_VM_IFACES_MAX; ++i) {
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN,
                "ifindex=%u AND deviceid=%u AND devicetype=%u",
                i, vm_id, LC_VM_TYPE_VM);
        res = ext_vl2_db_lookup_ids_v2("vinterface", "id", condition,
                &interface_id, 1);
        res = ext_vl2_db_lookup("vinterface", "mac", condition, if_mac[i]);
        res = ext_vl2_db_lookup_ids_v2("vinterface", "vlantag", condition,
                &vlantag, 1);
        memset(value, 0, sizeof(value));
        res = ext_vl2_db_lookup("vinterface", "ms_id", condition, value);
        sscanf(value, "%u", &ms_ids[i]);
        res = ext_vl2_db_lookup("vinterface", "bandw", condition, bandwidth);
        sscanf(bandwidth, "%lu", &bandw);
        memset(bandwidth, 0, sizeof(bandwidth));
        res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                                TABLE_VINTERFACE_COLUMN_BROADC_BANDW,
                                condition, bandwidth);
        sscanf(bandwidth, "%lu", &broadcast_bandw);
        memset(bandwidth, 0, sizeof(bandwidth));
        res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                                TABLE_VINTERFACE_COLUMN_BROADC_CEIL_BANDW,
                                condition, bandwidth);
        sscanf(bandwidth, "%lu", &broadcast_ceil_bandw);
        res = ext_vl2_db_lookup_ids_v2("vinterface", "subnetid", condition,
                &vl2id, 1);
        res = ext_vl2_db_lookup_ids_v2("vinterface", "state", condition, &state, 1);
        if (if_mac[i][0] == '\0') {
            LCPD_LOG(LOG_WARNING, "skip interface %d config due to mac absence\n", interface_id);
            continue;
        }

        memset(if_subtype, 0, sizeof(if_subtype));
        res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                                TABLE_VINTERFACEIP_COLUMN_SUBTYPE,
                                condition, if_subtype);
        if_subtype_int = atoi(if_subtype);

        snprintf(vinterface_ids + len, LG_API_URL_SIZE, ",%ld", (long)interface_id);
        len = strlen(vinterface_ids);

        vif = create_json(NX_JSON_OBJECT, NULL, root);
        if (state != LC_VIF_STATE_ATTACHED) {
            append_vm_vif(vif,
                    LCPD_API_VPORTS_STATE_DETACH,
                    interface_id, i,
                    if_mac[i], vlantag,
                    vm_label,
                    0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL);
            continue;
        }

        if (i == LC_VM_IFACE_CTRL) {
            //ctrl vlantag
            vlantag = 0;
            //ctrl bandwidth
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                    TABLE_DOMAIN_CONFIGURATION_OFS_CTRL_BANDWIDTH,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                    domain);
            res += ext_vl2_db_lookup(
                    TABLE_DOMAIN_CONFIGURATION,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                    condition, bandwidth);
            sscanf(bandwidth, "%lu", &bandw);

            //launch_server net
            snprintf(pdst_ip[i][0], sizeof(dst_ip[i][0]), "%s", server_ip);
            snprintf(pdst_netmask[i][0], sizeof(dst_netmask[i][0]), "255.255.255.255");
            //ctrl net
            if (vm_flag & LC_VM_TALK_WITH_CONTROLLER_FLAG) {
                memset(condition, 0, VL2_MAX_APP_CD_LEN);
                snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                        TABLE_DOMAIN_CONFIGURATION_CTRLLER_CTRL_IP_MIN,
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                        domain);
                res += ext_vl2_db_lookup(
                        TABLE_DOMAIN_CONFIGURATION,
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                        condition, dst_ip_min);
                memset(condition, 0, VL2_MAX_APP_CD_LEN);
                snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                        TABLE_DOMAIN_CONFIGURATION_CTRLLER_CTRL_IP_MAX,
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                        domain);
                res += ext_vl2_db_lookup(
                        TABLE_DOMAIN_CONFIGURATION,
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                        condition, dst_ip_max);
                //convert net
                ip_net_to_prefix(dst_ip_min, dst_ip_max,
                                 pdst_ip[i][1], pdst_netmask[i][1]);
            }

            //load ctrl ip
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s=%u",
                    TABLE_VINTERFACE_COLUMN_ID,
                    interface_id);
            res += ext_vl2_db_lookups_v2(TABLE_VINTERFACE,
                    TABLE_VINTERFACE_COLUMN_IP, condition, psrc_addr[i], 1);

            append_vm_vif(vif,
                    LCPD_API_VPORTS_STATE_ATTACH,
                    interface_id, i, if_mac[i], vlantag, vm_label,
                    0, bandw, broadcast_bandw, broadcast_ceil_bandw,
                    LCPD_API_VPORTS_ACL_TYPE_SRC_DST_IP,
                    acl_action, psrc_addr[i], pdst_ip[i], pdst_netmask[i]);
        } else if (i == LC_VM_IFACE_SERV) {
            //service bandwidth
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                    TABLE_DOMAIN_CONFIGURATION_OFS_SERV_BANDWIDTH,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                    domain);
            res += ext_vl2_db_lookup(
                    TABLE_DOMAIN_CONFIGURATION,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                    condition, bandwidth);
            sscanf(bandwidth, "%lu", &bandw);
            //service net
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                    TABLE_DOMAIN_CONFIGURATION_SERVICE_IP_MIN,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                    domain);
            res += ext_vl2_db_lookup(
                    TABLE_DOMAIN_CONFIGURATION,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                    condition, dst_ip_min);
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                    TABLE_DOMAIN_CONFIGURATION_SERVICE_IP_MAX,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                    domain);
            res += ext_vl2_db_lookup(
                    TABLE_DOMAIN_CONFIGURATION,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                    condition, dst_ip_max);
            //convert net
            ip_net_to_prefix(dst_ip_min, dst_ip_max, pdst_ip[i][0], pdst_netmask[i][0]);

            //load server ip
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s=%u",
                    TABLE_VINTERFACE_COLUMN_ID,
                    interface_id);
            res += ext_vl2_db_lookups_v2(TABLE_VINTERFACE,
                    TABLE_VINTERFACE_COLUMN_IP, condition, psrc_addr[i], 1);

            append_vm_vif(vif,
                    LCPD_API_VPORTS_STATE_ATTACH,
                    interface_id, i, if_mac[i], vlantag, vm_label,
                    0, bandw, broadcast_bandw, broadcast_ceil_bandw,
                    LCPD_API_VPORTS_ACL_TYPE_SRC_DST_IP,
                    acl_action, psrc_addr[i], pdst_ip[i], pdst_netmask[i]);
        } else {
            /* custom interfaces */
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_APP_CD_LEN,
                    "id=%u", vl2id);
            ext_vl2_db_lookup_ids_v2(TABLE_VL2, TABLE_VL2_COLUMN_NET_TYPE,
                    condition, &vl2_type, 1);
            if (vl2_type == VIF_TYPE_WAN) {
                if ((if_subtype_int & VIF_SUBTYPE_L2_INTERFACE)
                    != VIF_SUBTYPE_L2_INTERFACE) {
                    //load ip
                    memset(condition, 0, VL2_MAX_APP_CD_LEN);
                    snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s=%u",
                            TABLE_IP_RESOURCE_COLUMN_VIFID, interface_id);
                    res += ext_vl2_db_lookups_v2(
                            TABLE_IP_RESOURCE, TABLE_IP_RESOURCE_COLUMN_IP,
                            condition, psrc_addr[i], VIF_MAX_IP_NUM);

                    append_vm_vif(vif,
                            LCPD_API_VPORTS_STATE_ATTACH,
                            interface_id, i, if_mac[i], vlantag, vm_label,
                            bandw, bandw, broadcast_bandw, broadcast_ceil_bandw,
                            LCPD_API_VPORTS_ACL_TYPE_SRC_IP,
                            acl_action, psrc_addr[i], NULL, NULL);
                } else {
                    append_vm_vif(vif,
                            LCPD_API_VPORTS_STATE_ATTACH,
                            interface_id, i, if_mac[i], vlantag, vm_label,
                            bandw, bandw, broadcast_bandw, broadcast_ceil_bandw,
                            LCPD_API_VPORTS_ACL_TYPE_ALL_DROP,
                            acl_action, NULL, NULL, NULL);
                }
            } else {
                if ((if_subtype_int & VIF_SUBTYPE_L2_INTERFACE)
                    != VIF_SUBTYPE_L2_INTERFACE) {
                    append_vm_vif(vif,
                            LCPD_API_VPORTS_STATE_ATTACH,
                            interface_id, i, if_mac[i], vlantag, vm_label,
                            0, bandw, broadcast_bandw, broadcast_ceil_bandw,
                            LCPD_API_VPORTS_ACL_TYPE_SRC_MAC, acl_action,
                            NULL, NULL, NULL);
                } else {
                    append_vm_vif(vif,
                            LCPD_API_VPORTS_STATE_ATTACH,
                            interface_id, i, if_mac[i], vlantag, vm_label,
                            0, bandw, broadcast_bandw, broadcast_ceil_bandw,
                            LCPD_API_VPORTS_ACL_TYPE_ALL_DROP, acl_action,
                            NULL, NULL, NULL);
                }
            }
        }
        append_instance_vif_to_ms(vifs_to_ms[i], "VM", vm_lcuuid, i);
    }
    snprintf(vinterface_ids + len, LG_API_URL_SIZE, ")");

    dump_json_msg(root, post, LG_API_BIG_RESULT_SIZE);
    nx_json_free(root);

    snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_VPORTS,
            server_ip, LCPD_AGEXEC_DEST_PORT, LCPD_API_VERSION);
    if (callback_flag) {
        snprintf(url2, LG_API_URL_SIZE, LCPD_API_CALLBACK LCPD_API_CALLBACK_VPORTS,
                 master_ctrl_ip, LCPD_TALKER_LISTEN_PORT,  LCPD_API_VERSION);
        strcat(url, url2);
    }

    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "id in %s", vinterface_ids);
    ext_vl2_db_update("vinterface", "errno", "0", condition);

    LCPD_LOG(LOG_INFO, "Send url post:%s\n", post);
    result[0] = 0;
    res = lcpd_call_api(url, API_METHOD_POST, post, result);
    if (res != 0) {  // curl failed or api failed
        LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
        parse_json_msg_vif_config_result(result, resp_vinterface_ids);
        goto finish;
    }

    for (i = 0; i < LC_VM_IFACES_MAX; ++i) {
        if (ms_ids[i] == 0) {
            continue;
        }
        memset(post, 0, LG_API_BIG_RESULT_SIZE);
        dump_json_msg(vifs_to_ms[i], post, LG_API_BIG_RESULT_SIZE);
        nx_json_free(vifs_to_ms[i]);

        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        memset(ms_lcuuid, 0, sizeof(ms_lcuuid));
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", ms_ids[i]);
        res = ext_vl2_db_lookup("micro_segment", "lcuuid", condition,
                ms_lcuuid);

        memset(url, 0, LG_API_URL_SIZE);
        snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_MS_VIF_ATTACH,
                LCPD_TALKER_LISTEN_IP, LCPD_TALKER_LISTEN_PORT,
                LCPD_API_VERSION, ms_lcuuid);
        LCPD_LOG(LOG_INFO, "Send url %s post:%s\n", url, post);
        result[0] = 0;
        res = lcpd_call_api(url, API_METHOD_POST, post, result);
        if (res != 0) {  // curl failed or api failed
            LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
            goto finish;
        }
    }

finish:
    if (res) {
        if (resp_vinterface_ids[0] == 0) {
            snprintf(resp_vinterface_ids, sizeof(resp_vinterface_ids),
                vinterface_ids);
        }
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id in %s", resp_vinterface_ids);
        snprintf(value, VL2_MAX_APP_CD_LEN, "%d", LC_VIF_ERRNO_NETWORK);
        ext_vl2_db_update("vinterface", "errno", value, condition);
    }
    return res;
}

static int vm_curl_vport_detached(int vm_id, int callback_flag)
{
    /* used for vm boot_or_down(cb)/stop/del */
    char if_mac[LC_VM_IFACES_MAX][MAX_HOST_ADDRESS_LEN];
    char condition[VL2_MAX_APP_CD_LEN];
    char server_ip[VL2_MAX_POTS_SP_LEN] = {0};
    char url[LG_API_URL_SIZE] = {0};
    char url2[LG_API_URL_SIZE] = {0};
    char post[LG_API_BIG_RESULT_SIZE] = {0};
    char vinterface_ids[LG_API_URL_SIZE] = {0};
    char resp_vinterface_ids[LG_API_URL_SIZE] = {0};
    char result[LG_API_BIG_RESULT_SIZE] = {0};
    char value[VL2_MAX_APP_CD_LEN];
    char domain[LCUUID_LEN] = {0};
    char vm_lcuuid[LCUUID_LEN] = {0};
    char ms_lcuuid[LCUUID_LEN] = {0};
    char vm_label[LC_VM_LABEL_LEN] = {0};
    nx_json *root = NULL;
    nx_json *vif = NULL;
    uint32_t ms_ids[LC_VM_IFACES_MAX] = {0};
    int vlantag, state;
    int interface_id;
    int res, i, len;

    if (is_server_mode_vmware(LC_VM_TYPE_VM, vm_id)) {
        LCPD_LOG(LOG_WARNING, "VMware VM will never arrive here.");
        return 0;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_ARRAY;

    memset(if_mac, 0, sizeof(if_mac));

    sprintf(condition, "id=%u", vm_id);
    memset(server_ip, 0, VL2_MAX_POTS_SP_LEN);
    res = ext_vl2_db_lookup("vm", "launch_server", condition, server_ip);
    memset(domain, 0, sizeof(domain));
    res = ext_vl2_db_lookup("vm", "domain", condition, domain);
    memset(vm_lcuuid, 0, sizeof(vm_lcuuid));
    res = ext_vl2_db_lookup("vm", "lcuuid", condition, vm_lcuuid);
    res = ext_vl2_db_lookup("vm", "label", condition, vm_label);

    snprintf(vinterface_ids, LG_API_URL_SIZE, "(-1");
    len = strlen(vinterface_ids);
    for (i = 0; i < LC_VM_IFACES_MAX; ++i) {
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN,
                "ifindex=%u AND deviceid=%u AND devicetype=%u",
                i, vm_id, LC_VM_TYPE_VM);
        res = ext_vl2_db_lookup_ids_v2("vinterface", "state", condition, &state, 1);
        if (state != LC_VIF_STATE_ATTACHED) {
            continue;
        }
        res = ext_vl2_db_lookup_ids_v2("vinterface", "id", condition,
                &interface_id, 1);
        res = ext_vl2_db_lookup("vinterface", "mac", condition, if_mac[i]);
        res = ext_vl2_db_lookup_ids_v2("vinterface", "vlantag", condition,
                &vlantag, 1);
        memset(value, 0, sizeof(value));
        res = ext_vl2_db_lookup("vinterface", "ms_id", condition, value);
        sscanf(value, "%u", &ms_ids[i]);
        if (if_mac[i][0] == '\0') {
            LCPD_LOG(LOG_WARNING, "skip interface %d config due to mac absence\n", interface_id);
            continue;
        }

        snprintf(vinterface_ids + len, LG_API_URL_SIZE, ",%ld", (long)interface_id);
        len = strlen(vinterface_ids);

        vif = create_json(NX_JSON_OBJECT, NULL, root);
        if (i == LC_VM_IFACE_CTRL) {
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                    TABLE_DOMAIN_CONFIGURATION_OFS_CTRL_VLAN,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                    domain);
            res += ext_vl2_db_lookup_ids_v2(
                    TABLE_DOMAIN_CONFIGURATION,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                    condition, &vlantag, 1);
            append_vm_vif(vif,
                    LCPD_API_VPORTS_STATE_DETACH,
                    interface_id, i,
                    if_mac[i], vlantag, vm_label,
                    0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL);
        } else {
            append_vm_vif(vif,
                    LCPD_API_VPORTS_STATE_DETACH,
                    interface_id, i,
                    if_mac[i], vlantag, vm_label,
                    0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL);
        }
    }
    snprintf(vinterface_ids + len, LG_API_URL_SIZE, ")");

    dump_json_msg(root, post, LG_API_BIG_RESULT_SIZE);
    nx_json_free(root);

    snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_VPORTS,
            server_ip, LCPD_AGEXEC_DEST_PORT, LCPD_API_VERSION);
    if (callback_flag) {
        snprintf(url2, LG_API_URL_SIZE, LCPD_API_CALLBACK LCPD_API_CALLBACK_VPORTS,
                 master_ctrl_ip, LCPD_TALKER_LISTEN_PORT,  LCPD_API_VERSION);
        strcat(url, url2);
    }

    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "id in %s", vinterface_ids);
    ext_vl2_db_update("vinterface", "errno", "0", condition);

    LCPD_LOG(LOG_INFO, "Send url post:%s\n", post);
    result[0] = 0;
    res = lcpd_call_api(url, API_METHOD_POST, post, result);
    if (res) {  // curl failed or api failed
        LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
        parse_json_msg_vif_config_result(result, resp_vinterface_ids);
        goto finish;
    }

    for (i = 0; i < LC_VM_IFACES_MAX; ++i) {
        if (ms_ids[i] == 0) {
            continue;
        }
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        memset(ms_lcuuid, 0, sizeof(ms_lcuuid));
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", ms_ids[i]);
        res = ext_vl2_db_lookup("micro_segment", "lcuuid", condition,
                ms_lcuuid);

        memset(url, 0, LG_API_URL_SIZE);
        snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_MS_VIF_DETACH,
                LCPD_TALKER_LISTEN_IP, LCPD_TALKER_LISTEN_PORT,
                LCPD_API_VERSION, ms_lcuuid, "VM", vm_lcuuid, i);
        LCPD_LOG(LOG_INFO, "Send url %s post:%s\n", url, post);
        result[0] = 0;
        res = lcpd_call_api(url, API_METHOD_DEL, NULL, result);
        if (res != 0) {  // curl failed or api failed
            LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
            goto finish;
        }
    }

finish:
    if (res) {
        if (resp_vinterface_ids[0] == 0) {
            snprintf(resp_vinterface_ids, sizeof(resp_vinterface_ids),
                vinterface_ids);
        }
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id in %s", resp_vinterface_ids);
        snprintf(value, VL2_MAX_APP_CD_LEN, "%d", LC_VIF_ERRNO_NETWORK);
        ext_vl2_db_update("vinterface", "errno", value, condition);
    }
    return res;
}

static int request_kernel_add_vm(struct lc_msg_head *m_head)
{
    struct msg_vm_opt *m_opt;
    char interface_id[VL2_MAX_POTS_ID_LEN];
    char condition[VL2_MAX_APP_CD_LEN];
    char label[VL2_MAX_DB_BUF_LEN];
    int vl2id, vl2_type;
    int i, res = _VL2_TRUE_;
    uint32_t vlantag_int;
    char rack[VL2_MAX_RACK_ID_LEN];

    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    m_opt = (struct msg_vm_opt *)(m_head + 1);
    if (is_server_mode_vmware(LC_VM_TYPE_VM, m_opt->vm_id)) {
        LCPD_LOG(LOG_WARNING, "VMware VM will never arrive here.");
        return 0;
    }

    for (i = 0; i < LC_VM_IFACES_MAX; ++i) {
        res = 0;

        memset(condition, 0, sizeof(condition));
        snprintf(condition, VL2_MAX_APP_CD_LEN,
                "ifindex=%u AND deviceid=%u AND devicetype=%u",
                i, m_opt->vm_id, LC_VM_TYPE_VM);
        res += ext_vl2_db_lookup("vinterface", "id", condition, interface_id);
        res += ext_vl2_db_lookup_ids_v2("vinterface", "subnetid", condition,
                &vl2id, 1);

        memset(condition, 0, sizeof(condition));
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", vl2id);
        res += ext_vl2_db_lookup_ids_v2(TABLE_VL2, TABLE_VL2_COLUMN_NET_TYPE,
                condition, &vl2_type, 1);

        if (vl2_type == VIF_TYPE_SERVICE || vl2_type == VIF_TYPE_LAN) {
            memset(rack, 0, sizeof(rack));
            snprintf(condition, sizeof(condition), "%d", m_opt->vm_id);
            res += get_rack_of_vdevice(condition, false, rack);

            res += lc_db_alloc_vlantag(
                    &vlantag_int, vl2id, atoi(rack), 0, false);
            res += lc_vmdb_vif_update_vlantag(vlantag_int, atoi(interface_id));

            res += add_vl2_tunnel_policies(interface_id);
            if (res) {
                LCPD_LOG(LOG_ERR,
                        "Config tunnel policy failed for vm %d vif %s\n",
                        m_opt->vm_id, interface_id);
            }
        }
    }

    if (_VL2_TRUE_ != res) {
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_opt->vm_id);
        memset(label, 0, VL2_MAX_DB_BUF_LEN);
        ext_vl2_db_lookup("vm", "label", condition, label);
        lc_lcpd_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                          m_opt->vm_id, label, "network config failed",
                          LC_SYSLOG_LEVEL_ERR);
        res = _VL2_FALSE_;
    }
    return res;
}

static int request_kernel_del_vm(struct lc_msg_head *m_head)
{
    struct msg_vm_opt *m_opt;
    char port_id[VL2_MAX_POTS_PI_LEN];
    char interface_id[VL2_MAX_POTS_ID_LEN];
    char network_id[VL2_MAX_POTS_NI_LEN];
    char condition[VL2_MAX_APP_CD_LEN];
    char label[VL2_MAX_DB_BUF_LEN];
    char state[VL2_MAX_POTS_PS_LEN];
    char vl2_type[VL2_MAX_POTS_PS_LEN];
    int res = 0, i;

    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    m_opt = (struct msg_vm_opt *)(m_head + 1);
    if (is_server_mode_vmware(LC_VM_TYPE_VM, m_opt->vm_id)) {
        LCPD_LOG(LOG_WARNING, "VMware VM will never arrive here.");
        return 0;
    }

    sprintf(port_id, "%u", m_opt->vm_id);

    res = vm_curl_vport_detached(m_opt->vm_id, 0);
    for (i = 0; i < LC_VM_IFACES_MAX; ++i) {
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN,
                "ifindex=%u AND deviceid=%u AND devicetype=%u",
                i, m_opt->vm_id, LC_VM_TYPE_VM);
        memset(interface_id, 0, VL2_MAX_POTS_ID_LEN);
        res = ext_vl2_db_lookup("vinterface", "id", condition, interface_id);
        memset(network_id, 0, VL2_MAX_POTS_NI_LEN);
        res = ext_vl2_db_lookup("vinterface", "subnetid", condition, network_id);
        memset(state, 0, VL2_MAX_POTS_PS_LEN);
        res = ext_vl2_db_lookup("vinterface", "state", condition, state);
        if (atoi(state) != LC_VIF_STATE_ATTACHED) {
            continue;
        }
        if (i == LC_VM_IFACE_SERV) {
            res += del_vl2_tunnel_policies(interface_id);
        } else {
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_APP_CD_LEN,
                    "id=%s", network_id);
            memset(vl2_type, 0, VL2_MAX_POTS_PS_LEN);
            ext_vl2_db_lookup(TABLE_VL2, TABLE_VL2_COLUMN_NET_TYPE,
                    condition, vl2_type);
            if (atoi(vl2_type) == VIF_TYPE_LAN ||
                    atoi(vl2_type) == VIF_TYPE_SERVICE) {
                res += del_vl2_tunnel_policies(interface_id);
            }
        }
    }

    if (res) {
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_opt->vm_id);
        memset(label, 0, VL2_MAX_DB_BUF_LEN);
        ext_vl2_db_lookup("vm", "label", condition, label);
        lc_lcpd_db_syslog(LC_SYSLOG_ACTION_DELETE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                          m_opt->vm_id, label, "network config failed",
                          LC_SYSLOG_LEVEL_ERR);
    }

    if (is_server_mode_xen(LC_VM_TYPE_VM, m_opt->vm_id)) {
        lc_msg_forward((lc_mbuf_t *)m_head);
    }
    return 0;
}

static int request_kernel_start_vm(struct lc_msg_head *m_head)
{
    struct msg_vm_opt *m_opt;
    char condition[VL2_MAX_APP_CD_LEN];
    char label[VL2_MAX_DB_BUF_LEN];
    char *acl_action;
    int vm_flag;
    int res = 0;

    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    m_opt = (struct msg_vm_opt *)(m_head + 1);
    if (is_server_mode_vmware(LC_VM_TYPE_VM, m_opt->vm_id)) {
        LCPD_LOG(LOG_WARNING, "VMware VM will never arrive here.");
        return 0;
    }

    sprintf(condition, "id=%u", m_opt->vm_id);
    res = ext_vl2_db_lookup_ids_v2("vm", "flag", condition, &vm_flag, 1);
    if (vm_flag & LC_VM_FLAG_ISOLATE) {
        acl_action = LCPD_API_VPORTS_ACL_ISOLATE_SWITCH_OPEN;
    } else {
        acl_action = LCPD_API_VPORTS_ACL_ISOLATE_SWITCH_CLOSE;
    }

    res = vm_curl_vport_attached(m_opt->vm_id, acl_action, 0);

    if (res) {
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_opt->vm_id);
        memset(label, 0, VL2_MAX_DB_BUF_LEN);
        ext_vl2_db_lookup("vm", "label", condition, label);
        lc_lcpd_db_syslog(LC_SYSLOG_ACTION_START_VM, LC_SYSLOG_OBJECTTYPE_VM,
                          m_opt->vm_id, label, "network config failed",
                          LC_SYSLOG_LEVEL_ERR);
    }
    return 0;
}

static int request_kernel_stop_vm(struct lc_msg_head *m_head)
{
    struct msg_vm_opt *m_opt;
    char condition[VL2_MAX_APP_CD_LEN];
    char label[VL2_MAX_DB_BUF_LEN];
    int res = 0;

    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    m_opt = (struct msg_vm_opt *)(m_head + 1);
    if (is_server_mode_vmware(LC_VM_TYPE_VM, m_opt->vm_id)) {
        LCPD_LOG(LOG_WARNING, "VMware VM will never arrive here.");
        return 0;
    }

    res = vm_curl_vport_detached(m_opt->vm_id, 0);

    if (res) {
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_opt->vm_id);
        memset(label, 0, VL2_MAX_DB_BUF_LEN);
        ext_vl2_db_lookup("vm", "label", condition, label);
        lc_lcpd_db_syslog(LC_SYSLOG_ACTION_STOP_VM, LC_SYSLOG_OBJECTTYPE_VM,
                          m_opt->vm_id, label, "network config failed",
                          LC_SYSLOG_LEVEL_ERR);
    }

    if (is_server_mode_xen(LC_VM_TYPE_VM, m_opt->vm_id)) {
        lc_msg_forward((lc_mbuf_t *)m_head);
    }
    return 0;
}


static int request_kernel_change_vm(struct lc_msg_head *m_head)
{
    struct msg_vm_opt *mvm;
    vm_modify_t  *pvm_modify = NULL;
    uint8_t lc_msg_buf[
        sizeof(struct lc_msg_head) + sizeof(struct msg_vm_opt) +
        sizeof(struct msg_vm_opt_start)] = {0};

    struct lc_msg_head * const m_forward = (struct lc_msg_head *)lc_msg_buf;
    struct msg_vm_opt * const m_opt = (struct msg_vm_opt *)(m_forward + 1);
    struct msg_vm_opt_start * const m_start = (struct msg_vm_opt_start *)(m_opt->data);
    char condition[VL2_MAX_APP_CD_LEN] = {0};
    char value[VL2_MAX_APP_CD_LEN] = {0};
    int res = 0;

    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    mvm = (struct msg_vm_opt *)(m_head + 1);
    if (is_server_mode_vmware(LC_VM_TYPE_VM, mvm->vm_id)) {
        LCPD_LOG(LOG_WARNING, "VMware VM will never arrive here.");
        return 0;
    }
    pvm_modify = (vm_modify_t *)mvm->data;

    memset(lc_msg_buf, 0, sizeof(lc_msg_buf));
    m_forward->magic = MSG_MG_END;
    m_forward->direction = -1; /* KER2KER */
    m_forward->length = sizeof(struct msg_vm_opt);

    memcpy(m_opt, mvm, sizeof(struct msg_vm_opt));

    snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", mvm->vm_id);
    res += ext_vl2_db_lookup("vm", "launch_server", condition, value);
    snprintf(m_start->vm_server, MAX_HOST_ADDRESS_LEN, "%s", value);

    /* stop vm (disable old policy), error is ok */
    m_forward->seq = 0;
    m_forward->type = LC_MSG_VM_STOP;
    request_kernel_stop_vm(m_forward);

    /* del vm (delete old policy) */

    m_forward->seq = 1;
    m_forward->type = LC_MSG_VM_DEL;
    res += request_kernel_del_vm(m_forward);

    /* update vm */
    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", mvm->vm_id);
    snprintf(value, VL2_MAX_APP_CD_LEN,
            "vl2id='%d'", pvm_modify->vl2id);
    res += ext_vl2_db_updates("vm", value, condition);
    snprintf(value, VL2_MAX_APP_CD_LEN,
            "vnetid='%d'", pvm_modify->vnetid);
    res += ext_vl2_db_updates("vm", value, condition);

    m_opt->vnet_id = pvm_modify->vnetid;
    m_opt->vl2_id = pvm_modify->vl2id;

    /* add vm (construct new policy) */
    m_forward->seq = 2;
    m_forward->type = LC_MSG_VM_ADD;
    res += request_kernel_add_vm(m_forward);

    /* start vm (enable new policy) */
    m_forward->seq = 3;
    m_forward->type = LC_MSG_VM_START;
    m_forward->length += sizeof(struct msg_vm_opt_start);
    res += request_kernel_start_vm(m_forward);

    return (res < 0 ? -1 : 0);
}

static int request_kernel_isolate_vm(struct lc_msg_head *m_head)
{
    struct msg_vm_opt *m_opt;
    char condition[VL2_MAX_APP_CD_LEN];
    char label[VL2_MAX_DB_BUF_LEN];
    int res    = 0;

    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    m_opt = (struct msg_vm_opt *)(m_head + 1);
    if (is_server_mode_vmware(LC_VM_TYPE_VM, m_opt->vm_id)) {
        LCPD_LOG(LOG_WARNING, "VMware VM will never arrive here.");
        return 0;
    }

    res = vm_curl_vport_attached(m_opt->vm_id, LCPD_API_VPORTS_ACL_ISOLATE_SWITCH_OPEN, 0);

    if (res) {
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_opt->vm_id);
        memset(label, 0, VL2_MAX_DB_BUF_LEN);
        ext_vl2_db_lookup("vm", "label", condition, label);
        lc_lcpd_db_syslog(LC_SYSLOG_ACTION_ISOLATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                          m_opt->vm_id, label, "network config failed",
                          LC_SYSLOG_LEVEL_ERR);
    }
    return 0;
}

static int request_kernel_release_vm(struct lc_msg_head *m_head)
{
    struct msg_vm_opt *m_opt;
    //struct msg_vm_opt_start *m_start;
    char condition[VL2_MAX_APP_CD_LEN];
    char label[VL2_MAX_DB_BUF_LEN];
    int res    = 0;

    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
             m_head->type, m_head->length);

    m_opt = (struct msg_vm_opt *)(m_head + 1);
    if (is_server_mode_vmware(LC_VM_TYPE_VM, m_opt->vm_id)) {
        LCPD_LOG(LOG_WARNING, "VMware VM will never arrive here.");
        return 0;
    }
    //m_start = (struct msg_vm_opt_start *)m_opt->data;

    res = vm_curl_vport_attached(m_opt->vm_id, LCPD_API_VPORTS_ACL_ISOLATE_SWITCH_CLOSE, 0);

    if (res) {
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_opt->vm_id);
        memset(label, 0, VL2_MAX_DB_BUF_LEN);
        ext_vl2_db_lookup("vm", "label", condition, label);
        lc_lcpd_db_syslog(LC_SYSLOG_ACTION_RELEASE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                          m_opt->vm_id, label, "network config failed",
                          LC_SYSLOG_LEVEL_ERR);
    }
    return 0;
}

static int request_kernel_migrate_vm(struct lc_msg_head *m_head)
{
    struct msg_vm_migrate_opt *m_migrate = NULL;
    uint8_t lc_msg_buf[
        sizeof(struct lc_msg_head) + sizeof(struct msg_vm_opt) +
        sizeof(struct msg_vm_opt_start)] = {0};

    struct lc_msg_head * const m_forward = (struct lc_msg_head *)lc_msg_buf;
    struct msg_vm_opt * const m_opt = (struct msg_vm_opt *)(m_forward + 1);
    struct msg_vm_opt_start * const m_start = (struct msg_vm_opt_start *)(m_opt->data);

    char url[LCMG_API_URL_SIZE] = {0};
    char result[LCMG_API_RESULT_SIZE] = {0};

    char condition[VL2_MAX_APP_CD_LEN];
    char value[VL2_MAX_APP_CD_LEN];
    char pool_id[VL2_MAX_POTS_ID_LEN];
    int res = 0;
    int flag = 0;

    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    m_migrate = (struct msg_vm_migrate_opt *)(m_head + 1);
    if (is_server_mode_vmware(LC_VM_TYPE_VM, m_migrate->vm_id)) {
        LCPD_LOG(LOG_WARNING, "VMware VM will never arrive here.");
        return 0;
    }

    /* do NOT return until launch_server & flag are updated */

    memset(lc_msg_buf, 0, sizeof(lc_msg_buf));
    m_forward->magic = MSG_MG_END;
    m_forward->direction = -1; /* KER2KER */
    m_forward->length = sizeof(struct msg_vm_opt);

    m_opt->vm_id = m_migrate->vm_id;
    snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_migrate->vm_id);
    res += ext_vl2_db_lookup("vm", "vnetid", condition, value);
    m_opt->vnet_id = atoi(value);
    res += ext_vl2_db_lookup("vm", "vl2id", condition, value);
    m_opt->vl2_id = atoi(value);

    res += ext_vl2_db_lookup("vm", "launch_server", condition, value);
    snprintf(m_start->vm_server, MAX_HOST_ADDRESS_LEN, "%s", value);

    if (lc_ker_knob->lc_opt.lc_flags & LC_FLAG_CONTROL_MODE_LCM) {

        /* stop vm (disable old policy), error is ok */

        m_forward->seq = 0;
        m_forward->type = LC_MSG_VM_STOP;
        request_kernel_stop_vm(m_forward);

        /* update launch_server and flag */

        res += ext_vl2_db_lookup("sys_configuration", "value",
                "param_name='lcm_ip'", value);
        snprintf(url, LCMG_API_URL_SIZE, LCMG_API_UPDATE_VM_LAUNCH_SERVER,
                value, m_migrate->vm_id, m_migrate->des_server);
        res += call_lcmg_api(url, result, LCMG_API_RESULT_SIZE);

        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_migrate->vm_id);
        snprintf(value, VL2_MAX_APP_CD_LEN,
                "launch_server='%s'", m_migrate->des_server);
        res += ext_vl2_db_updates("vm", value, condition);
        snprintf(m_start->vm_server, MAX_HOST_ADDRESS_LEN,
                "%s", m_migrate->des_server);

        res += ext_vl2_db_lookup("vm", "flag", condition, value);
        flag = atoi(value);
        flag &= (~LC_VM_MIGRATE_FLAG);
        snprintf(value, VL2_MAX_APP_CD_LEN, "%d", flag);
        res += ext_vl2_db_update("vm", "flag", value, condition);

        /* start vm (enable new policy) */

        m_forward->seq = 3;
        m_forward->type = LC_MSG_VM_START;
        res += request_kernel_start_vm(m_forward);

        LCPD_LOG(LOG_INFO, "In hierarchical mode, lcpd does NOT calculate tunnel policy.\n");

    } else {

        /* stop vm (disable old policy), error is ok */

        m_forward->seq = 0;
        m_forward->type = LC_MSG_VM_STOP;
        request_kernel_stop_vm(m_forward);

        /* del vm (delete old policy) */

        m_forward->seq = 1;
        m_forward->type = LC_MSG_VM_DEL;
        res += request_kernel_del_vm(m_forward);

        /* update launch_server */

        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_migrate->vm_id);
        snprintf(value, VL2_MAX_APP_CD_LEN,
                "launch_server='%s'", m_migrate->des_server);
        res += ext_vl2_db_updates("vm", value, condition);
        snprintf(m_start->vm_server, MAX_HOST_ADDRESS_LEN,
                "%s", m_migrate->des_server);

        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN, "ip='%s'", m_migrate->des_server);
        memset(pool_id, 0, VL2_MAX_POTS_ID_LEN);
        res += ext_vl2_db_lookup("host_device", "poolid", condition, pool_id);

        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_migrate->vm_id);
        memset(value, 0, VL2_MAX_APP_CD_LEN);
        snprintf(value, VL2_MAX_APP_CD_LEN, "poolid=%s", pool_id);
        res += ext_vl2_db_updates("vm", value, condition);

        /* add vm (construct new policy) */

        m_forward->seq = 2;
        m_forward->type = LC_MSG_VM_ADD;
        res += request_kernel_add_vm(m_forward);

        /* start vm (enable new policy) */

        m_forward->seq = 3;
        m_forward->type = LC_MSG_VM_START;
        res += request_kernel_start_vm(m_forward);

        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_migrate->vm_id);
        ext_vl2_db_lookup("vm", "label", condition, value);
        LCPD_LOG(LOG_INFO, "vm %s migrate to %s.\n", value, m_migrate->des_server);
    }

    return (res < 0 ? -1 : 0);
}

static int request_kernel_boot_or_down_vm(
        uint32_t vm_id, char *launch_server, bool is_boot)
{
    uint8_t lc_msg_buf[
        sizeof(struct lc_msg_head) + sizeof(struct msg_vm_opt) +
        sizeof(struct msg_vm_opt_start)] = {0};

    struct lc_msg_head * const m_forward = (struct lc_msg_head *)lc_msg_buf;
    struct msg_vm_opt * const m_opt = (struct msg_vm_opt *)(m_forward + 1);
    struct msg_vm_opt_start * const m_start = (struct msg_vm_opt_start *)(m_opt->data);

    char condition[VL2_MAX_APP_CD_LEN];
    char value[VL2_MAX_APP_CD_LEN];
    char label[VL2_MAX_DB_BUF_LEN];
    int vm_flag, res = 0;

    if (is_server_mode_vmware(LC_VM_TYPE_VM, vm_id)) {
        LCPD_LOG(LOG_WARNING, "VMware VM will never arrive here.");
        return 0;
    }

    if (is_server_mode_kvm(LC_VM_TYPE_VM, vm_id) ||
            is_server_mode_xen(LC_VM_TYPE_VM, vm_id)) {
        memset(lc_msg_buf, 0, sizeof(lc_msg_buf));
        m_forward->magic = MSG_MG_END; /* disable forward */
        m_forward->length = sizeof(struct msg_vm_opt);

        m_opt->vm_id = vm_id;
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%d", m_opt->vm_id);
        res += ext_vl2_db_lookup("vm", "flag", condition, value);
        vm_flag = atoi(value);

        res += ext_vl2_db_lookup("vm", "launch_server", condition, value);
        snprintf(m_start->vm_server, MAX_HOST_ADDRESS_LEN, "%s", value);

        /* start/stop vm (enable policy) */
        m_forward->seq = 0;
        if (is_boot) {
            m_forward->type = LC_MSG_VM_START;

            if (strcmp(m_start->vm_server, launch_server) != 0) {
                res += request_kernel_stop_vm(m_forward);
                res += request_kernel_del_vm(m_forward);

                snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", vm_id);
                snprintf(value, VL2_MAX_APP_CD_LEN,
                        "launch_server='%s'", launch_server);
                res += ext_vl2_db_updates("vm", value, condition);
                snprintf(m_start->vm_server, MAX_HOST_ADDRESS_LEN,
                        "%s", launch_server);

                lc_network_notify(vm_id, TALKER__RESOURCE_TYPE__VM_SERVER_RESOURCE,
                                                                    launch_server);
                res += request_kernel_add_vm(m_forward);
            }
            if (vm_flag & LC_VM_ISOLATE) {
                res += vm_curl_vport_attached(vm_id, LCPD_API_VPORTS_ACL_ISOLATE_SWITCH_OPEN, 1);
            } else {
                res += vm_curl_vport_attached(vm_id, LCPD_API_VPORTS_ACL_ISOLATE_SWITCH_NONE, 1);
            }
        } else {
            res += vm_curl_vport_detached(vm_id, 1);
        }

        m_forward->seq = 1;
    }

    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_opt->vm_id);
    memset(label, 0, VL2_MAX_DB_BUF_LEN);
    ext_vl2_db_lookup("vm", "label", condition, label);

    if (res) {
        snprintf(value, sizeof(value),
                "network config failed on %s", launch_server);
        if (is_boot) {
            lc_lcpd_db_syslog(LC_SYSLOG_ACTION_BOOT_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              m_opt->vm_id, label, value,
                              LC_SYSLOG_LEVEL_ERR);
        } else {
            lc_lcpd_db_syslog(LC_SYSLOG_ACTION_DOWN_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              m_opt->vm_id, label, value,
                              LC_SYSLOG_LEVEL_ERR);
        }
    } else {
        snprintf(value, sizeof(value),
                "network config successful on %s", launch_server);
        if (is_boot) {
            lc_lcpd_db_syslog(LC_SYSLOG_ACTION_BOOT_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              m_opt->vm_id, label, value,
                              LC_SYSLOG_LEVEL_INFO);
        } else {
            lc_lcpd_db_syslog(LC_SYSLOG_ACTION_DOWN_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              m_opt->vm_id, label, value,
                              LC_SYSLOG_LEVEL_INFO);
        }
    }

    return (res < 0 ? -1 : 0);
}

static int request_kernel_replace_vm(struct lc_msg_head *m_head)
{
    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    if (m_head->magic == MSG_MG_UI2KER) {
        return request_kernel_del_vm(m_head);
    } else {
        return request_kernel_add_vm(m_head);
    }
}

static int request_kernel_delete_vgateway(struct lc_msg_head *m_head, bool send_response)
{
    struct msg_vgateway_opt *m_opt;
    char url[LG_API_URL_SIZE] = {0};
    char launchserver[VL2_MAX_FWK_EP_LEN];
    char condition[VL2_MAX_APP_CD_LEN];
    char label[VL2_MAX_DB_BUF_LEN];
    char id_buf[MAX_LANS_NUM][MAX_ID_LEN];
    char *ids[MAX_LANS_NUM];
    int  i, res = 0;

    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    memset(id_buf, 0, sizeof(id_buf));
    for (i = 0; i < MAX_LANS_NUM; i++) {
        ids[i] = id_buf[i];
    }

    m_opt = (struct msg_vgateway_opt *)(m_head + 1);

    LCPD_LOG(LOG_INFO, "delete vgateway %d\n",
            m_opt->vnet_id);

    snprintf(condition, VL2_MAX_APP_CD_LEN,
            "devicetype=%d AND deviceid=%d AND iftype=%d AND state=%d",
            VIF_DEVICE_TYPE_VGATEWAY, m_opt->vnet_id,
            VIF_TYPE_LAN, VIF_STATE_ATTACHED);
    ext_vl2_db_lookups_v2(TABLE_VINTERFACE,
                          TABLE_VINTERFACE_COLUMN_ID,
                          condition, ids, MAX_LANS_NUM);
    for (i = 0; i < MAX_LANS_NUM && id_buf[i][0]; ++i) {
        res = del_vl2_tunnel_policies(id_buf[i]);
        if (res) {
            LCPD_LOG(LOG_ERR, "vgateway %d lanid %s config tunnel failed.",
                    m_opt->vnet_id, id_buf[i]);
            goto ret;
        }
    }

    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "%s='%d'",
           TABLE_VNET_COLUMN_ID, m_opt->vnet_id);
    res = ext_vl2_db_lookup(TABLE_VNET,
                            TABLE_VNET_COLUMN_LAUNCH_SERVER,
                            condition, launchserver);
    if (res) {
        LCPD_LOG(LOG_DEBUG, "delete vgateway %d failed, cannot find "
                "launch server", m_opt->vnet_id);
        goto ret;
    }

    snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_ROUTER "%d/",
            launchserver, LCPD_API_DEST_PORT,
            LCPD_API_VERSION, m_opt->vnet_id);
    LCPD_LOG(LOG_DEBUG, "delete vgateway url: [%s]", url);
    res = lcpd_call_api(url, API_METHOD_DEL, NULL, NULL);
    if (res) {
        LCPD_LOG(LOG_ERR, "vgateway delete call url delete router fail, "
                "res is %d", res);
        goto ret;
    }

ret:
    if (res) {
        if (send_response) {
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_opt->vnet_id);
            memset(label, 0, VL2_MAX_DB_BUF_LEN);
            ext_vl2_db_lookup("vnet", "label", condition, label);
            lc_lcpd_db_syslog(LC_SYSLOG_ACTION_DELETE_VGATEWAY,
                              LC_SYSLOG_OBJECTTYPE_VGATEWAY,
                              m_opt->vnet_id, label, "network config failed",
                              LC_SYSLOG_LEVEL_ERR);
            lc_lcpd_vgateway_msg_reply(m_opt->vnet_id, TALKER__RESULT__FAIL,
                         m_head->seq, TALKER__VGATEWAY_OPS__VGATEWAY_DELETE);
        }
        return -1;

    } else {
        if (send_response) {
            lc_lcpd_vgateway_msg_reply(
                m_opt->vnet_id, TALKER__RESULT__SUCCESS,
                m_head->seq, TALKER__VGATEWAY_OPS__VGATEWAY_DELETE);
        }
        return 0;
    }
}

static int update_vgateway_vif_mac_and_tunnel(
    char *launchserver, int vnet_id,
    vinterface_info_t *wansinfo, int nwans,
    vinterface_info_t *lansinfo, int nlans)
{
    char url[LG_API_URL_SIZE] = {0};
    char condition[VL2_MAX_APP_CD_LEN] = {0};
    char mac[VL2_MAX_APP_CD_LEN] = {0};
    char value[VL2_MAX_APP_CD_LEN] = {0};
    char post[LG_API_BIG_RESULT_SIZE] = {0};
    char ifindex[VL2_MAX_POTS_VI_LEN] = {0};
    int res = 0;
    int i;

    for (i = 0; i < nwans; ++i) {
        if (wansinfo[i].state != VIF_STATE_ATTACHED) {
            continue;
        }
        snprintf(condition, VL2_MAX_APP_CD_LEN,
                "devicetype=%d AND deviceid=%d AND ifindex=%u",
                VIF_DEVICE_TYPE_VGATEWAY, vnet_id, wansinfo[i].ifindex);
        memset(mac, 0, sizeof(mac));
        res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                                TABLE_VINTERFACE_COLUMN_MAC,
                                condition, mac);
        if (res == 0 && strlen(mac) == 17) {
            continue;
        }

        snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_ROUTER_WAN,
                launchserver, LCPD_API_DEST_PORT,
                LCPD_API_VERSION, vnet_id, wansinfo[i].ifindex);
        res = lcpd_call_api(url, API_METHOD_GET, NULL, post);
        res += parse_json_msg_get_mac(mac, post);
        if (res) {
            LCPD_LOG(LOG_ERR, "failed to get vgateway wan mac: [%s] [%s], "
                     "res=%d", url, post, res);
            return -1;
        }
        LCPD_LOG(LOG_DEBUG, "vgateway get wan mac: [%s] [%s], "
                 "res=%d, mac=%s", url, post, res, mac);

        snprintf(condition, VL2_MAX_APP_CD_LEN,
                "devicetype=%d AND deviceid=%d AND ifindex=%u",
                VIF_DEVICE_TYPE_VGATEWAY, vnet_id, wansinfo[i].ifindex);
        snprintf(value, VL2_MAX_APP_CD_LEN, "\"%s\"", mac);
        ext_vl2_db_update(TABLE_VINTERFACE, TABLE_VINTERFACE_COLUMN_MAC,
                          value, condition);
    }

    for (i = 0; i < nlans; ++i) {
        if (lansinfo[i].state != VIF_STATE_ATTACHED) {
            continue;
        }
        snprintf(condition, VL2_MAX_APP_CD_LEN,
                "devicetype=%d AND deviceid=%d AND ifindex=%u",
                VIF_DEVICE_TYPE_VGATEWAY, vnet_id, lansinfo[i].ifindex);
        memset(mac, 0, sizeof(mac));
        res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                                TABLE_VINTERFACE_COLUMN_MAC,
                                condition, mac);

        if (res != 0 || strlen(mac) != 17) {
            snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_ROUTER_LAN,
                    launchserver, LCPD_API_DEST_PORT,
                    LCPD_API_VERSION, vnet_id, lansinfo[i].ifindex);
            res = lcpd_call_api(url, API_METHOD_GET, NULL, post);
            res += parse_json_msg_get_mac(mac, post);
            if (res) {
                LCPD_LOG(LOG_ERR, "failed to get vgateway lan mac: [%s] [%s], "
                         "res=%d", url, post, res);
                return -1;
            }
            LCPD_LOG(LOG_DEBUG, "vgateway get lan mac: [%s] [%s], "
                     "res=%d, mac=%s", url, post, res, mac);

            snprintf(condition, VL2_MAX_APP_CD_LEN,
                    "devicetype=%d AND deviceid=%d AND ifindex=%u",
                    VIF_DEVICE_TYPE_VGATEWAY, vnet_id, lansinfo[i].ifindex);
            snprintf(value, VL2_MAX_APP_CD_LEN, "\"%s\"", mac);
            ext_vl2_db_update(TABLE_VINTERFACE, TABLE_VINTERFACE_COLUMN_MAC,
                              value, condition);
        }

        snprintf(condition, VL2_MAX_APP_CD_LEN,
                "devicetype=%d AND deviceid=%d AND ifindex=%u",
                VIF_DEVICE_TYPE_VGATEWAY, vnet_id, lansinfo[i].ifindex);
        res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                                TABLE_VINTERFACE_COLUMN_ID,
                                condition, ifindex);
        res += add_vl2_tunnel_policies(ifindex);
        if (res) {
            LCPD_LOG(LOG_ERR, "vgateway %d lanid %s config tunnel failed.",
                    vnet_id, ifindex);
            return -1;
        }
    }
    return 0;
}

static int request_kernel_change_vgateway(struct lc_msg_head *m_head, bool send_response)
{
    struct msg_vgateway_opt *m_opt;
    vinterface_info_t wansinfo[MAX_WANS_NUM];
    vinterface_info_t lansinfo[MAX_LANS_NUM];
    char label[VL2_MAX_DB_BUF_LEN];
    char url[LG_API_URL_SIZE] = {0};
    char ifindex[VL2_MAX_POTS_VI_LEN];
    char state[VL2_MAX_POTS_VI_LEN];
    char launchserver[VL2_MAX_POTS_SP_LEN];
    char subnetid[VL2_MAX_POTS_VI_LEN];
    char rack[VL2_MAX_RACK_ID_LEN];
    char view_id[VL2_MAX_POTS_VI_LEN];
    char bandwidth[VL2_MAX_INTERF_BW_LEN];
    char gateway[VL2_MAX_POTS_VI_LEN];
    char mac[LC_NAMESIZE];
    char condition[VL2_MAX_APP_CD_LEN];
    char *ips[VIF_MAX_IP_NUM] = {0};
    char *masks[VIF_MAX_IP_NUM] = {0};
    char ip_buf[VIF_MAX_IP_NUM][MAX_HOST_ADDRESS_LEN];
    char mask_buf[VIF_MAX_IP_NUM][MAX_HOST_ADDRESS_LEN];
    int  isp_id = 0, wanid = 0, lanid = 0, res = 0;
    int  wan_count = 0, lan_count = 0, j = 0;
    uint32_t masknum = 0;
    char post[LG_API_URL_SIZE_ROUTER] = {0};
    char wansid[LC_VGATEWAY_IDS_LEN] = {0};
    char lansid[LC_VGATEWAY_IDS_LEN] = {0};
    char *token = NULL;
    char *saveptr = NULL;
    uint32_t vlantag = 0, rackid = 0;
    char conn_max[VL2_MAX_POTS_VI_LEN];
    char new_conn_per_sec[VL2_MAX_POTS_VI_LEN];

    LCPD_LOG(LOG_INFO, "The vgateway message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    memset(wansinfo, 0, sizeof(wansinfo));
    memset(lansinfo, 0, sizeof(lansinfo));
    memset(ip_buf, 0, sizeof(ip_buf));
    memset(mask_buf, 0, sizeof(mask_buf));
    for (j = 0; j < VIF_MAX_IP_NUM; ++j) {
        ips[j] = ip_buf[j];
    }
    for (j = 0; j < VIF_MAX_IP_NUM; ++j) {
        masks[j] = mask_buf[j];
    }

    m_opt = (struct msg_vgateway_opt  *)(m_head + 1);
    LCPD_LOG(LOG_DEBUG, "vgateway %d update info "
            "wanslist is: %s and lanslist is %s.",
            m_opt->vnet_id, m_opt->wans, m_opt->lans);
    memcpy(wansid, m_opt->wans, LC_VGATEWAY_IDS_LEN);
    memcpy(lansid, m_opt->lans, LC_VGATEWAY_IDS_LEN);
    sprintf(view_id, "%u", m_opt->vnet_id);

    saveptr = NULL;
    for (wan_count = 0, token = strtok_r(wansid, "#", &saveptr);
        NULL != token; token = strtok_r(NULL, "#", &saveptr), wan_count++) {
        wanid = atoi(token);

        memset(ifindex, 0, sizeof(ifindex));
        memset(state, 0, sizeof(state));
        memset(bandwidth, 0, sizeof(bandwidth));
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", wanid);
        res = ext_vl2_db_lookup(TABLE_VINTERFACE ,
                                TABLE_VINTERFACE_COLUMN_IFINDEX,
                                condition, ifindex);
        res += ext_vl2_db_lookup(TABLE_VINTERFACE ,
                                 TABLE_VINTERFACE_COLUMN_STATE,
                                 condition, state);
        res += ext_vl2_db_lookup(TABLE_VINTERFACE ,
                                 TABLE_VINTERFACE_COLUMN_BANDW,
                                 condition, bandwidth);
        res += ext_vl2_db_lookup(TABLE_VINTERFACE ,
                                 TABLE_VINTERFACE_COLUMN_MAC,
                                 condition, mac);
        if (res) {
            LCPD_LOG(LOG_ERR, "vgateway %d wan %d (ifindex,state,bandw) not exist.",
                    m_opt->vnet_id, wanid);
            goto ret;
        }
        wansinfo[wan_count].ifindex = atoi(ifindex);
        wansinfo[wan_count].state = atoi(state);
        sscanf(bandwidth, "%lu", &wansinfo[wan_count].qos.minbandwidth);
        sscanf(bandwidth, "%lu", &wansinfo[wan_count].qos.maxbandwidth);
        memset(bandwidth, 0, sizeof(bandwidth));
        res = ext_vl2_db_lookup(TABLE_VINTERFACE ,
                                TABLE_VINTERFACE_COLUMN_BROADC_BANDW,
                                condition, bandwidth);
        if (res) {
            LCPD_LOG(LOG_ERR, "vgateway %d wan %d (broadcast_bandw) not exist.",
                    m_opt->vnet_id, wanid);
            goto ret;
        }
        sscanf(bandwidth, "%lu", &wansinfo[wan_count].broadcast_qos.minbandwidth);
        memset(bandwidth, 0, sizeof(bandwidth));
        res = ext_vl2_db_lookup(TABLE_VINTERFACE ,
                                TABLE_VINTERFACE_COLUMN_BROADC_CEIL_BANDW,
                                condition, bandwidth);
        if (res) {
            LCPD_LOG(LOG_ERR, "vgateway %d wan %d (broadcast_ceil_bandw) not exist.",
                    m_opt->vnet_id, wanid);
            goto ret;
        }
        sscanf(bandwidth, "%lu", &wansinfo[wan_count].broadcast_qos.maxbandwidth);
        if (mac[0]) {
            snprintf(wansinfo[wan_count].mac,
                    sizeof(wansinfo[wan_count].mac), "%s", mac);
        } else {
            snprintf(wansinfo[wan_count].mac,
                    sizeof(wansinfo[wan_count].mac), "00:00:00:00:00:00");
        }
        if (wansinfo[wan_count].state != VIF_STATE_ATTACHED) {
            continue;
        }

        memset(gateway, 0, sizeof(gateway));
        snprintf(condition, VL2_MAX_APP_CD_LEN, "%s=%d",
                TABLE_IP_RESOURCE_COLUMN_VIFID, wanid);
        res = ext_vl2_db_lookup(TABLE_IP_RESOURCE,
                                TABLE_IP_RESOURCE_COLUMN_GATEWAY,
                                condition, gateway);
        memcpy(wansinfo[wan_count].gateway, gateway, IP_LEN);

        LCPD_LOG(LOG_DEBUG, "wansinfo[%d].ifindex is: %d, "
                "wansinfo[%d].gateway is: %s, "
                "wansinfo[%d].qos.minbandwith is: %lu, "
                "wansinfo[%d].qos.maxbandwith is: %lu, wanid is: %d.\n",
                wan_count, wansinfo[wan_count].ifindex,
                wan_count, wansinfo[wan_count].gateway,
                wan_count, wansinfo[wan_count].qos.minbandwidth,
                wan_count, wansinfo[wan_count].qos.maxbandwidth, wanid);

        memset(ip_buf, 0, sizeof(ip_buf));
        memset(mask_buf, 0, sizeof(mask_buf));
        snprintf(condition, VL2_MAX_APP_CD_LEN, "%s=%d",
                TABLE_IP_RESOURCE_COLUMN_VIFID, wanid);
        res = ext_vl2_db_lookups_multi_v2(TABLE_IP_RESOURCE,
                                          TABLE_IP_RESOURCE_COLUMN_IP,
                                          condition, ips, VIF_MAX_IP_NUM);
        res += ext_vl2_db_lookups_multi_v2(TABLE_IP_RESOURCE,
                                           TABLE_IP_RESOURCE_COLUMN_MASK,
                                           condition, masks, VIF_MAX_IP_NUM);
        res += ext_vl2_db_lookup_ids_v2(TABLE_IP_RESOURCE,
                                        TABLE_IP_RESOURCE_COLUMN_ISPID,
                                        condition, &isp_id, 1);
        res += ext_vl2_db_lookup_ids_v2(TABLE_IP_RESOURCE,
                                        TABLE_IP_RESOURCE_COLUMN_VLANTAG,
                                        condition, (int*)&vlantag, 1);
        wansinfo[wan_count].ispid = isp_id;
        wansinfo[wan_count].vlantag = vlantag;
        for (j = 0; j < VIF_MAX_IP_NUM && ips[j] && ips[j][0]; ++j) {
            memcpy(wansinfo[wan_count].ips[j].address, ips[j], IP_LEN);
            masknum = (~0) << (32 - atoi(masks[j]));
            sprintf(wansinfo[wan_count].ips[j].netmask, "%u.%u.%u.%u",
                    (masknum >> 24) & 0xff, (masknum >> 16) & 0xff,
                    (masknum >> 8) & 0xff, masknum & 0xff);
            LCPD_LOG(LOG_DEBUG, "wansinfo[%d].isp_id is: %d, "
                "wansinfo[%d].ips[%d].address is: %s, "
                "wansinfo[%d].ips[%d].netmask is: %s, vlantag is: %d.\n",
                wan_count, wansinfo[wan_count].ispid,
                wan_count, j, wansinfo[wan_count].ips[j].address,
                wan_count, j, wansinfo[wan_count].ips[j].netmask,
                wansinfo[wan_count].vlantag);
        }
    }

    saveptr = NULL;
    for (lan_count = 0, token = strtok_r(lansid, "#", &saveptr);
        NULL != token; token = strtok_r(NULL, "#", &saveptr), lan_count++) {
        lanid = atoi(token);

        memset(ifindex, 0, sizeof(ifindex));
        memset(state, 0, sizeof(state));
        memset(subnetid, 0, sizeof(subnetid));
        memset(bandwidth, 0, sizeof(bandwidth));
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", lanid);
        res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                                TABLE_VINTERFACE_COLUMN_IFINDEX,
                                condition, ifindex);
        res += ext_vl2_db_lookup(TABLE_VINTERFACE,
                                 TABLE_VINTERFACE_COLUMN_STATE,
                                 condition, state);
        res += ext_vl2_db_lookup(TABLE_VINTERFACE,
                                 TABLE_VINTERFACE_COLUMN_SUBNETID,
                                 condition, subnetid);
        res += ext_vl2_db_lookup(TABLE_VINTERFACE,
                                 TABLE_VINTERFACE_COLUMN_BANDW,
                                 condition, bandwidth);
        ext_vl2_db_lookup(TABLE_VINTERFACE , TABLE_VINTERFACE_COLUMN_MAC,
                          condition, mac);
        if (res) {
            LCPD_LOG(LOG_ERR, "vgateway %d lan %d (ifindex,state,subnetid,bandw) not exist.",
                    m_opt->vnet_id, lanid);
            goto ret;
        }
        lansinfo[lan_count].ifindex = atoi(ifindex);
        lansinfo[lan_count].state = atoi(state);
        lansinfo[lan_count].qos.minbandwidth = 0;
        sscanf(bandwidth, "%lu", &lansinfo[lan_count].qos.maxbandwidth);
        if (mac[0]) {
            snprintf(lansinfo[lan_count].mac,
                    sizeof(lansinfo[lan_count].mac), "%s", mac);
        } else {
            snprintf(lansinfo[lan_count].mac,
                    sizeof(lansinfo[lan_count].mac), "00:00:00:00:00:00");
        }

        if (lansinfo[lan_count].state != VIF_STATE_ATTACHED) {
            res = del_vl2_tunnel_policies(token);
            if (res) {
                LCPD_LOG(LOG_ERR, "vgateway %d lanid %s config tunnel failed.",
                        m_opt->vnet_id, token);
                goto ret;
            }
            continue;
        }

        memset(rack, 0, sizeof(rack));
        if (get_rack_of_vdevice(view_id, true, rack) != _VL2_TRUE_ ||
            sscanf(rack, "%u", &rackid) != 1) {
            LCPD_LOG(LOG_ERR, "can not get rack of vgateway %d.",
                    m_opt->vnet_id);
            goto ret;
        }
        res = lc_db_alloc_vlantag(&vlantag, atoi(subnetid), rackid, 0, false);
        res += lc_vmdb_vif_update_vlantag(vlantag, lanid);
        if (res) {
            LCPD_LOG(LOG_ERR, "vgateway %d lanid %d alloc vlantag failed.",
                    m_opt->vnet_id, lanid);
            goto ret;
        }
        lansinfo[lan_count].vlantag = vlantag;

        LCPD_LOG(LOG_DEBUG, "lansinfo[%d].ifindex is: %d, "
                "lansinfo[%d].vlantag is: %d, "
                "lansinfo[%d].qos.minbandwidth is: %lu, "
                "lansinfo[%d].qos.maxbandwidth is: %lu, lanid is: %d.",
                lan_count, lansinfo[lan_count].ifindex,
                lan_count, lansinfo[lan_count].vlantag,
                lan_count, lansinfo[lan_count].qos.minbandwidth,
                lan_count, lansinfo[lan_count].qos.maxbandwidth, lanid);

        /* get ips info by lanid */
        memset(ip_buf, 0, sizeof(ip_buf));
        memset(mask_buf, 0, sizeof(mask_buf));
        snprintf(condition, VL2_MAX_APP_CD_LEN, "%s=%d",
                TABLE_VINTERFACEIP_COLUMN_VIFID, lanid);
        res = ext_vl2_db_lookups_multi_v2(TABLE_VINTERFACEIP,
                                          TABLE_VINTERFACEIP_COLUMN_IP,
                                          condition, ips, VIF_MAX_IP_NUM);
        res += ext_vl2_db_lookups_multi_v2(TABLE_VINTERFACEIP,
                                           TABLE_VINTERFACEIP_COLUMN_NETMASK,
                                           condition, masks, VIF_MAX_IP_NUM);
        for (j = 0; j < VIF_MAX_IP_NUM && ips[j] && ips[j][0]; ++j) {
            memcpy(lansinfo[lan_count].ips[j].address, ips[j],
                    MAX_HOST_ADDRESS_LEN);
            memcpy(lansinfo[lan_count].ips[j].netmask, masks[j],
                    MAX_HOST_ADDRESS_LEN);

            LCPD_LOG(LOG_DEBUG, "lansinfo[%d].ips[%d].address is: %s, "
                "lansinfo[%d].ips[%d].netmask is: %s, "
                "masks[%d] is: %s, sizeof is: %zd, "
                "ips[%d] is: %s.",
                lan_count, j, lansinfo[lan_count].ips[j].address,
                lan_count, j, lansinfo[lan_count].ips[j].netmask,
                j, masks[j], sizeof(masks[j]), j, ips[j]);
        }
    }

    /* generate url info */

    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "%s=%d",
             TABLE_VNET_COLUMN_ID, m_opt->vnet_id);
    res = ext_vl2_db_lookup(TABLE_VNET, TABLE_VNET_COLUMN_LAUNCH_SERVER,
                            condition, launchserver);
    if (res) {
        LCPD_LOG(LOG_ERR, "vgateway %d launch server not exist.",
                m_opt->vnet_id);
        goto ret;
    }
    res = ext_vl2_db_lookup(TABLE_VNET, TABLE_VNET_COLUMN_CONN_MAX,
                            condition, conn_max);
    if (res) {
        LCPD_LOG(LOG_ERR, "vgateway %d conn_max not exist.",
                m_opt->vnet_id);
        goto ret;
    }
    res = ext_vl2_db_lookup(TABLE_VNET, TABLE_VNET_COLUMN_NEW_CONN_PER_SEC,
                            condition, new_conn_per_sec);
    if (res) {
        LCPD_LOG(LOG_ERR, "vgateway %d new_conn_per_sec not exist.",
                m_opt->vnet_id);
        goto ret;
    }

    snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_ROUTER,
            launchserver, LCPD_API_DEST_PORT, LCPD_API_VERSION);
    pack_vgateway_update(post, LG_API_URL_SIZE_ROUTER,
                         view_id, wan_count, wansinfo, lan_count, lansinfo,
                         atoi(conn_max), atoi(new_conn_per_sec), LC_ROLE_GATEWAY);
    LCPD_LOG(LOG_DEBUG,
            "vgateway update url is: [%s] post is: [%s]",
            url, post);
    res = lcpd_call_api(url, API_METHOD_POST, post, NULL);
    if (res) {
        LCPD_LOG(LOG_ERR, "vgateway update call url return is %d",
                 res);
        goto ret;
    }

    res = update_vgateway_vif_mac_and_tunnel(launchserver, m_opt->vnet_id,
            wansinfo, wan_count, lansinfo, lan_count);
    if (res) {
        LCPD_LOG(LOG_ERR, "vgateway update mac and tunnel policy failed.");
        goto ret;
    }

ret:
    if (res) {
        if (send_response) {
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_opt->vnet_id);
            memset(label, 0, VL2_MAX_DB_BUF_LEN);
            ext_vl2_db_lookup("vnet", "label", condition, label);
            lc_lcpd_db_syslog(LC_SYSLOG_ACTION_DELETE_VGATEWAY,
                              LC_SYSLOG_OBJECTTYPE_VGATEWAY,
                              m_opt->vnet_id, label, "network config failed",
                              LC_SYSLOG_LEVEL_ERR);
            lc_lcpd_vgateway_msg_reply(m_opt->vnet_id, TALKER__RESULT__FAIL,
                         m_head->seq, TALKER__VGATEWAY_OPS__VGATEWAY_MODIFY);
        }
        return -1;
    } else {
        if (send_response) {
            lc_lcpd_vgateway_msg_reply(m_opt->vnet_id, TALKER__RESULT__SUCCESS,
                              m_head->seq, TALKER__VGATEWAY_OPS__VGATEWAY_MODIFY);
        }
        return 0;
    }
}

int request_kernel_migrate_vgateway (struct lc_msg_head *m_head)
{
    struct msg_vgateway_opt *m_opt;
    char condition[VL2_MAX_APP_CD_LEN];
    char *wan_ids[MAX_WANS_NUM] = {0};
    char *lan_ids[MAX_LANS_NUM] = {0};
    char wan_ids_buf[MAX_WANS_NUM][VL2_MAX_POTS_ID_LEN];
    char lan_ids_buf[MAX_LANS_NUM][VL2_MAX_POTS_ID_LEN];
    int i = 0, j = 0, res = 0;
    lc_mbuf_t m_forward;
    struct msg_vgateway_opt *m_forward_opt = (struct msg_vgateway_opt *)m_forward.data;
    char value[VL2_MAX_APP_CD_LEN];
    char label[VL2_MAX_DB_BUF_LEN];
    char curr_launch_server[VL2_MAX_POTS_SP_LEN] = {0};

    LCPD_LOG(LOG_INFO, "The vgateway message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    m_opt = (struct msg_vgateway_opt  *)(m_head + 1);
    LCPD_LOG(LOG_DEBUG, "vgateway migrate info "
            "id is %d, and target gw_launch_server is %s.\n",
            m_opt->vnet_id, m_opt->gw_launch_server);

    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "%s=%u",
             TABLE_VNET_COLUMN_ID, m_opt->vnet_id);
    ext_vl2_db_lookup("vnet", "gw_launch_server", condition, curr_launch_server);

    if (0 != strcmp(curr_launch_server, m_opt->gw_launch_server)) {
        // STEP 1: delete
        memset(&m_forward, 0, sizeof(m_forward));
        memcpy(&m_forward, m_head, sizeof(struct lc_msg_head));
        m_forward_opt->vnet_id = m_opt->vnet_id;
        if (request_kernel_delete_vgateway(&(m_forward.hdr), false)) {
            LCPD_LOG(LOG_ERR, "delete vgateway %d in old server failed, "
                    "but will still create it in new server (%s).\n",
                    m_opt->vnet_id, m_opt->gw_launch_server);
        } else {
            LCPD_LOG(LOG_INFO, "delete vgateway %d in old server succeed, "
                    "will create it in new server (%s).\n",
                    m_opt->vnet_id, m_opt->gw_launch_server);
        }
        // SETP 2: modify gw_launch_server

        snprintf(condition, VL2_MAX_APP_CD_LEN, "%s=%u",
                 TABLE_VNET_COLUMN_ID, m_opt->vnet_id);
        sprintf(value, "'%s'", m_opt->gw_launch_server);
        res = ext_vl2_db_update(TABLE_VNET, TABLE_VNET_COLUMN_LAUNCH_SERVER,
                                value, condition);
    }

    // STEP 3: recreate

    memset(&m_forward, 0, sizeof(m_forward));
    memcpy(&m_forward, m_head, sizeof(struct lc_msg_head));
    m_forward_opt->vnet_id = m_opt->vnet_id;

    memset(wan_ids_buf, 0, sizeof(wan_ids_buf));
    for (i = 0; i < MAX_WANS_NUM; i++) {
        wan_ids[i] = wan_ids_buf[i];
    }
    memset(lan_ids_buf, 0, sizeof(lan_ids_buf));
    for (i = 0; i < MAX_LANS_NUM; i++) {
        lan_ids[i] = lan_ids_buf[i];
    }

    snprintf(condition, VL2_MAX_APP_CD_LEN, "%s='%d' and %s='%d' and %s='%d' and %s='%d'",
             TABLE_VINTERFACE_COLUMN_DEVICETYPE, VIF_DEVICE_TYPE_VGATEWAY,
             TABLE_VINTERFACE_COLUMN_DEVICEID, m_opt->vnet_id,
             TABLE_VINTERFACE_COLUMN_IFTYPE, VIF_TYPE_WAN,
             TABLE_VINTERFACE_COLUMN_STATE, VIF_STATE_ATTACHED);
    res = ext_vl2_db_lookups_v2(TABLE_VINTERFACE, TABLE_VINTERFACE_COLUMN_ID,
                                condition, wan_ids, MAX_WANS_NUM);
    for (i = j = 0; i < MAX_WANS_NUM && wan_ids[i][0]; ++i) {
        j += snprintf(m_forward_opt->wans + j, LC_VGATEWAY_IDS_LEN - j, "%s#", wan_ids[i]);
    }

    snprintf(condition, VL2_MAX_APP_CD_LEN, "%s='%d' and %s='%d' and %s='%d' and %s='%d'",
             TABLE_VINTERFACE_COLUMN_DEVICETYPE, VIF_DEVICE_TYPE_VGATEWAY,
             TABLE_VINTERFACE_COLUMN_DEVICEID, m_opt->vnet_id,
             TABLE_VINTERFACE_COLUMN_IFTYPE, VIF_TYPE_LAN,
             TABLE_VINTERFACE_COLUMN_STATE, VIF_STATE_ATTACHED);
    res = ext_vl2_db_lookups_v2(TABLE_VINTERFACE, TABLE_VINTERFACE_COLUMN_ID,
                                condition, lan_ids, MAX_LANS_NUM);
    for (i = j = 0; i < MAX_LANS_NUM && lan_ids[i][0]; ++i) {
        j += snprintf(m_forward_opt->lans + j, LC_VGATEWAY_IDS_LEN - j, "%s#", lan_ids[i]);
    }

    res = request_kernel_change_vgateway(&(m_forward.hdr), false);
    if (res) {
        LCPD_LOG(LOG_ERR, "recreate vgateway %d in %s failed.",
                m_opt->vnet_id, m_opt->gw_launch_server);
        goto ret;
    } else {
        LCPD_LOG(LOG_INFO, "recreate vgateway %d in %s succeed.",
                m_opt->vnet_id, m_opt->gw_launch_server);
    }

ret:
    if (res) {
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_opt->vnet_id);
        memset(label, 0, VL2_MAX_DB_BUF_LEN);
        ext_vl2_db_lookup("vnet", "label", condition, label);
        lc_lcpd_db_syslog(LC_SYSLOG_ACTION_DELETE_VGATEWAY,
                          LC_SYSLOG_OBJECTTYPE_VGATEWAY,
                          m_opt->vnet_id, label, "network config failed",
                          LC_SYSLOG_LEVEL_ERR);
        lc_lcpd_vgateway_msg_reply(m_opt->vnet_id, TALKER__RESULT__FAIL,
                                   m_head->seq,
                                   TALKER__VGATEWAY_OPS__VGATEWAY_MIGRATE);
        return -1;

    } else {
        lc_lcpd_vgateway_msg_reply(m_opt->vnet_id, TALKER__RESULT__SUCCESS,
                                   m_head->seq,
                                   TALKER__VGATEWAY_OPS__VGATEWAY_MIGRATE);
        return 0;
    }
}

static int request_kernel_delete_valve(struct lc_msg_head *m_head, bool send_response)
{
    struct msg_valve_opt *m_opt;
    char url[LG_API_URL_SIZE] = {0};
    char launchserver[VL2_MAX_FWK_EP_LEN];
    char condition[VL2_MAX_APP_CD_LEN];
    char label[VL2_MAX_DB_BUF_LEN];
    char id_buf[MAX_LANS_NUM][MAX_ID_LEN];
    char *ids[MAX_LANS_NUM];
    int  i, res = 0;

    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    memset(id_buf, 0, sizeof(id_buf));
    for (i = 0; i < MAX_LANS_NUM; i++) {
        ids[i] = id_buf[i];
    }

    m_opt = (struct msg_valve_opt *)(m_head + 1);

    LCPD_LOG(LOG_INFO, "delete valve %d\n",
            m_opt->vnet_id);

    snprintf(condition, VL2_MAX_APP_CD_LEN,
            "devicetype=%d AND deviceid=%d AND iftype=%d AND state=%d",
            VIF_DEVICE_TYPE_VGATEWAY, m_opt->vnet_id,
            VIF_TYPE_LAN, VIF_STATE_ATTACHED);
    ext_vl2_db_lookups_v2(TABLE_VINTERFACE,
                          TABLE_VINTERFACE_COLUMN_ID,
                          condition, ids, MAX_LANS_NUM);
    for (i = 0; i < MAX_LANS_NUM && id_buf[i][0]; ++i) {
        res = del_vl2_tunnel_policies(id_buf[i]);
        if (res) {
            LCPD_LOG(LOG_ERR, "valve %d lanid %s config tunnel failed.",
                    m_opt->vnet_id, id_buf[i]);
            goto ret;
        }
    }

    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "%s='%d'",
           TABLE_VNET_COLUMN_ID, m_opt->vnet_id);
    res = ext_vl2_db_lookup(TABLE_VNET,
                            TABLE_VNET_COLUMN_LAUNCH_SERVER,
                            condition, launchserver);
    if (res) {
        LCPD_LOG(LOG_DEBUG, "delete valve %d failed, cannot find "
                "launch server", m_opt->vnet_id);
        goto ret;
    }

    snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_VALVE "%d/",
            launchserver, LCPD_API_DEST_PORT,
            LCPD_API_VERSION, m_opt->vnet_id);
    LCPD_LOG(LOG_DEBUG, "delete valve url: [%s]", url);
    res = lcpd_call_api(url, API_METHOD_DEL, NULL, NULL);
    if (res) {
        LCPD_LOG(LOG_ERR, "valve delete call url delete valve fail, "
                "res is %d", res);
        goto ret;
    }

ret:
    if (res) {
        if (send_response) {
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_opt->vnet_id);
            memset(label, 0, VL2_MAX_DB_BUF_LEN);
            ext_vl2_db_lookup("vnet", "label", condition, label);
            lc_lcpd_db_syslog(LC_SYSLOG_ACTION_DELETE_VALVE,
                              LC_SYSLOG_OBJECTTYPE_VALVE,
                              m_opt->vnet_id, label, "network config failed",
                              LC_SYSLOG_LEVEL_ERR);
            lc_lcpd_valve_msg_reply(m_opt->vnet_id, TALKER__RESULT__FAIL,
                         m_head->seq, TALKER__VALVE_OPS__VALVE_DELETE);
        }
        return -1;

    } else {
        if (send_response) {
            lc_lcpd_valve_msg_reply(
                m_opt->vnet_id, TALKER__RESULT__SUCCESS,
                m_head->seq, TALKER__VALVE_OPS__VALVE_DELETE);
        }
        return 0;
    }
}

static int update_valve_vif_mac_and_tunnel(
    char *launchserver, int vnet_id,
    vinterface_info_t *wansinfo, int nwans,
    vinterface_info_t *lansinfo, int nlans)
{
    char url[LG_API_URL_SIZE] = {0};
    char condition[VL2_MAX_APP_CD_LEN] = {0};
    char mac[VL2_MAX_APP_CD_LEN] = {0};
    char value[VL2_MAX_APP_CD_LEN] = {0};
    char post[LG_API_BIG_RESULT_SIZE] = {0};
    char ifindex[VL2_MAX_POTS_VI_LEN] = {0};
    int res = 0;
    int i;

    /* Disable mac update for wan port due to the duplicate mac address
     * of the single port in valve */
#if 0
    for (i = 0; i < nwans; ++i) {
        if (wansinfo[i].state != VIF_STATE_ATTACHED) {
            continue;
        }
        snprintf(condition, VL2_MAX_APP_CD_LEN,
                "devicetype=%d AND deviceid=%d AND ifindex=%u",
                VIF_DEVICE_TYPE_VGATEWAY, vnet_id, wansinfo[i].ifindex);
        memset(mac, 0, sizeof(mac));
        res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                                TABLE_VINTERFACE_COLUMN_MAC,
                                condition, mac);
        if (res == 0 && strlen(mac) == 17) {
            continue;
        }

        snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_VALVE_WAN,
                launchserver, LCPD_API_DEST_PORT,
                LCPD_API_VERSION, vnet_id, wansinfo[i].ifindex);
        res = lcpd_call_api(url, API_METHOD_GET, NULL, post);
        res += parse_json_msg_get_mac(mac, post);
        if (res) {
            LCPD_LOG(LOG_ERR, "failed to get valve wan mac: [%s] [%s], "
                     "res=%d", url, post, res);
            return -1;
        }
        LCPD_LOG(LOG_DEBUG, "valve get wan mac: [%s] [%s], "
                 "res=%d, mac=%s", url, post, res, mac);

        snprintf(condition, VL2_MAX_APP_CD_LEN,
                "devicetype=%d AND deviceid=%d AND ifindex=%u",
                VIF_DEVICE_TYPE_VGATEWAY, vnet_id, wansinfo[i].ifindex);
        snprintf(value, VL2_MAX_APP_CD_LEN, "\"%s\"", mac);
        ext_vl2_db_update(TABLE_VINTERFACE, TABLE_VINTERFACE_COLUMN_MAC,
                          value, condition);
    }
#endif

    for (i = 0; i < nlans; ++i) {
        if (lansinfo[i].state != VIF_STATE_ATTACHED) {
            continue;
        }
        snprintf(condition, VL2_MAX_APP_CD_LEN,
                "devicetype=%d AND deviceid=%d AND ifindex=%u",
                VIF_DEVICE_TYPE_VGATEWAY, vnet_id, lansinfo[i].ifindex);
        memset(mac, 0, sizeof(mac));
        res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                                TABLE_VINTERFACE_COLUMN_MAC,
                                condition, mac);

        if (res != 0 || strlen(mac) != 17) {
            snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_VALVE_LAN,
                    launchserver, LCPD_API_DEST_PORT,
                    LCPD_API_VERSION, vnet_id, lansinfo[i].ifindex);
            res = lcpd_call_api(url, API_METHOD_GET, NULL, post);
            res += parse_json_msg_get_mac(mac, post);
            if (res) {
                LCPD_LOG(LOG_ERR, "failed to get valve lan mac: [%s] [%s], "
                         "res=%d", url, post, res);
                return -1;
            }
            LCPD_LOG(LOG_DEBUG, "valve get lan mac: [%s] [%s], "
                     "res=%d, mac=%s", url, post, res, mac);

            snprintf(condition, VL2_MAX_APP_CD_LEN,
                    "devicetype=%d AND deviceid=%d AND ifindex=%u",
                    VIF_DEVICE_TYPE_VGATEWAY, vnet_id, lansinfo[i].ifindex);
            snprintf(value, VL2_MAX_APP_CD_LEN, "\"%s\"", mac);
            ext_vl2_db_update(TABLE_VINTERFACE, TABLE_VINTERFACE_COLUMN_MAC,
                              value, condition);
        }

        snprintf(condition, VL2_MAX_APP_CD_LEN,
                "devicetype=%d AND deviceid=%d AND ifindex=%u",
                VIF_DEVICE_TYPE_VGATEWAY, vnet_id, lansinfo[i].ifindex);
        res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                                TABLE_VINTERFACE_COLUMN_ID,
                                condition, ifindex);
        res += add_vl2_tunnel_policies(ifindex);
        if (res) {
            LCPD_LOG(LOG_ERR, "valve %d lanid %s config tunnel failed.",
                    vnet_id, ifindex);
            return -1;
        }
    }
    return 0;
}

static int request_kernel_change_valve(struct lc_msg_head *m_head, bool send_response)
{
    struct msg_valve_opt *m_opt;
    vinterface_info_t wansinfo[MAX_WANS_NUM];
    vinterface_info_t lansinfo[MAX_LANS_NUM];
    char label[VL2_MAX_DB_BUF_LEN];
    char url[LG_API_URL_SIZE] = {0};
    char ifindex[VL2_MAX_POTS_VI_LEN];
    char state[VL2_MAX_POTS_VI_LEN];
    char launchserver[VL2_MAX_POTS_SP_LEN];
    char subnetid[VL2_MAX_POTS_VI_LEN];
    char rack[VL2_MAX_RACK_ID_LEN];
    char view_id[VL2_MAX_POTS_VI_LEN];
    char bandwidth[VL2_MAX_INTERF_BW_LEN];
    char ceil_bandwidth[VL2_MAX_POTS_VI_LEN];
    char gateway[VL2_MAX_POTS_VI_LEN];
    char mac[LC_NAMESIZE];
    char condition[VL2_MAX_APP_CD_LEN];
    char *ips[VIF_MAX_IP_NUM] = {0};
    char *masks[VIF_MAX_IP_NUM] = {0};
    char ip_buf[VIF_MAX_IP_NUM][MAX_HOST_ADDRESS_LEN];
    char mask_buf[VIF_MAX_IP_NUM][MAX_HOST_ADDRESS_LEN];
    int  isp_id = 0, wanid = 0, lanid = 0, res = 0;
    int  wan_count = 0, lan_count = 0, j = 0;
    int  mark_avg = 1, used_wan_number = 0;
    uint64_t bandwidth_int = 0;
    uint32_t masknum = 0;
    char post[LG_API_URL_SIZE_VALVE] = {0};
    char wansid[LC_VALVE_IDS_LEN] = {0};
    char lansid[LC_VALVE_IDS_LEN] = {0};
    char *token = NULL;
    char *saveptr = NULL;
    uint32_t vlantag = 0, rackid = 0;

    LCPD_LOG(LOG_INFO, "The valve message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    memset(wansinfo, 0, sizeof(wansinfo));
    memset(lansinfo, 0, sizeof(lansinfo));
    memset(ip_buf, 0, sizeof(ip_buf));
    memset(mask_buf, 0, sizeof(mask_buf));
    for (j = 0; j < VIF_MAX_IP_NUM; ++j) {
        ips[j] = ip_buf[j];
    }
    for (j = 0; j < VIF_MAX_IP_NUM; ++j) {
        masks[j] = mask_buf[j];
    }

    m_opt = (struct msg_valve_opt  *)(m_head + 1);
    LCPD_LOG(LOG_DEBUG, "valve %d update info "
            "wanslist is: %s and lanslist is %s.",
            m_opt->vnet_id, m_opt->wans, m_opt->lans);
    memcpy(wansid, m_opt->wans, LC_VALVE_IDS_LEN);
    memcpy(lansid, m_opt->lans, LC_VALVE_IDS_LEN);
    sprintf(view_id, "%u", m_opt->vnet_id);

    saveptr = NULL;
    for (wan_count = 0, token = strtok_r(wansid, "#", &saveptr);
        NULL != token; token = strtok_r(NULL, "#", &saveptr), wan_count++) {
        wanid = atoi(token);

        memset(state, 0, sizeof(state));
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", wanid);
        res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                                TABLE_VINTERFACE_COLUMN_STATE,
                                condition, state);
        if (atoi(state) != VIF_STATE_ATTACHED) {
            continue;
        }

        memset(bandwidth, 0, sizeof(bandwidth));
        res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                                TABLE_VINTERFACE_COLUMN_BANDW,
                                condition, bandwidth);
        sscanf(bandwidth, "%lu", &bandwidth_int);
        used_wan_number += 1;
        if (bandwidth_int > 0) {
            mark_avg = 0;
        }
    }

    memcpy(wansid, m_opt->wans, LC_VALVE_IDS_LEN);
    saveptr = NULL;
    for (wan_count = 0, token = strtok_r(wansid, "#", &saveptr);
        NULL != token; token = strtok_r(NULL, "#", &saveptr), wan_count++) {
        wanid = atoi(token);

        memset(ifindex, 0, sizeof(ifindex));
        memset(state, 0, sizeof(state));
        memset(bandwidth, 0, sizeof(bandwidth));
        memset(ceil_bandwidth, 0, sizeof(ceil_bandwidth));
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", wanid);
        res += ext_vl2_db_lookup(TABLE_VINTERFACE,
                                 TABLE_VINTERFACE_COLUMN_IFINDEX,
                                 condition, ifindex);
        res += ext_vl2_db_lookup(TABLE_VINTERFACE,
                                 TABLE_VINTERFACE_COLUMN_STATE,
                                 condition, state);
        res += ext_vl2_db_lookup(TABLE_VINTERFACE,
                                 TABLE_VINTERFACE_COLUMN_BANDW,
                                 condition, bandwidth);
        res += ext_vl2_db_lookup(TABLE_VINTERFACE,
                                 TABLE_VINTERFACE_COLUMN_CEIL_BANDW,
                                 condition, ceil_bandwidth);
        ext_vl2_db_lookup(TABLE_VINTERFACE , TABLE_VINTERFACE_COLUMN_MAC,
                          condition, mac);
        if (res) {
            LCPD_LOG(LOG_ERR, "valve %d wan %d (ifindex,state,bandw) not exist.",
                    m_opt->vnet_id, wanid);
            goto ret;
        }
        wansinfo[wan_count].ifindex = atoi(ifindex);
        wansinfo[wan_count].state = atoi(state);
        sscanf(ceil_bandwidth, "%lu", &wansinfo[wan_count].qos.maxbandwidth);
        if (mark_avg && used_wan_number > 0) {
            wansinfo[wan_count].qos.minbandwidth =
                wansinfo[wan_count].qos.maxbandwidth / used_wan_number;
        } else {
            sscanf(bandwidth, "%lu", &wansinfo[wan_count].qos.minbandwidth);
        }
        memset(bandwidth, 0, sizeof(bandwidth));
        res = ext_vl2_db_lookup(TABLE_VINTERFACE ,
                                TABLE_VINTERFACE_COLUMN_BROADC_BANDW,
                                condition, bandwidth);
        if (res) {
            LCPD_LOG(LOG_ERR, "valve %d wan %d (broadcast_bandw) not exist.",
                    m_opt->vnet_id, wanid);
            goto ret;
        }
        sscanf(bandwidth, "%lu", &wansinfo[wan_count].broadcast_qos.minbandwidth);
        memset(bandwidth, 0, sizeof(bandwidth));
        res = ext_vl2_db_lookup(TABLE_VINTERFACE ,
                                TABLE_VINTERFACE_COLUMN_BROADC_CEIL_BANDW,
                                condition, bandwidth);
        if (res) {
            LCPD_LOG(LOG_ERR, "valve %d wan %d (broadcast_ceil_bandw) not exist.",
                    m_opt->vnet_id, wanid);
            goto ret;
        }
        sscanf(bandwidth, "%lu", &wansinfo[wan_count].broadcast_qos.maxbandwidth);
        if (mac[0]) {
            snprintf(wansinfo[wan_count].mac,
                    sizeof(wansinfo[wan_count].mac), "%s", mac);
        } else {
            snprintf(wansinfo[wan_count].mac,
                    sizeof(wansinfo[wan_count].mac), "00:00:00:00:00:00");
        }
        if (wansinfo[wan_count].state != VIF_STATE_ATTACHED) {
            continue;
        }

        memset(gateway, 0, sizeof(gateway));
        snprintf(condition, VL2_MAX_APP_CD_LEN, "%s=%d",
                TABLE_IP_RESOURCE_COLUMN_VIFID, wanid);
        res = ext_vl2_db_lookup(TABLE_IP_RESOURCE,
                                TABLE_IP_RESOURCE_COLUMN_GATEWAY,
                                condition, gateway);
        memcpy(wansinfo[wan_count].gateway, gateway, IP_LEN);

        LCPD_LOG(LOG_DEBUG, "wansinfo[%d].ifindex is: %d, "
                "wansinfo[%d].gateway is: %s, "
                "wansinfo[%d].qos.minbandwith is: %lu, "
                "wansinfo[%d].qos.maxbandwith(ceil) is: %lu, wanid is: %d.\n",
                wan_count, wansinfo[wan_count].ifindex,
                wan_count, wansinfo[wan_count].gateway,
                wan_count, wansinfo[wan_count].qos.minbandwidth,
                wan_count, wansinfo[wan_count].qos.maxbandwidth, wanid);

        memset(ip_buf, 0, sizeof(ip_buf));
        memset(mask_buf, 0, sizeof(mask_buf));
        snprintf(condition, VL2_MAX_APP_CD_LEN, "%s=%d",
                TABLE_IP_RESOURCE_COLUMN_VIFID, wanid);
        res = ext_vl2_db_lookups_multi_v2(TABLE_IP_RESOURCE,
                                          TABLE_IP_RESOURCE_COLUMN_IP,
                                          condition, ips, VIF_MAX_IP_NUM);
        res += ext_vl2_db_lookups_multi_v2(TABLE_IP_RESOURCE,
                                           TABLE_IP_RESOURCE_COLUMN_MASK,
                                           condition, masks, VIF_MAX_IP_NUM);
        res += ext_vl2_db_lookup_ids_v2(TABLE_IP_RESOURCE,
                                        TABLE_IP_RESOURCE_COLUMN_ISPID,
                                        condition, &isp_id, 1);
        res += ext_vl2_db_lookup_ids_v2(TABLE_IP_RESOURCE,
                                        TABLE_IP_RESOURCE_COLUMN_VLANTAG,
                                        condition, (int*)&vlantag, 1);
        wansinfo[wan_count].ispid = isp_id;
        wansinfo[wan_count].vlantag = vlantag;
        for (j = 0; j < VIF_MAX_IP_NUM && ips[j] && ips[j][0]; ++j) {
            memcpy(wansinfo[wan_count].ips[j].address, ips[j], IP_LEN);
            masknum = (~0) << (32 - atoi(masks[j]));
            sprintf(wansinfo[wan_count].ips[j].netmask, "%u.%u.%u.%u",
                    (masknum >> 24) & 0xff, (masknum >> 16) & 0xff,
                    (masknum >> 8) & 0xff, masknum & 0xff);
            LCPD_LOG(LOG_DEBUG, "wansinfo[%d].isp_id is: %d, "
                "wansinfo[%d].ips[%d].address is: %s, "
                "wansinfo[%d].ips[%d].netmask is: %s, vlantag is: %d.\n",
                wan_count, wansinfo[wan_count].ispid,
                wan_count, j, wansinfo[wan_count].ips[j].address,
                wan_count, j, wansinfo[wan_count].ips[j].netmask,
                wansinfo[wan_count].vlantag);
        }
    }

    saveptr = NULL;
    for (lan_count = 0, token = strtok_r(lansid, "#", &saveptr);
        NULL != token; token = strtok_r(NULL, "#", &saveptr), lan_count++) {
        lanid = atoi(token);

        memset(ifindex, 0, sizeof(ifindex));
        memset(state, 0, sizeof(state));
        memset(subnetid, 0, sizeof(subnetid));
        memset(bandwidth, 0, sizeof(bandwidth));
        memset(ceil_bandwidth, 0, sizeof(ceil_bandwidth));
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", lanid);
        res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                                TABLE_VINTERFACE_COLUMN_IFINDEX,
                                condition, ifindex);
        res += ext_vl2_db_lookup(TABLE_VINTERFACE,
                                 TABLE_VINTERFACE_COLUMN_STATE,
                                 condition, state);
        res += ext_vl2_db_lookup(TABLE_VINTERFACE,
                                 TABLE_VINTERFACE_COLUMN_SUBNETID,
                                 condition, subnetid);
        res += ext_vl2_db_lookup(TABLE_VINTERFACE,
                                 TABLE_VINTERFACE_COLUMN_BANDW,
                                 condition, bandwidth);
        res += ext_vl2_db_lookup(TABLE_VINTERFACE,
                                 TABLE_VINTERFACE_COLUMN_CEIL_BANDW,
                                 condition, ceil_bandwidth);
        ext_vl2_db_lookup(TABLE_VINTERFACE , TABLE_VINTERFACE_COLUMN_MAC,
                          condition, mac);
        if (res) {
            LCPD_LOG(LOG_ERR, "valve %d lan %d (ifindex,state,subnetid,bandw) not exist.",
                    m_opt->vnet_id, lanid);
            goto ret;
        }
        lansinfo[lan_count].ifindex = atoi(ifindex);
        lansinfo[lan_count].state = atoi(state);
        lansinfo[lan_count].qos.minbandwidth = 0;
        sscanf(bandwidth, "%lu", &lansinfo[lan_count].qos.maxbandwidth);
        if (mac[0]) {
            snprintf(lansinfo[lan_count].mac,
                    sizeof(lansinfo[lan_count].mac), "%s", mac);
        } else {
            snprintf(lansinfo[lan_count].mac,
                    sizeof(lansinfo[lan_count].mac), "00:00:00:00:00:00");
        }

        if (lansinfo[lan_count].state != VIF_STATE_ATTACHED) {
            res = del_vl2_tunnel_policies(token);
            if (res) {
                LCPD_LOG(LOG_ERR, "valve %d lanid %s config tunnel failed.",
                        m_opt->vnet_id, token);
                goto ret;
            }
            continue;
        }

        memset(rack, 0, sizeof(rack));
        if (get_rack_of_vdevice(view_id, true, rack) != _VL2_TRUE_ ||
            sscanf(rack, "%u", &rackid) != 1) {
            LCPD_LOG(LOG_ERR, "can not get rack of valve %d.",
                    m_opt->vnet_id);
            goto ret;
        }
        res = lc_db_alloc_vlantag(&vlantag, atoi(subnetid), rackid, 0, false);
        res += lc_vmdb_vif_update_vlantag(vlantag, lanid);
        if (res) {
            LCPD_LOG(LOG_ERR, "valve %d lanid %d alloc vlantag failed.",
                    m_opt->vnet_id, lanid);
            goto ret;
        }
        lansinfo[lan_count].vlantag = vlantag;

        LCPD_LOG(LOG_DEBUG, "lansinfo[%d].ifindex is: %d, "
                "lansinfo[%d].vlantag is: %d, "
                "lansinfo[%d].qos.minbandwidth is: %lu, "
                "lansinfo[%d].qos.maxbandwidth is: %lu, lanid is: %d.",
                lan_count, lansinfo[lan_count].ifindex,
                lan_count, lansinfo[lan_count].vlantag,
                lan_count, lansinfo[lan_count].qos.minbandwidth,
                lan_count, lansinfo[lan_count].qos.maxbandwidth, lanid);

        /* get ips info by lanid */
        memset(ip_buf, 0, sizeof(ip_buf));
        memset(mask_buf, 0, sizeof(mask_buf));
        snprintf(condition, VL2_MAX_APP_CD_LEN, "%s=%d",
                TABLE_VINTERFACEIP_COLUMN_VIFID, lanid);
        res = ext_vl2_db_lookups_multi_v2(TABLE_VINTERFACEIP,
                                          TABLE_VINTERFACEIP_COLUMN_IP,
                                          condition, ips, VIF_MAX_IP_NUM);
        res += ext_vl2_db_lookups_multi_v2(TABLE_VINTERFACEIP,
                                           TABLE_VINTERFACEIP_COLUMN_NETMASK,
                                           condition, masks, VIF_MAX_IP_NUM);
        for (j = 0; j < VIF_MAX_IP_NUM && ips[j] && ips[j][0]; ++j) {
            memcpy(lansinfo[lan_count].ips[j].address, ips[j],
                    MAX_HOST_ADDRESS_LEN);
            memcpy(lansinfo[lan_count].ips[j].netmask, masks[j],
                    MAX_HOST_ADDRESS_LEN);

            LCPD_LOG(LOG_DEBUG, "lansinfo[%d].ips[%d].address is: %s, "
                "lansinfo[%d].ips[%d].netmask is: %s, "
                "masks[%d] is: %s, sizeof is: %zd, "
                "ips[%d] is: %s.",
                lan_count, j, lansinfo[lan_count].ips[j].address,
                lan_count, j, lansinfo[lan_count].ips[j].netmask,
                j, masks[j], sizeof(masks[j]), j, ips[j]);
        }
    }

    /* generate url info */

    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "%s=%d",
             TABLE_VNET_COLUMN_ID, m_opt->vnet_id);
    res = ext_vl2_db_lookup(TABLE_VNET, TABLE_VNET_COLUMN_LAUNCH_SERVER,
                            condition, launchserver);
    if (res) {
        LCPD_LOG(LOG_ERR, "valve %d launch server not exist.",
                m_opt->vnet_id);
        goto ret;
    }

    snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_VALVE,
            launchserver, LCPD_API_DEST_PORT, LCPD_API_VERSION);
    pack_vgateway_update(post, LG_API_URL_SIZE_VALVE,
                         view_id, wan_count, wansinfo,
                         lan_count, lansinfo, 0, 0, LC_ROLE_VALVE);
    LCPD_LOG(LOG_DEBUG,
            "valve update url is: [%s] post is: [%s]",
            url, post);
    res = lcpd_call_api(url, API_METHOD_POST, post, NULL);
    if (res) {
        LCPD_LOG(LOG_ERR, "valve update call url return is %d",
                 res);
        goto ret;
    }

    res = update_valve_vif_mac_and_tunnel(launchserver, m_opt->vnet_id,
            wansinfo, wan_count, lansinfo, lan_count);
    if (res) {
        LCPD_LOG(LOG_ERR, "valve update mac and tunnel policy failed.");
        goto ret;
    }

ret:
    if (res) {
        if (send_response) {
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_opt->vnet_id);
            memset(label, 0, VL2_MAX_DB_BUF_LEN);
            ext_vl2_db_lookup("vnet", "label", condition, label);
            lc_lcpd_db_syslog(LC_SYSLOG_ACTION_DELETE_VALVE,
                              LC_SYSLOG_OBJECTTYPE_VALVE,
                              m_opt->vnet_id, label, "network config failed",
                              LC_SYSLOG_LEVEL_ERR);
            lc_lcpd_valve_msg_reply(m_opt->vnet_id, TALKER__RESULT__FAIL,
                         m_head->seq, TALKER__VALVE_OPS__VALVE_MODIFY);
        }
        return -1;
    } else {
        if (send_response) {
            lc_lcpd_valve_msg_reply(m_opt->vnet_id, TALKER__RESULT__SUCCESS,
                              m_head->seq, TALKER__VALVE_OPS__VALVE_MODIFY);
        }
        return 0;
    }
}

int request_kernel_migrate_valve(struct lc_msg_head *m_head)
{
    struct msg_valve_opt *m_opt;
    char condition[VL2_MAX_APP_CD_LEN];
    char *wan_ids[MAX_WANS_NUM] = {0};
    char *lan_ids[MAX_LANS_NUM] = {0};
    char wan_ids_buf[MAX_WANS_NUM][VL2_MAX_POTS_ID_LEN];
    char lan_ids_buf[MAX_LANS_NUM][VL2_MAX_POTS_ID_LEN];
    int i = 0, j = 0, res = 0;
    lc_mbuf_t m_forward;
    struct msg_valve_opt *m_forward_opt = (struct msg_valve_opt *)m_forward.data;
    char value[VL2_MAX_APP_CD_LEN];
    char label[VL2_MAX_DB_BUF_LEN];
    char curr_launch_server[VL2_MAX_POTS_SP_LEN] = {0};

    LCPD_LOG(LOG_INFO, "The valve message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    m_opt = (struct msg_valve_opt  *)(m_head + 1);
    LCPD_LOG(LOG_DEBUG, "valve migrate info "
            "id is %d, and target gw_launch_server is %s.\n",
            m_opt->vnet_id, m_opt->gw_launch_server);

    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "%s=%u",
             TABLE_VNET_COLUMN_ID, m_opt->vnet_id);
    ext_vl2_db_lookup("vnet", "gw_launch_server", condition, curr_launch_server);

    if (0 != strcmp(curr_launch_server, m_opt->gw_launch_server)) {
        // STEP 1: delete
        memset(&m_forward, 0, sizeof(m_forward));
        memcpy(&m_forward, m_head, sizeof(struct lc_msg_head));
        m_forward_opt->vnet_id = m_opt->vnet_id;
        if (request_kernel_delete_valve(&(m_forward.hdr), false)) {
            LCPD_LOG(LOG_ERR, "delete valve %d in old server failed, "
                    "but will still create it in new server (%s).\n",
                    m_opt->vnet_id, m_opt->gw_launch_server);
        } else {
            LCPD_LOG(LOG_INFO, "delete valve %d in old server succeed, "
                    "will create it in new server (%s).\n",
                    m_opt->vnet_id, m_opt->gw_launch_server);
        }
        // SETP 2: modify gw_launch_server

        snprintf(condition, VL2_MAX_APP_CD_LEN, "%s=%u",
                 TABLE_VNET_COLUMN_ID, m_opt->vnet_id);
        sprintf(value, "'%s'", m_opt->gw_launch_server);
        res = ext_vl2_db_update(TABLE_VNET, TABLE_VNET_COLUMN_LAUNCH_SERVER,
                                value, condition);
    }

    // STEP 3: recreate

    memset(&m_forward, 0, sizeof(m_forward));
    memcpy(&m_forward, m_head, sizeof(struct lc_msg_head));
    m_forward_opt->vnet_id = m_opt->vnet_id;

    memset(wan_ids_buf, 0, sizeof(wan_ids_buf));
    for (i = 0; i < MAX_WANS_NUM; i++) {
        wan_ids[i] = wan_ids_buf[i];
    }
    memset(lan_ids_buf, 0, sizeof(lan_ids_buf));
    for (i = 0; i < MAX_LANS_NUM; i++) {
        lan_ids[i] = lan_ids_buf[i];
    }

    snprintf(condition, VL2_MAX_APP_CD_LEN, "%s='%d' and %s='%d' and %s='%d' and %s='%d'",
             TABLE_VINTERFACE_COLUMN_DEVICETYPE, VIF_DEVICE_TYPE_VGATEWAY,
             TABLE_VINTERFACE_COLUMN_DEVICEID, m_opt->vnet_id,
             TABLE_VINTERFACE_COLUMN_IFTYPE, VIF_TYPE_WAN,
             TABLE_VINTERFACE_COLUMN_STATE, VIF_STATE_ATTACHED);
    res = ext_vl2_db_lookups_v2(TABLE_VINTERFACE, TABLE_VINTERFACE_COLUMN_ID,
                                condition, wan_ids, MAX_WANS_NUM);
    for (i = j = 0; i < MAX_WANS_NUM && wan_ids[i][0]; ++i) {
        j += snprintf(m_forward_opt->wans + j, LC_VALVE_IDS_LEN - j, "%s#", wan_ids[i]);
    }

    snprintf(condition, VL2_MAX_APP_CD_LEN, "%s='%d' and %s='%d' and %s='%d' and %s='%d'",
             TABLE_VINTERFACE_COLUMN_DEVICETYPE, VIF_DEVICE_TYPE_VGATEWAY,
             TABLE_VINTERFACE_COLUMN_DEVICEID, m_opt->vnet_id,
             TABLE_VINTERFACE_COLUMN_IFTYPE, VIF_TYPE_LAN,
             TABLE_VINTERFACE_COLUMN_STATE, VIF_STATE_ATTACHED);
    res = ext_vl2_db_lookups_v2(TABLE_VINTERFACE, TABLE_VINTERFACE_COLUMN_ID,
                                condition, lan_ids, MAX_LANS_NUM);
    for (i = j = 0; i < MAX_LANS_NUM && lan_ids[i][0]; ++i) {
        j += snprintf(m_forward_opt->lans + j, LC_VALVE_IDS_LEN - j, "%s#", lan_ids[i]);
    }

    res = request_kernel_change_valve(&(m_forward.hdr), false);
    if (res) {
        LCPD_LOG(LOG_ERR, "recreate valve %d in %s failed.",
                m_opt->vnet_id, m_opt->gw_launch_server);
        goto ret;
    } else {
        LCPD_LOG(LOG_INFO, "recreate valve %d in %s succeed.",
                m_opt->vnet_id, m_opt->gw_launch_server);
    }

ret:
    if (res) {
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_opt->vnet_id);
        memset(label, 0, VL2_MAX_DB_BUF_LEN);
        ext_vl2_db_lookup("vnet", "label", condition, label);
        lc_lcpd_db_syslog(LC_SYSLOG_ACTION_DELETE_VALVE,
                          LC_SYSLOG_OBJECTTYPE_VALVE,
                          m_opt->vnet_id, label, "network config failed",
                          LC_SYSLOG_LEVEL_ERR);
        lc_lcpd_valve_msg_reply(m_opt->vnet_id, TALKER__RESULT__FAIL,
                                m_head->seq,
                                TALKER__VALVE_OPS__VALVE_MIGRATE);
        return -1;

    } else {
        lc_lcpd_valve_msg_reply(m_opt->vnet_id, TALKER__RESULT__SUCCESS,
                                m_head->seq,
                                TALKER__VALVE_OPS__VALVE_MIGRATE);
        return 0;
    }
}

static int request_kernel_boot_or_down_vdevice(
        struct lc_msg_head *m_head, bool is_boot)
{
    struct msg_vm_opt *m_boot = (struct msg_vm_opt *)(m_head + 1);
    struct msg_vm_opt_start *m_boot_start = (struct msg_vm_opt_start *)(m_boot->data);
    char loginfo[VL2_MAX_DB_BUF_LEN];
    uint32_t vdevice_id;
    int res;
    int res_log = 0;

    char condition[VL2_MAX_APP_CD_LEN] = {0};
    char value[VL2_MAX_APP_CD_LEN] = {0};

    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
             m_head->type, m_head->length);

    snprintf(condition, VL2_MAX_APP_CD_LEN, "label='%s'", m_boot->vm_name);

    res = ext_vl2_db_lookup_v2("vm", "id", condition, value, LOG_WARNING);
    if (res == 0) {
        vdevice_id = atoi(value);
        return request_kernel_boot_or_down_vm(
                vdevice_id, m_boot_start->vm_server, is_boot);
    }

    memset(loginfo, 0, VL2_MAX_DB_BUF_LEN);
    snprintf(loginfo, VL2_MAX_DB_BUF_LEN,
             "vm in host %s does not exist in LiveCloud DB",
             m_boot_start->vm_server);
    if (is_boot) {
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN,
                 "action='%s' and objectname='%s'and date(time)=date(now())",
                 LC_SYSLOG_ACTION_BOOT_VM, m_boot->vm_name);
        ext_vl2_db_count_v2("syslog", "*", condition, &res_log, LOG_WARNING);
        if (!res_log) {
            lc_lcpd_db_syslog(LC_SYSLOG_ACTION_BOOT_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              0, m_boot->vm_name, loginfo, LC_SYSLOG_LEVEL_WARNING);
        }
    } else {
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN,
                 "action='%s' and objectname='%s'and date(time)=date(now())",
                 LC_SYSLOG_ACTION_DOWN_VM, m_boot->vm_name);
        ext_vl2_db_count_v2("syslog", "*", condition, &res_log, LOG_WARNING);
        if (!res_log) {
            lc_lcpd_db_syslog(LC_SYSLOG_ACTION_DOWN_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              0, m_boot->vm_name, loginfo, LC_SYSLOG_LEVEL_WARNING);
        }
    }
    return -1;
}

static int request_kernel_boot_vdevice(struct lc_msg_head *m_head)
{
    return request_kernel_boot_or_down_vdevice(m_head, true);
}

static int request_kernel_down_vdevice(struct lc_msg_head *m_head)
{
    return request_kernel_boot_or_down_vdevice(m_head, false);
}

int request_kernel_attach_interface(struct lc_msg_head *m_head, int callback_flag)
{
    /* used for attach interface from lcweb */
    struct msg_vm_opt *m_opt;
    struct msg_vm_opt_start *m_start;
    char view_id[VL2_MAX_POTS_VI_LEN];
    char network_id[VL2_MAX_POTS_NI_LEN];
    char port_id[VL2_MAX_POTS_PI_LEN];
    char interface_id[VL2_MAX_POTS_ID_LEN];
    char resp_vinterface_ids[LG_API_URL_SIZE] = {0};
    char if_index[VL2_MAX_POTS_ID_LEN];
    char vl2_type[VL2_MAX_POTS_PS_LEN];
    char vlantag[VL2_MAX_INTERF_VD_LEN];
    char bandwidth[VL2_MAX_INTERF_BW_LEN];
    uint64_t bandw, broadcast_bandw, broadcast_ceil_bandw;
    char condition[VL2_MAX_APP_CD_LEN];
    char label[LC_VM_LABEL_LEN] = {0};
    char rack[VL2_MAX_RACK_ID_LEN];
    char if_mac[MAX_HOST_ADDRESS_LEN] = {0};
    char src_addr[VIF_MAX_IP_NUM][MAX_HOST_ADDRESS_LEN];
    char *psrc_addr[VIF_MAX_IP_NUM];
    char dst_ip_min[MAX_HOST_ADDRESS_LEN] = {0};
    char dst_ip_max[MAX_HOST_ADDRESS_LEN] = {0};
    char dst_ip[LC_VM_IFACE_ACL_MAX][MAX_HOST_ADDRESS_LEN];
    char dst_netmask[LC_VM_IFACE_ACL_MAX][MAX_HOST_ADDRESS_LEN];
    char *pdst_ip[LC_VM_IFACE_ACL_MAX];
    char *pdst_netmask[LC_VM_IFACE_ACL_MAX];
    char url[LG_API_URL_SIZE] = {0};
    char url2[LG_API_URL_SIZE] = {0};
    char post[LG_API_BIG_RESULT_SIZE] = {0};
    char result[LG_API_BIG_RESULT_SIZE] = {0};
    char value[VL2_MAX_APP_CD_LEN];
    char domain[LCUUID_LEN] = {0};
    char if_subtype[VL2_MAX_INTERF_SB_LEN] = {0};
    char *acl_action;
    int vm_flag;
    uint32_t vlantag_int;
    uint32_t if_subtype_int = 0;
    //char dest[LC_NAMESIZE];
    int res = 0, i;
    intf_core_t *iface = 0;
    nx_json *root = NULL;
    nx_json *vif = NULL;

    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    m_opt = (struct msg_vm_opt *)(m_head + 1);

    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_opt->vm_id);
    ext_vl2_db_lookup(TABLE_VM, TABLE_VM_COLUMN_DOMAIN,
                      condition, domain);
    ext_vl2_db_lookup(TABLE_VM, TABLE_VM_COLUMN_LABEL,
                      condition, label);

    res = ext_vl2_db_lookup_ids_v2("vm", "flag", condition, &vm_flag, 1);
    if (vm_flag & LC_VM_FLAG_ISOLATE) {
        acl_action = LCPD_API_VPORTS_ACL_ISOLATE_SWITCH_OPEN;
    } else {
        acl_action = LCPD_API_VPORTS_ACL_ISOLATE_SWITCH_CLOSE;
    }

    memset(src_addr, 0, sizeof(src_addr));
    for (i = 0; i < VIF_MAX_IP_NUM; i++) {
        psrc_addr[i] = src_addr[i];
    }
    memset(dst_ip, 0, sizeof(dst_ip));
    memset(dst_netmask, 0, sizeof(dst_netmask));
    for (i = 0; i < LC_VM_IFACE_ACL_MAX; i++) {
        pdst_ip[i] = dst_ip[i];
        pdst_netmask[i] = dst_netmask[i];
    }

    m_start = (struct msg_vm_opt_start *)m_opt->data;

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_ARRAY;
    vif = create_json(NX_JSON_OBJECT, NULL, root);

    iface = &(m_start->vm_ifaces[0]);
    LCPD_LOG(LOG_DEBUG, "vm%d, if%d, idx%d, sub%d, vlan%d, bw%lu, host%s",
            m_opt->vm_id, iface->if_id, iface->if_index,
            iface->if_subnetid, iface->if_vlan, iface->if_bandw,
            m_start->vm_server);

    memset(view_id, 0, VL2_MAX_POTS_VI_LEN);
    strncpy(view_id, "0", VL2_MAX_POTS_VI_LEN - 1);
    memset(network_id, 0, VL2_MAX_POTS_NI_LEN);
    snprintf(network_id, VL2_MAX_POTS_NI_LEN, "%u", iface->if_subnetid);
    memset(port_id, 0, VL2_MAX_POTS_PI_LEN);
    snprintf(port_id, VL2_MAX_POTS_PI_LEN, "%u", m_opt->vm_id);
    memset(interface_id, 0, VL2_MAX_POTS_ID_LEN);
    sprintf(interface_id, "%u", iface->if_id);
    memset(if_index, 0, VL2_MAX_POTS_ID_LEN);
    snprintf(if_index, VL2_MAX_POTS_ID_LEN, "%u", iface->if_index);

    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", iface->if_subnetid);
    memset(vl2_type, 0, VL2_MAX_POTS_PS_LEN);
    ext_vl2_db_lookup(TABLE_VL2, TABLE_VL2_COLUMN_NET_TYPE,
            condition, vl2_type);
    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", iface->if_id);
    memset(if_mac, 0, sizeof(if_mac));
    ext_vl2_db_lookup(TABLE_VINTERFACE, TABLE_VINTERFACE_COLUMN_MAC, condition, if_mac);

    memset(bandwidth, 0, sizeof(bandwidth));
    res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                            TABLE_VINTERFACE_COLUMN_BROADC_BANDW,
                            condition, bandwidth);
    if (res) {
        LCPD_LOG(LOG_ERR, "vm %d if %d (broadcast_bandw) not exist.",
                m_opt->vm_id, iface->if_id);
        goto finish;
    }
    sscanf(bandwidth, "%lu", &broadcast_bandw);
    memset(bandwidth, 0, sizeof(bandwidth));
    res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                            TABLE_VINTERFACE_COLUMN_BROADC_CEIL_BANDW,
                            condition, bandwidth);
    if (res) {
        LCPD_LOG(LOG_ERR, "vm %d if %d (broadcast_ceil_bandw) not exist.",
                m_opt->vm_id, iface->if_id);
        goto finish;
    }
    sscanf(bandwidth, "%lu", &broadcast_ceil_bandw);

    memset(if_subtype, 0, sizeof(if_subtype));
    ext_vl2_db_lookup(TABLE_VINTERFACE, TABLE_VINTERFACEIP_COLUMN_SUBTYPE,
            condition, if_subtype);
    if_subtype_int = atoi(if_subtype);

    if (iface->if_index == LC_VM_IFACE_SERV ||
            atoi(vl2_type) == VIF_TYPE_SERVICE ||
            atoi(vl2_type) == VIF_TYPE_LAN) {

        memset(rack, 0, sizeof(rack));
        if (get_rack_of_vdevice(port_id, false, rack) != _VL2_TRUE_) {
            res = _VL2_FALSE_;
            goto finish;
        }

        res += lc_db_alloc_vlantag(&vlantag_int, iface->if_subnetid,
                atoi(rack), 0, false);
        res += lc_vmdb_vif_update_vlantag(vlantag_int, iface->if_id);

        memset(vlantag, 0, VL2_MAX_INTERF_VD_LEN);
        snprintf(vlantag, VL2_MAX_INTERF_VD_LEN, "%u", vlantag_int);

    } else {
        memset(vlantag, 0, VL2_MAX_INTERF_VD_LEN);
        snprintf(vlantag, VL2_MAX_INTERF_VD_LEN, "%u", iface->if_vlan);
    }
    memset(bandwidth, 0, VL2_MAX_INTERF_BW_LEN);
    snprintf(bandwidth, VL2_MAX_INTERF_BW_LEN, "%lu", iface->if_bandw);

    if (iface->if_index == LC_VM_IFACE_CTRL) {
        //ctrl vlantag
        vlantag_int = 0;
        //ctrl bandwidth
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                TABLE_DOMAIN_CONFIGURATION_OFS_CTRL_BANDWIDTH,
                TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                domain);
        res += ext_vl2_db_lookup(
                TABLE_DOMAIN_CONFIGURATION,
                TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                condition, bandwidth);
        sscanf(bandwidth, "%lu", &bandw);

        //launch_server net
        snprintf(pdst_ip[0], sizeof(dst_ip[0]), "%s", m_start->vm_server);
        snprintf(pdst_netmask[0], sizeof(dst_netmask[0]), "255.255.255.255");
        //ctrl net
        if (vm_flag & LC_VM_TALK_WITH_CONTROLLER_FLAG) {
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                    TABLE_DOMAIN_CONFIGURATION_CTRLLER_CTRL_IP_MIN,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                    domain);
            res += ext_vl2_db_lookup(
                    TABLE_DOMAIN_CONFIGURATION,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                    condition, dst_ip_min);
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                    TABLE_DOMAIN_CONFIGURATION_CTRLLER_CTRL_IP_MAX,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                    domain);
            res += ext_vl2_db_lookup(
                    TABLE_DOMAIN_CONFIGURATION,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                    condition, dst_ip_max);
            //convert net
            ip_net_to_prefix(dst_ip_min, dst_ip_max,
                             pdst_ip[1], pdst_netmask[1]);
        }
        //load ctrl ip
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s=%u",
                TABLE_VINTERFACE_COLUMN_ID,
                atoi(interface_id));
        res += ext_vl2_db_lookups_v2(TABLE_VINTERFACE,
                TABLE_VINTERFACE_COLUMN_IP, condition, psrc_addr, 1);

        append_vm_vif(vif,
                LCPD_API_VPORTS_STATE_ATTACH,
                atoi(interface_id), iface->if_index, if_mac,
                vlantag_int, label,
                0, bandw, broadcast_bandw, broadcast_ceil_bandw,
                LCPD_API_VPORTS_ACL_TYPE_SRC_DST_IP,
                acl_action, psrc_addr, pdst_ip, pdst_netmask);

    } else if (iface->if_index == LC_VM_IFACE_SERV) {
        if (atoi(vl2_type) != VIF_TYPE_SERVICE) {
            LCPD_LOG(LOG_ERR, "Interface %d mismatch with subnet type %s",
                    iface->if_index, vl2_type);
            res += 1;
            goto finish;
        }
        //service bandwidth
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                TABLE_DOMAIN_CONFIGURATION_OFS_SERV_BANDWIDTH,
                TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                domain);
        res += ext_vl2_db_lookup(
                TABLE_DOMAIN_CONFIGURATION,
                TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                condition, bandwidth);
        sscanf(bandwidth, "%lu", &bandw);
        //service net
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                TABLE_DOMAIN_CONFIGURATION_SERVICE_IP_MIN,
                TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                domain);
        res += ext_vl2_db_lookup(
                TABLE_DOMAIN_CONFIGURATION,
                TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                condition, dst_ip_min);
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                TABLE_DOMAIN_CONFIGURATION_SERVICE_IP_MAX,
                TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                domain);
        res += ext_vl2_db_lookup(
                TABLE_DOMAIN_CONFIGURATION,
                TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                condition, dst_ip_max);
        //convert net
        ip_net_to_prefix(dst_ip_min, dst_ip_max, pdst_ip[0], pdst_netmask[0]);

        //load server ip
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s=%u",
                TABLE_VINTERFACE_COLUMN_ID,
                atoi(interface_id));
        res += ext_vl2_db_lookups_v2(TABLE_VINTERFACE,
                TABLE_VINTERFACE_COLUMN_IP, condition, psrc_addr, 1);

        append_vm_vif(vif,
                LCPD_API_VPORTS_STATE_ATTACH,
                atoi(interface_id), iface->if_index, if_mac,
                atoi(vlantag), label,
                0, bandw, broadcast_bandw, broadcast_ceil_bandw,
                LCPD_API_VPORTS_ACL_TYPE_SRC_DST_IP,
                acl_action, psrc_addr, pdst_ip, pdst_netmask);
    } else {
        sscanf(bandwidth, "%lu", &bandw);
        /* custom interfaces */
        if (atoi(vl2_type) == VIF_TYPE_WAN) {
            //load ip
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s=%u",
                    TABLE_IP_RESOURCE_COLUMN_VIFID,
                    atoi(interface_id));
            if ((if_subtype_int & VIF_SUBTYPE_L2_INTERFACE)
                != VIF_SUBTYPE_L2_INTERFACE) {
                ext_vl2_db_lookups_v2(
                        TABLE_IP_RESOURCE, TABLE_IP_RESOURCE_COLUMN_IP,
                        condition, psrc_addr, VIF_MAX_IP_NUM);
                append_vm_vif(vif,
                        LCPD_API_VPORTS_STATE_ATTACH,
                        atoi(interface_id), iface->if_index, if_mac,
                        atoi(vlantag), label,
                        bandw, bandw, broadcast_bandw, broadcast_ceil_bandw,
                        LCPD_API_VPORTS_ACL_TYPE_SRC_IP,
                        acl_action, psrc_addr, NULL, NULL);
            } else {
                append_vm_vif(vif,
                        LCPD_API_VPORTS_STATE_ATTACH,
                        atoi(interface_id), iface->if_index, if_mac,
                        atoi(vlantag), label,
                        bandw, bandw, broadcast_bandw, broadcast_ceil_bandw,
                        LCPD_API_VPORTS_ACL_TYPE_ALL_DROP,
                        acl_action, NULL, NULL, NULL);
            }
        } else if (atoi(vl2_type) == VIF_TYPE_LAN) {
            if ((if_subtype_int & VIF_SUBTYPE_L2_INTERFACE)
                != VIF_SUBTYPE_L2_INTERFACE) {
                append_vm_vif(vif,
                        LCPD_API_VPORTS_STATE_ATTACH,
                        atoi(interface_id), iface->if_index, if_mac,
                        atoi(vlantag), label,
                        0, bandw, broadcast_bandw, broadcast_ceil_bandw,
                        LCPD_API_VPORTS_ACL_TYPE_SRC_MAC,
                        acl_action, NULL, NULL, NULL);
            } else {
                append_vm_vif(vif,
                        LCPD_API_VPORTS_STATE_ATTACH,
                        atoi(interface_id), iface->if_index, if_mac,
                        atoi(vlantag), label,
                        0, bandw, broadcast_bandw, broadcast_ceil_bandw,
                        LCPD_API_VPORTS_ACL_TYPE_ALL_DROP,
                        acl_action, NULL, NULL, NULL);
            }
        } else {
            LCPD_LOG(LOG_INFO, "Handles imported VM");
        }
    }

    dump_json_msg(root, post, LG_API_BIG_RESULT_SIZE);

    snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_VPORTS,
            m_start->vm_server, LCPD_AGEXEC_DEST_PORT, LCPD_API_VERSION);
    if (callback_flag) {
        snprintf(url2, LG_API_URL_SIZE, LCPD_API_CALLBACK LCPD_API_CALLBACK_VPORTS,
                 master_ctrl_ip, LCPD_TALKER_LISTEN_PORT, LCPD_API_VERSION);
        strcat(url, url2);
    }

    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", atoi(interface_id));
    ext_vl2_db_update("vinterface", "errno", "0", condition);

    if (is_server_mode_vmware(LC_VM_TYPE_VM, m_opt->vm_id)) {
        LCPD_LOG(LOG_INFO, "Skip pyagexec call for VMware VM.");
    } else {
        LCPD_LOG(LOG_INFO, "Send url post:%s\n", post);
        result[0] = 0;
        res = lcpd_call_api(url, API_METHOD_POST, post, result);
        if (res) {  // curl failed or api failed
            LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
            parse_json_msg_vif_config_result(result, resp_vinterface_ids);
            goto finish;
        }
    }

    if (iface->if_index == LC_VM_IFACE_SERV ||
            atoi(vl2_type) == VIF_TYPE_SERVICE ||
            atoi(vl2_type) == VIF_TYPE_LAN) {
        res = add_vl2_tunnel_policies(interface_id);
        if (res) {
            LCPD_LOG(LOG_ERR, "config tunnel policy failed for vif %s\n",
                    interface_id);
            goto finish;
        }
    }

finish:
    nx_json_free(root);
    if (res) {
        if (resp_vinterface_ids[0] == 0) {
            snprintf(resp_vinterface_ids, sizeof(resp_vinterface_ids),
                "(%s)", interface_id);
        }
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id in %s", resp_vinterface_ids);
        snprintf(value, VL2_MAX_APP_CD_LEN, "%d", LC_VIF_ERRNO_NETWORK);
        ext_vl2_db_update("vinterface", "errno", value, condition);

        lc_lcpd_db_syslog(LC_SYSLOG_ACTION_ATTACH_VIF, LC_SYSLOG_OBJECTTYPE_VM,
                          m_opt->vm_id, label, "network config failed",
                          LC_SYSLOG_LEVEL_ERR);
        lc_lcpd_vinterface_msg_reply(interface_id, TALKER__RESULT__FAIL,
                                     m_head->seq, TALKER__INTERFACE_OPS__INTERFACE_ATTACH);
        return -1;
    }
    lc_lcpd_vinterface_msg_reply(interface_id, TALKER__RESULT__SUCCESS,
                                 m_head->seq, TALKER__INTERFACE_OPS__INTERFACE_ATTACH);
    return 0;
}

static int request_kernel_detach_interface(struct lc_msg_head *m_head)
{
    /* used for detach interface from lcweb */
    struct msg_vm_opt *m_opt;
    struct msg_vm_opt_start *m_start;
    char view_id[VL2_MAX_POTS_VI_LEN];
    char network_id[VL2_MAX_POTS_NI_LEN];
    char port_id[VL2_MAX_POTS_PI_LEN];
    char interface_id[VL2_MAX_POTS_ID_LEN];
    char resp_vinterface_ids[LG_API_URL_SIZE] = {0};
    char condition[VL2_MAX_APP_CD_LEN];
    char label[LC_VM_LABEL_LEN] = {0};
    char if_mac[MAX_HOST_ADDRESS_LEN];
    char vlantag[VL2_MAX_INTERF_VD_LEN];
    char url[LG_API_URL_SIZE] = {0};
    char post[LG_API_BIG_RESULT_SIZE] = {0};
    char result[LG_API_BIG_RESULT_SIZE] = {0};
    char value[VL2_MAX_APP_CD_LEN];
    //char dest[LC_NAMESIZE];
    int res = 0;
    intf_core_t *iface = 0;
    nx_json *root = NULL;
    nx_json *vif = NULL;

    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    m_opt = (struct msg_vm_opt *)(m_head + 1);
    m_start = (struct msg_vm_opt_start *)m_opt->data;

    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", m_opt->vm_id);
    ext_vl2_db_lookup("vm", "label", condition, label);

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_ARRAY;
    vif = create_json(NX_JSON_OBJECT, NULL, root);

    iface = &(m_start->vm_ifaces[0]);
    LCPD_LOG(LOG_DEBUG, "vm%d, if%d, idx%d, sub%d, host%s",
            m_opt->vm_id, iface->if_id, iface->if_index,
            iface->if_subnetid, m_start->vm_server);

    memset(view_id, 0, VL2_MAX_POTS_VI_LEN);
    strncpy(view_id, "0", VL2_MAX_POTS_VI_LEN - 1);
    memset(network_id, 0, VL2_MAX_POTS_NI_LEN);
    snprintf(network_id, VL2_MAX_POTS_NI_LEN, "%u", iface->if_subnetid);
    memset(port_id, 0, VL2_MAX_POTS_PI_LEN);
    snprintf(port_id, VL2_MAX_POTS_PI_LEN, "%u", m_opt->vm_id);
    memset(interface_id, 0, VL2_MAX_POTS_ID_LEN);
    sprintf(interface_id, "%u", iface->if_id);

    if (iface->if_index == LC_VM_IFACE_CTRL) {
        LCPD_LOG(LOG_ERR, "Cannot modify control interface");
        res += 1;
        goto finish;
    }

    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", iface->if_id);
    memset(if_mac, 0, sizeof(if_mac));
    ext_vl2_db_lookup(TABLE_VINTERFACE, TABLE_VINTERFACE_COLUMN_MAC, condition, if_mac);
    memset(vlantag, 0, sizeof(vlantag));
    ext_vl2_db_lookup(TABLE_VINTERFACE, TABLE_VINTERFACE_COLUMN_VLANTAG, condition, vlantag);

    append_vm_vif(vif, LCPD_API_VPORTS_STATE_DETACH,
                  atoi(interface_id), iface->if_index,
                  if_mac, atoi(vlantag), label,
                  0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL);

    dump_json_msg(root, post, LG_API_BIG_RESULT_SIZE);

    snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_VPORTS,
             m_start->vm_server, LCPD_AGEXEC_DEST_PORT, LCPD_API_VERSION);

    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", atoi(interface_id));
    ext_vl2_db_update("vinterface", "errno", "0", condition);

    if (is_server_mode_vmware(LC_VM_TYPE_VM, m_opt->vm_id)) {
        LCPD_LOG(LOG_INFO, "Skip pyagexec call for VMware VM.");
    } else {
        LCPD_LOG(LOG_INFO, "Send url post:%s\n", post);
        result[0] = 0;
        res = lcpd_call_api(url, API_METHOD_POST, post, result);
        if (res) {  // curl failed or api failed
            LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
            parse_json_msg_vif_config_result(result, resp_vinterface_ids);
            // goto finish after delete tunnel policy
        }
    }

    res += del_vl2_tunnel_policies(interface_id);
    if (res) {
        goto finish;
    }

finish:
    nx_json_free(root);
    if (res) {
        if (resp_vinterface_ids[0] == 0) {
            snprintf(resp_vinterface_ids, sizeof(resp_vinterface_ids),
                "(%s)", interface_id);
        }
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id in %s", resp_vinterface_ids);
        snprintf(value, VL2_MAX_APP_CD_LEN, "%d", LC_VIF_ERRNO_NETWORK);
        ext_vl2_db_update("vinterface", "errno", value, condition);

        lc_lcpd_db_syslog(LC_SYSLOG_ACTION_DETACH_VIF, LC_SYSLOG_OBJECTTYPE_VM,
                          m_opt->vm_id, label, "network config failed",
                          LC_SYSLOG_LEVEL_ERR);
        lc_lcpd_vinterface_msg_reply(interface_id, TALKER__RESULT__FAIL,
                                     m_head->seq, TALKER__INTERFACE_OPS__INTERFACE_DETACH);
        return -1;
    }
    lc_lcpd_vinterface_msg_reply(interface_id, TALKER__RESULT__SUCCESS,
                                 m_head->seq, TALKER__INTERFACE_OPS__INTERFACE_DETACH);
    return 0;
}

typedef struct {
    int num;
    int order;
} count_t;

static inline int count_cmp( const void *a ,const void *b)
{
    return (*(count_t *)a).num > (*(count_t *)b).num ? 1 : -1;
}

#if 0
void nsp_cluster_ha_ipmi_notify(int host_id)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__PMOpsReq pm_ops_req = TALKER__PMOPS_REQ__INIT;

    pm_ops_req.pm_id = host_id;
    pm_ops_req.pm_type = HOST_DEVICE;
    pm_ops_req.pm_ops = PM_STOP;

    msg.pm_ops_req = &pm_ops_req;
    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__LCPD;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    fill_message_header(&hdr, buf);

    lc_bus_lcpd_publish_unicast(buf, (hdr_len + msg_len), LC_BUS_QUEUE_PMADAPTER);
    return 0;
}
#endif

static int nsp_ipmi_shutdown_exec(ipmi_t *ipmip)
{
    int cnt;
    char cmd[LC_EXEC_CMD_LEN];
    FILE *stream  = NULL;

    /* PING: avoid the block from IPMI commands */
    memset(cmd, 0, LC_EXEC_CMD_LEN);
    sprintf(cmd, "ping -w 1 -i 0.001 -c 100 %s -q | grep -Eo \"[0-9]+ received\"",
            ipmip->ip);
    stream = popen(cmd, "r");
    if (NULL == stream) {
        return -1;
    }
    fscanf(stream, "%d received", &cnt);
    pclose(stream);

    if (cnt == 0) {
        return -2;
    };

    /* Power off */
    memset(cmd, 0, LC_EXEC_CMD_LEN);
    sprintf(cmd, "ipmitool -H %s -I lanplus -U %s -P %s power off | grep -ic off",
            ipmip->ip, ipmip->username, ipmip->password);
    stream = popen(cmd, "r");
    if (NULL == stream) {
        LC_LOG(LOG_ERR, "stream is NULL.\n");
        return -3;
    }
    fscanf(stream, "%d", &cnt);
    pclose(stream);

    if (cnt == 0) {
        return -4;
    }

    sleep(10);

    /* Power status */
    memset(cmd, 0, LC_EXEC_CMD_LEN);
    sprintf(cmd, "ipmitool -H %s -I lanplus -U %s -P %s power status | grep -ic off",
            ipmip->ip, ipmip->username, ipmip->password);
    stream = popen(cmd, "r");
    if (NULL == stream) {
        LC_LOG(LOG_ERR, "stream is NULL.\n");
        return -5;
    }
    fscanf(stream, "%d", &cnt);
    pclose(stream);

    if (cnt == 0) {
        return -6;
    }

    return 0;
}

static int nsp_ipmi_shutdown(char *host_address)
{
    ipmi_t ipmi = {0};
    int host_id, code;

    code = lc_vmdb_host_load_hostid(&host_id, host_address);
    if (code == LC_DB_HOST_NOT_FOUND) {
        return -10;
    }

    code = lc_vmdb_ipmi_load_by_type(&ipmi, host_id, IPMI_HOST_DEV);
    if (code == LC_DB_HOST_NOT_FOUND) {
        return -20;
    }

    code = nsp_ipmi_shutdown_exec(&ipmi);
    if (code) {
        return -30+code;
    }

    return 0;
}

int nsp_vgws_migrate(vnet_t *vnet, char *dst_nsp_ip)
{
    if (vnet->role == LC_ROLE_VALVE) {
        return call_curl_patch_all_nsp_valves(vnet->lcuuid, dst_nsp_ip);
    } else {
        return call_curl_patch_all_nsp_vgws(vnet->lcuuid, dst_nsp_ip);
    }
}

static int nsp_get_config_info(nsp_host_t *nsp_host, char *host_address)
{
    memset(nsp_host, 0, sizeof(nsp_host_t));
    strncpy(nsp_host->host_address, host_address, MAX_HOST_ADDRESS_LEN);
    lc_vmdb_nsp_host_load(nsp_host, host_address);

    if (nsp_host->email_address[0]) {
        nsp_host->email_enable = 1;
    } else {
        nsp_host->email_enable = 0;
    }

    return 0;
}

static int send_nsp_fail_over_email(int type, nsp_host_t *hostp,
        char *email_msg, int level, int flag)
{
    header_t hdr;
    char cmd[MAX_CMD_BUF_SIZE];
    time_t current;
    char subject[50];
    char mail_buf[MAX_CMD_BUF_SIZE];
    //int is_admin = 0;
    send_request_t send_req;
    char *to = NULL;
    int mlen = sizeof(mail_buf);
    int hlen = 0;
    int ret = 0;
    int addrlen = 0;

    if (type == POSTMAN__RESOURCE__HOST) {
        to = hostp->email_address;
    }
    if (NULL == to) {
        return 0;
    }

    memset(subject, 0, 50);
    memset(cmd, 0, MAX_CMD_BUF_SIZE);
    memset(&send_req, 0, sizeof(send_request_t));
    memset(mail_buf, 0, sizeof(mail_buf));

    current = time(NULL);
    send_req.policy.priority = POSTMAN__PRIORITY__URGENT;
    send_req.policy.aggregate_id = POSTMAN__AGGREGATE_ID__AGG_NO_AGGREGATE;
    send_req.policy.min_event_interval = 0;
    send_req.policy.min_dest_interval = 0;
    send_req.policy.issue_timestamp = current;

    if (type == POSTMAN__RESOURCE__HOST) {
        if (level == LC_SYSLOG_LEVEL_WARNING) {
            send_req.id.event_type = POSTMAN__EVENT__NSP_FAIL;
        } else {
            send_req.id.event_type = POSTMAN__EVENT__NSP_FAIL_OVER;
        }
        send_req.id.resource_type = POSTMAN__RESOURCE__HOST;
        send_req.id.resource_id = hostp->cr_id;
    }

    if (strlen(hostp->from)) {
        strcpy(send_req.mail.from, hostp->from);
    }
    strcpy(send_req.mail.to, to);
    if (level == LC_SYSLOG_LEVEL_WARNING) {
        send_req.mail.mail_type = POSTMAN__MAIL_TYPE__ALARM_MAIL;
    } else {
        send_req.mail.mail_type = POSTMAN__MAIL_TYPE__NOTIFY_MAIL;
    }
    if (type == POSTMAN__RESOURCE__HOST) {
        strcpy(send_req.mail.head_template, "head_notify_nsp_ha_to_admin");
        strcpy(send_req.mail.tail_template, "tail_notify_nsp_ha_to_admin");
        strcpy(send_req.mail.resource_type, "NSP");
        strcpy(send_req.mail.resource_name, hostp->host_address);
        strcpy(subject, "Controller Message");
        strcpy(send_req.mail.subject, subject);
    }

    if (hostp->email_enable) {
        if (0 == strcmp(to, hostp->email_address)) {
            //is_admin = 1;
        } else {
            strcpy(send_req.mail.cc, hostp->email_address);
        }
    }
    if (hostp->email_address_ccs[0]) {
        addrlen = strlen(send_req.mail.cc);
        snprintf(send_req.mail.cc + addrlen,
                 MAIL_ADDR_LEN - addrlen, ",%s", hostp->email_address_ccs);
    }

    strncpy(send_req.mail.alarm_message, email_msg, LC_EMAIL_ALARM_MESSAGE_LEN);
    send_req.mail.alarm_begin = current;

    /* publish send_request to postman */

    hlen = get_message_header_pb_len();
    mlen = construct_send_request(&send_req, mail_buf + hlen, mlen - hlen);

    hdr.sender = HEADER__MODULE__LCPD;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = mlen;
    hdr.seq = 0;
    fill_message_header(&hdr, mail_buf);

    ret = lc_bus_lcpd_publish_unicast(mail_buf, mlen + hlen, LC_BUS_QUEUE_POSTMAND);
    if (ret) {
        LCPD_LOG(LOG_ERR, "error send mail to postman");
    }

    return 0;
}

static int request_kernel_down_nsp(struct lc_msg_head *m_head)
{
    struct msg_nsp_opt *m_opt;
    char msg[MAX_LOG_MSG_LEN] = {0};
    char host_address[MAX_HOST_ADDRESS_LEN] = {0};
    uint32_t host_state, pool_index;

    host_t up_host_set[MAX_HOST_NUM];
    vnet_t down_vnet_set[MAX_VNET_NUM];
    count_t up_vnet_count[MAX_HOST_NUM];
    nsp_host_t nsp_host;
    int down_vnet_num, all_vnet_num, avg_vnet_num, avail_vnet_num, up;
    int up_host_num, order_i, i, j, undone, retry, ret;

    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    m_opt = (struct msg_nsp_opt *)(m_head + 1);
    strcpy(host_address, m_opt->host_address);
    host_state = m_opt->host_state;
    pool_index = m_opt->pool_index;

    /* STEP1: Analyze the received message from lcmonitor
     *
     * STEP2: Search the DB to find all the vgateways {A} that
     * the DOWN NSP is supporting
     *
     * STEP3: Disable target NSP by SBD fencing or IPMI shutdown
     *
     * STEP4: Calculate the number of vgateways {B} in the UP NSPs
     * of corresponding pool where the target NSP exists
     *
     * STEP5: Allocate the {A} into the UP NSPs from small to big
     * to guarantee that each NSP has at most N(A)+N(B)/N(NSP)
     * vgateways
     *
     * STEP6: Email, syslog
     */

    retry = 0;
un_done:
    undone = 0;

    memset(down_vnet_set, 0, sizeof(vnet_t) * MAX_VNET_NUM);
    lc_vmdb_vnet_load_all_v2_by_server_for_migrate(
            down_vnet_set, sizeof(vnet_t), &down_vnet_num, host_address);
    if (!down_vnet_num) {
        if (!strlen(msg)) {
            snprintf(msg, MAX_LOG_MSG_LEN, "NSP server %s has not been used yet, "
                    "No vgateway in NSP server %s needs to be migrated",
                    host_address, host_address);
        } else {
            strcat(msg, ", corresponding vgateways has been migrated");
        }
        goto end;
    }
    if (down_vnet_num > MAX_VNET_NUM) {
        LC_LOG(LOG_WARNING, "The vnet number is %d, "
                "exceeding the max vnet number %d",
                down_vnet_num, MAX_VNET_NUM);
        down_vnet_num = MAX_VNET_NUM;
        undone = 1;
    }

    all_vnet_num = down_vnet_num;

    memset(up_host_set, 0, sizeof(host_t) * MAX_HOST_NUM);
    lc_vmdb_compute_resource_load_all_v2_by_state(up_host_set, sizeof(host_t),
            &up_host_num, pool_index, host_state);
    if (!up_host_num) {
        snprintf(msg, MAX_LOG_MSG_LEN, "No available NSP server exists, "
                "the vgateways in NSP server %s has not been migrated", host_address);
        LCPD_LOG(LOG_ERR, "%s\n", msg);
        goto error;
    }
    if (up_host_num > MAX_HOST_NUM) {
        LC_LOG(LOG_WARNING, "The host number is %d, "
                "exceeding the max host number %d",
                up_host_num, MAX_HOST_NUM);
        up_host_num = MAX_HOST_NUM;
    }

    for (i=0; i<up_host_num; i++) {
        up_vnet_count[i].order = i;
        lc_vmdb_vnet_get_num_by_server(&up_vnet_count[i].num,
                up_host_set[i].ip);
        all_vnet_num += up_vnet_count[i].num;
    }

    avg_vnet_num = ceil(all_vnet_num / up_host_num);
    qsort(up_vnet_count, up_host_num, sizeof(count_t), count_cmp);

    /* IPMI: Xiaoquan's API */
    if ((ret = nsp_ipmi_shutdown(host_address))) {
        snprintf(msg, MAX_LOG_MSG_LEN, "NSP server %s cannot be maintained",
                host_address);
        LCPD_LOG(LOG_WARNING, "%s (%d)\n", msg, ret);
    } else {
        snprintf(msg, MAX_LOG_MSG_LEN, "NSP server %s is maintained",
                host_address);
    }

    up = 0;
    for (i=0; i<up_host_num; i++) {
        order_i = up_vnet_count[i].order;
        avail_vnet_num = avg_vnet_num - up_vnet_count[i].num;
        for (j=0; j<avail_vnet_num; j++) {
#if 0
            lc_vmdb_vnet_update_state(TABLE_VNET_STATE_EXCEPTION,
                    down_vnet_set[up].vnet_id);
#endif
            if ((ret = nsp_vgws_migrate(&down_vnet_set[up],
                    up_host_set[order_i].ip))) {
                LCPD_LOG(LOG_DEBUG,
                    "Call curl for NSP vgateway %s migrate failed (%d)\n",
                    down_vnet_set[up].vnet_name, ret);
                /* continue; */
            }
            if (++up >= down_vnet_num) {
                break;
            }
        }
        if (up >= down_vnet_num) {
            break;
        }
    }
    if (up < down_vnet_num) {
        undone = 1;
    }
#define MAX_RETRY_TIMES     5
    if (undone) {
        if (retry < MAX_RETRY_TIMES) {
            retry++;
            goto un_done;
        } else {
            strcat(msg, ", not all corresponding vgateways are migrated");
            goto error;
        }
    }

    strcat(msg, ", corresponding vgateways has been migrated");

end:
    LCPD_LOG(LOG_INFO, "%s\n", msg);
    nsp_get_config_info(&nsp_host, host_address);
    send_nsp_fail_over_email(POSTMAN__RESOURCE__HOST, &nsp_host, msg,
            LC_SYSLOG_LEVEL_INFO, LC_LCPD_POSTMAN_NOTIFY_FLAG);
    return 0;
error:
    LCPD_LOG(LOG_ERR, "%s\n", msg);
    nsp_get_config_info(&nsp_host, host_address);
    send_nsp_fail_over_email(POSTMAN__RESOURCE__HOST, &nsp_host, msg,
            LC_SYSLOG_LEVEL_WARNING, LC_LCPD_POSTMAN_NO_FLAG);
    return -1;
}

int iprange_to_prefix(char *start, char *end, ip_prefix_t *prefixes)
{
    int i, count = 0;
    uint32_t left, right;
    struct in_addr tmpaddr;

    inet_aton(start, &tmpaddr);
    left = ntohl(tmpaddr.s_addr);
    inet_aton(end, &tmpaddr);
    right = ntohl(tmpaddr.s_addr);

    if (left == right) {
        tmpaddr.s_addr = htonl(left);
        strncpy(prefixes[0].address, inet_ntoa(tmpaddr), 16);
        tmpaddr.s_addr = htonl(0xffffffff);
        strncpy(prefixes[0].mask, inet_ntoa(tmpaddr), 16);
        count = 1;
    } else {
        while (left < right) {
            for (i = 0; i < IP_ADDR_WIDTH; i++) {
                if (left % (1 << (i+1)) != 0 || (left + (1 << (i+1)) - 1) > right)
                    break;
            }
            tmpaddr.s_addr = htonl(left);
            strncpy(prefixes[count].address, inet_ntoa(tmpaddr), 16);
            tmpaddr.s_addr = htonl(0xffffffff << i);
            strncpy(prefixes[count].mask, inet_ntoa(tmpaddr), 16);
            left += (1 << i);
            count++;
        }
        if (left == right) {
            tmpaddr.s_addr = htonl(left);
            strncpy(prefixes[count].address, inet_ntoa(tmpaddr), 16);
            tmpaddr.s_addr = htonl(0xffffffff);
            strncpy(prefixes[count].mask, inet_ntoa(tmpaddr), 16);
            count++;
        }
    }
    return count;
}


int request_kernel_add_hwdevice(struct lc_msg_head *m_head)
{
    char name[VL2_MAX_DB_BUF_LEN];
    struct msg_hwdev_opt *m_opt;
    struct msg_hwdev_opt_start *m_start;
    int res = 0, i = 0;
    uint32_t vlantag, rackid, switch_port;
    char vlantag_c[VL2_MAX_INTERF_VD_LEN];
    char vlantag_buf[VIF_MAX_IP_NUM][VL2_MAX_INTERF_VD_LEN];
    char *vlantag_p[VIF_MAX_IP_NUM];
    char view_id[VL2_MAX_POTS_VI_LEN];
    char hwdev_id[VL2_MAX_POTS_VI_LEN];
    char interface_id[VL2_MAX_POTS_ID_LEN];
    char bandwidth[VL2_MAX_INTERF_BW_LEN];
    char network_id[VL2_MAX_POTS_NI_LEN];
    char conditions[VL2_MAX_DB_BUF_LEN];
    char rack[VL2_MAX_RACK_ID_LEN];
    int vl2_type = 0;
    intf_t intf_info;
    torswitch_t torswitch_info;
    int torswitch_type;
    char ip_min[MAX_HOST_ADDRESS_LEN];
    char ip_max[MAX_HOST_ADDRESS_LEN];
    int ip_prefix_count = 0;
    char ip_buf[VIF_MAX_IP_NUM][MAX_HOST_ADDRESS_LEN];
    char *ips[VIF_MAX_IP_NUM];
    char *ip_list_src;
    char *ip_list_dst;
    int cur_len = 0, total_len = 0;
    ip_prefix_t prefixes[PREFIX_MAX];
    char url[LG_API_URL_SIZE] = {0};
    char url2[LG_API_URL_SIZE] = {0};
    char result[LG_API_BIG_RESULT_SIZE] = {0};
    char post[LG_API_BIG_RESULT_SIZE] = {0};
    char domain[LCUUID_LEN] = {0};
    char conf_action[LCUUID_LEN] = {0};
    nx_json *root = NULL;

    total_len = VIF_MAX_IP_NUM * (MAX_HOST_ADDRESS_LEN + 1);
    ip_list_src = malloc(total_len);
    if (!ip_list_src) {
        LCPD_LOG(LOG_ERR, "failed to allocate mem for ip_list_src");
        res = -1;
        goto error_malloc;
    }
    ip_list_dst = malloc(total_len);
    if (!ip_list_dst) {
        LCPD_LOG(LOG_ERR, "failed to allocate mem for ip_list_dst");
        free(ip_list_src);
        res = -1;
        goto error_malloc;
    }
    memset(prefixes, 0, sizeof(prefixes));
    memset(ip_list_src, 0, total_len);
    memset(ip_list_dst, 0, total_len);
    memset(ip_buf, 0, sizeof(ip_buf));
    memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
    memset(vlantag_c, 0, VL2_MAX_INTERF_VD_LEN);
    memset(rack, 0, sizeof(rack));
    memset((char*)&torswitch_info, 0, sizeof(torswitch_info));

    for (i = 0; i < VIF_MAX_IP_NUM; ++i) {
        ips[i] = ip_buf[i];
    }
    for (i = 0; i < VIF_MAX_IP_NUM; ++i) {
        vlantag_p[i] = vlantag_buf[i];
    }

    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    m_opt = (struct msg_hwdev_opt *)(m_head + 1);
    sprintf(view_id, "%u", m_opt->vnet_id);
    sprintf(network_id, "%u", m_opt->vl2_id);
    sprintf(hwdev_id, "%u", m_opt->hwdev_id);
    vl2_type = m_opt->vl2_type;

    m_start = (struct msg_hwdev_opt_start *)m_opt->data;
    switch_port = m_start->switch_port;
    sprintf(interface_id, "%u", m_start->hwdev_iface.if_id);
    sprintf(bandwidth, "%lu", m_start->hwdev_iface.if_bandw/1024);

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%s",
            TABLE_VINTERFACE_COLUMN_ID, interface_id);
    if (lc_db_vif_load(&intf_info, "*", conditions)) {
        LCPD_LOG(LOG_ERR, "failed to load vinterface info vifid=%s\n", interface_id);
        goto error;
    }

    memset(conditions, 0, sizeof(conditions));
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "id=%s", hwdev_id);
    if (ext_vl2_db_lookup("third_party_device", "domain", conditions, domain)) {
        LCPD_LOG(LOG_ERR, "failed to load domain info\n");
        goto error;
    }

    if (get_rack_of_hwdev(hwdev_id, rack) == _VL2_TRUE_) {
        sscanf(rack, "%u", &rackid);
    }
    if (vl2_type == VIF_TYPE_LAN) {
        if ((res += lc_db_alloc_vlantag(&vlantag, atoi(network_id), rackid, 0, false))
            != _VL2_TRUE_) {
            LCPD_LOG(LOG_ERR, "Failed to allocate vlantag");
            goto error;
        }
        if ((res += lc_vmdb_vif_update_vlantag(vlantag, atoi(interface_id)))
            != _VL2_TRUE_) {
            LCPD_LOG(LOG_ERR, "Failed to update vlantag");
            goto error;
        }
        sprintf(vlantag_c, "%u", vlantag);
        if ((res += add_vl2_tunnel_policies(interface_id))
            != _VL2_TRUE_) {
            LCPD_LOG(LOG_ERR, "Failed to add vl2 tunnel policies");
            goto error;
        }
    } else if (vl2_type == VIF_TYPE_SERVICE) {
        if ((res += lc_db_alloc_vlantag(&vlantag, atoi(network_id), rackid, 0, false))
            != _VL2_TRUE_) {
            LCPD_LOG(LOG_ERR, "Failed to allocate vlantag");
            goto error;
        }
        if ((res += lc_vmdb_vif_update_vlantag(vlantag, atoi(interface_id)))
            != _VL2_TRUE_) {
            LCPD_LOG(LOG_ERR, "Failed to update vlantag");
            goto error;
        }
        sprintf(vlantag_c, "%u", vlantag);
        if ((res += add_vl2_tunnel_policies(interface_id))
            != _VL2_TRUE_) {
            LCPD_LOG(LOG_ERR, "Failed to add vl2 tunnel policies");
            goto error;
        }
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d", TABLE_RACK_COLUMN_ID, rackid);
    if((res += ext_vl2_db_lookup_ids(TABLE_RACK, TABLE_RACK_COLUMN_TORSWITCH_TYPE, conditions, &torswitch_type)
        != _VL2_TRUE_)) {
        LCPD_LOG(LOG_ERR, "failed to get torswitch type of rack%d", rackid);
        goto error;
    }
    //if (torswitch_type != TORSWITCH_TYPE_TUNNEL) {
    if (switch_port == 0) {
        LCPD_LOG(LOG_DEBUG, "The torswitch type of rack%d is not TUNNEL. "
                 "Please refer to the log file and config torswitch manually.",
                 rackid);
        sprintf(conf_action, "%s", LCPD_API_CONF_SWITCH_ACT_NONE);
    } else {
        sprintf(conf_action, "%s", LCPD_API_CONF_SWITCH_ACT_CONF);
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d and %s='%s'", TABLE_NETWORK_DEVICE_COLUMN_RACKID, rackid,
             TABLE_NETWORK_DEVICE_COLUMN_LCUUID, intf_info.if_netdevice_lcuuid);
    if(ext_vl2_db_network_device_load(&torswitch_info, conditions)) {
        LCPD_LOG(LOG_ERR, "failed to load torswitch for rack%d\n", rackid);
        goto error;
    }

    /*To attach an interface to a subnet*/
    if (vl2_type ==  VIF_TYPE_WAN) {
        memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%s",
                TABLE_IP_RESOURCE_COLUMN_VIFID, interface_id);
        ext_vl2_db_lookups_v2(TABLE_IP_RESOURCE,
                TABLE_IP_RESOURCE_COLUMN_VLANTAG, conditions, vlantag_p, 1);
        if (vlantag_buf[0][0] == 0) {
            LCPD_LOG(LOG_ERR, "failed to get the vlantag of pub interface");
            goto error;
        }
        sprintf(vlantag_c, vlantag_buf[0]);
        if ((res += lc_vmdb_vif_update_vlantag(atoi(vlantag_c), atoi(interface_id)))
            != _VL2_TRUE_) {
            LCPD_LOG(LOG_ERR, "Failed to update vlantag");
            goto error;
        }

        memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%s",
                TABLE_IP_RESOURCE_COLUMN_VIFID, interface_id);
        ext_vl2_db_lookups_v2(TABLE_IP_RESOURCE,
                TABLE_IP_RESOURCE_COLUMN_IP, conditions, ips,
                sizeof(ips) / sizeof(ips[0]));
        if (ips[0][0] == 0) {
            LCPD_LOG(LOG_ERR, "failed to get the ips of the interface");
            goto error;
        }
        cur_len = 0;
        for (i = 0; i < VIF_MAX_IP_NUM && ips[i] && ips[i][0]; ++i) {
            if (i != 0) {
                cur_len += snprintf(ip_list_src + cur_len, total_len - cur_len, ",");
            }
            cur_len += snprintf(ip_list_src + cur_len, total_len - cur_len, "%s/255.255.255.255", ips[i]);
        }
        snprintf(ip_list_dst, total_len, "0.0.0.0/0.0.0.0");

    } else if (vl2_type == VIF_TYPE_LAN) {
        snprintf(ip_list_src, total_len, "0.0.0.0/0.0.0.0");
        snprintf(ip_list_dst, total_len, "0.0.0.0/0.0.0.0");
    } else if (vl2_type == VIF_TYPE_CTRL) {
        memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                TABLE_DOMAIN_CONFIGURATION_OFS_CTRL_VLAN,
                TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                domain);
        ext_vl2_db_lookups_v2(TABLE_DOMAIN_CONFIGURATION,
                              TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE, conditions, vlantag_p, 1);
        if (vlantag_buf[0][0] == 0) {
            LCPD_LOG(LOG_ERR, "failed to get the vlantag of control interface");
            goto error;
        }
        sprintf(vlantag_c, vlantag_buf[0]);
        if ((res += lc_vmdb_vif_update_vlantag(atoi(vlantag_c), atoi(interface_id)))
            != _VL2_TRUE_) {
            LCPD_LOG(LOG_ERR, "Failed to update vlantag");
            goto error;
        }

        memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                TABLE_DOMAIN_CONFIGURATION_CTRLLER_CTRL_IP_MIN,
                TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                domain);
        ext_vl2_db_lookups_v2(TABLE_DOMAIN_CONFIGURATION,
                              TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE, conditions, ips, 1);
        if (ips[0][0] == 0) {
            LCPD_LOG(LOG_ERR, "failed to get the ips of the %s",
                    TABLE_DOMAIN_CONFIGURATION_CTRLLER_CTRL_IP_MIN);
            goto error;
        }
        strncpy(ip_min, ips[0], MAX_HOST_ADDRESS_LEN);

        memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
        memset(ip_buf, 0, sizeof(ip_buf));
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                TABLE_DOMAIN_CONFIGURATION_CTRLLER_CTRL_IP_MAX,
                TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                domain);
        ext_vl2_db_lookups_v2(TABLE_DOMAIN_CONFIGURATION,
                              TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE, conditions, ips, 1);
        if (ips[0][0] == 0) {
            LCPD_LOG(LOG_ERR, "failed to get the ips of the %s",
                    TABLE_DOMAIN_CONFIGURATION_CTRLLER_CTRL_IP_MAX);
            goto error;
        }
        strncpy(ip_max, ips[0], MAX_HOST_ADDRESS_LEN);
        ip_prefix_count = iprange_to_prefix(ip_min, ip_max, prefixes);

        snprintf(ip_list_src, total_len, "%s/255.255.255.255", intf_info.if_ip);
        cur_len = 0;
        for (i = 0; i < ip_prefix_count; ++i) {
            if (i != 0) {
                cur_len += snprintf(ip_list_dst + cur_len, total_len - cur_len, ",");
            }
            cur_len += snprintf(ip_list_dst + cur_len, total_len - cur_len, "%s/%s",
                                prefixes[i].address, prefixes[i].mask);
        }

    } else if (vl2_type == VIF_TYPE_SERVICE){
        memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
        if (intf_info.subtype == VIF_SUBTYPE_SERVICE_USER) {
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                    TABLE_DOMAIN_CONFIGURATION_SERVICE_IP_MIN,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                    domain);
        } else {
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                    TABLE_DOMAIN_CONFIGURATION_SERVICE_VM_IP_MIN,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                    domain);
        }
        ext_vl2_db_lookups_v2(TABLE_DOMAIN_CONFIGURATION,
                              TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE, conditions, ips, 1);
        if (ips[0][0] == 0) {
            LCPD_LOG(LOG_ERR, "failed to get the ips of the service_ip_min");
            goto error;
        }
        strncpy(ip_min, ips[0], MAX_HOST_ADDRESS_LEN);

        memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
        memset(ip_buf, 0, sizeof(ip_buf));
        if (intf_info.subtype == VIF_SUBTYPE_SERVICE_USER) {
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                    TABLE_DOMAIN_CONFIGURATION_SERVICE_IP_MAX,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                    domain);
        } else {
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                    TABLE_DOMAIN_CONFIGURATION_SERVICE_VM_IP_MAX,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                    domain);
        }
        ext_vl2_db_lookups_v2(TABLE_DOMAIN_CONFIGURATION,
                              TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE, conditions, ips, 1);
        if (ips[0][0] == 0) {
            LCPD_LOG(LOG_ERR, "failed to get the ips of the service_ip_max");
            goto error;
        }
        strncpy(ip_max, ips[0], MAX_HOST_ADDRESS_LEN);
        ip_prefix_count = iprange_to_prefix(ip_min, ip_max, prefixes);

        snprintf(ip_list_src, total_len, "%s/255.255.255.255", intf_info.if_ip);
        cur_len = 0;
        for (i = 0; i < ip_prefix_count; ++i) {
            if (i != 0) {
                cur_len += snprintf(ip_list_dst + cur_len, total_len - cur_len, ",");
            }
            cur_len += snprintf(ip_list_dst + cur_len, total_len - cur_len, "%s/%s",
                                prefixes[i].address, prefixes[i].mask);
        }

    } else {
        LCPD_LOG(LOG_ERR, "vl2 type error");
        goto error;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;
    nx_json_switch_conf_info(root, conf_action, switch_port,
            vlantag_c, bandwidth, NULL, intf_info.if_mac,
            ip_list_src, ip_list_dst);
    dump_json_msg(root, post, LG_API_BIG_RESULT_SIZE);
    nx_json_free(root);

    snprintf(url, LG_API_URL_SIZE, SDN_API_PREFIX SDN_API_SWITCH_PORT,
            SDN_API_DEST_IP_SDNCTRL, SDN_CTRL_LISTEN_PORT, SDN_API_VERSION,
            torswitch_info.ip, switch_port);
    snprintf(url2, LG_API_URL_SIZE, SDN_API_CALLBACK SDN_API_CALLBACK_VPORTS,
             master_ctrl_ip, LCPD_TALKER_LISTEN_PORT, SDN_API_VERSION);
    strcat(url, url2);

    LCPD_LOG(LOG_INFO, "Send url: %s ##### post:%s\n", url, post);
    result[0] = 0;
    res = lcpd_call_api(url, API_METHOD_POST, post, result);
    if (res) {  // curl failed or api failed
        LCPD_LOG(LOG_ERR, "failed to config switch port on rack mip = %s\n", torswitch_info.ip);
        LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
        goto error;
    }

    lc_lcpd_hwdev_msg_reply(m_opt->hwdev_id, m_start->hwdev_iface.if_id,
                            TALKER__HWDEV_OPS__HWDEV_ATTACH, res,
                            TALKER__RESULT__SUCCESS, m_head->seq);
    free(ip_list_src);
    free(ip_list_dst);
    return res;

error:
    memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "id=%u", m_opt->hwdev_id);
    memset(name, 0, VL2_MAX_DB_BUF_LEN);
    ext_vl2_db_lookup("third_party_device", "name", conditions, name);
    lc_lcpd_db_syslog(LC_SYSLOG_ACTION_ATTACH_HWDEV,
                      LC_SYSLOG_OBJECTTYPE_HWDEV,
                      m_opt->hwdev_id, name,
                      "hardware device attaching failed",
                      LC_SYSLOG_LEVEL_ERR);
    lc_lcpd_hwdev_msg_reply(m_opt->hwdev_id, m_start->hwdev_iface.if_id,
                            TALKER__HWDEV_OPS__HWDEV_ATTACH, res,
                            TALKER__RESULT__FAIL, m_head->seq);
    free(ip_list_src);
    free(ip_list_dst);

error_malloc:
    return res;
}

int request_kernel_del_hwdevice(struct lc_msg_head *m_head)
{
    char name[VL2_MAX_DB_BUF_LEN];
    struct msg_hwdev_opt *m_opt;
    struct msg_hwdev_opt_start *m_start;
    int res = 0;
    uint32_t rackid, switch_port;
    char view_id[VL2_MAX_POTS_VI_LEN];
    char hwdev_id[VL2_MAX_POTS_VI_LEN];
    char interface_id[VL2_MAX_POTS_ID_LEN];
    char bandwidth[VL2_MAX_INTERF_BW_LEN];
    char network_id[VL2_MAX_POTS_NI_LEN];
    char conditions[VL2_MAX_DB_BUF_LEN];
    char rack[VL2_MAX_RACK_ID_LEN];
    int vl2_type = 0;
    intf_t intf_info;
    torswitch_t torswitch_info;
    int torswitch_type;
    char url[LG_API_URL_SIZE] = {0};
    char url2[LG_API_URL_SIZE] = {0};
    char result[LG_API_BIG_RESULT_SIZE] = {0};
    char post[LG_API_BIG_RESULT_SIZE] = {0};
    char conf_action[LCUUID_LEN] = {0};
    nx_json *root = NULL;

    memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
    memset(rack, 0, sizeof(rack));
    memset((char*)&torswitch_info, 0, sizeof(torswitch_info));


    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    m_opt = (struct msg_hwdev_opt *)(m_head + 1);
    sprintf(view_id, "%u", m_opt->vnet_id);
    sprintf(network_id, "%u", m_opt->vl2_id);
    sprintf(hwdev_id, "%u", m_opt->hwdev_id);
    vl2_type = m_opt->vl2_type;

    m_start = (struct msg_hwdev_opt_start *)m_opt->data;
    switch_port = m_start->switch_port;
    sprintf(interface_id, "%u", m_start->hwdev_iface.if_id);
    sprintf(bandwidth, "%lu", m_start->hwdev_iface.if_bandw/1024);

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%s",
            TABLE_VINTERFACE_COLUMN_ID, interface_id);
    if (lc_db_vif_load(&intf_info, "*", conditions)) {
        LCPD_LOG(LOG_ERR, "failed to load vinterface info vifid=%s\n", interface_id);
        goto error;
    }

    if (get_rack_of_hwdev(hwdev_id, rack) == _VL2_TRUE_) {
        sscanf(rack, "%u", &rackid);
    }
    if (vl2_type == VIF_TYPE_LAN || vl2_type == VIF_TYPE_SERVICE) {
        if ((res += del_vl2_tunnel_policies(interface_id))
            != _VL2_TRUE_) {
            LCPD_LOG(LOG_ERR, "Failed to del vl2 tunnel policies");
            goto error;
        }
    }
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d", TABLE_RACK_COLUMN_ID, rackid);
    if((res += ext_vl2_db_lookup_ids(TABLE_RACK, TABLE_RACK_COLUMN_TORSWITCH_TYPE, conditions, &torswitch_type)
        != _VL2_TRUE_)) {
        LCPD_LOG(LOG_ERR, "failed to get torswitch type of rack%d", rackid);
        goto error;
    }
    //if (torswitch_type != TORSWITCH_TYPE_TUNNEL) {
    if (switch_port == 0) {
        LCPD_LOG(LOG_DEBUG, "The torswitch type of rack%d is not TUNNEL. "
                 "Please refer to the log file and config torswitch manually.",
                 rackid);
        sprintf(conf_action, "%s", LCPD_API_CONF_SWITCH_ACT_NONE);
    } else {
        sprintf(conf_action, "%s", LCPD_API_CONF_SWITCH_ACT_CONF);
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d and %s='%s'", TABLE_NETWORK_DEVICE_COLUMN_RACKID, rackid,
             TABLE_NETWORK_DEVICE_COLUMN_LCUUID, intf_info.if_netdevice_lcuuid);
    if(ext_vl2_db_network_device_load(&torswitch_info, conditions)) {
        LCPD_LOG(LOG_ERR, "failed to load torswitch for rack%d\n", rackid);
        goto error;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;
    nx_json_switch_conf_info(root, conf_action, switch_port,
            NULL, NULL, NULL, intf_info.if_mac, NULL, NULL);
    dump_json_msg(root, post, LG_API_BIG_RESULT_SIZE);
    nx_json_free(root);

    snprintf(url, LG_API_URL_SIZE, SDN_API_PREFIX SDN_API_SWITCH_PORT,
            SDN_API_DEST_IP_SDNCTRL, SDN_CTRL_LISTEN_PORT, SDN_API_VERSION,
            torswitch_info.ip, switch_port);
    snprintf(url2, LG_API_URL_SIZE, SDN_API_CALLBACK SDN_API_CALLBACK_VPORTS,
             master_ctrl_ip, LCPD_TALKER_LISTEN_PORT, SDN_API_VERSION);
    strcat(url, url2);

    LCPD_LOG(LOG_INFO, "Send url: %s ##### post:%s\n", url, post);
    result[0] = 0;
    res = lcpd_call_api(url, API_METHOD_PATCH, post, result);
    if (res) {  // curl failed or api failed
        LCPD_LOG(LOG_ERR, "failed to close switch port on rack mip = %s, port=%u\n",
                torswitch_info.ip, switch_port);
        LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
        res = -1;
        goto error;
    }

    lc_lcpd_hwdev_msg_reply(m_opt->hwdev_id, m_start->hwdev_iface.if_id,
                            TALKER__HWDEV_OPS__HWDEV_DETACH, res,
                            TALKER__RESULT__SUCCESS, m_head->seq);
    return res;

error:
    memset(conditions, 0, VL2_MAX_DB_BUF_LEN);
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "id=%u", m_opt->hwdev_id);
    memset(name, 0, VL2_MAX_DB_BUF_LEN);
    ext_vl2_db_lookup("third_party_device", "name", conditions, name);
    lc_lcpd_db_syslog(LC_SYSLOG_ACTION_DETACH_HWDEV,
                      LC_SYSLOG_OBJECTTYPE_HWDEV,
                      m_opt->hwdev_id, name,
                      "hardware device detaching failed",
                      LC_SYSLOG_LEVEL_ERR);
    lc_lcpd_hwdev_msg_reply(m_opt->hwdev_id, m_start->hwdev_iface.if_id,
                            TALKER__HWDEV_OPS__HWDEV_DETACH, res,
                            TALKER__RESULT__FAIL, m_head->seq);
    return res;
}


static int request_kernel_vinterfaces_config(struct lc_msg_head *m_head)
{
    /* used for vinterface config callback */
    struct msg_vinterfaces_opt *m_opt;
    intf_t intfs_info[VL2_MAX_VIF_NUM];
    char condition[VL2_MAX_APP_CD_LEN];
    char url[LG_API_URL_SIZE] = {0};
    char post[LG_API_BIG_RESULT_SIZE] = {0};
    char result[LG_API_BIG_RESULT_SIZE] = {0};
    char vinterface_ids[LG_API_URL_SIZE] = {0};
    char resp_vinterface_ids[LG_API_URL_SIZE] = {0};
    char server_ip[VL2_MAX_POTS_SP_LEN] = {0};
    char src_addr[VL2_MAX_VIF_NUM][VIF_MAX_IP_NUM][MAX_HOST_ADDRESS_LEN];
    char *psrc_addr[VL2_MAX_VIF_NUM][VIF_MAX_IP_NUM];
    char dst_ip_min[MAX_HOST_ADDRESS_LEN] = {0};
    char dst_ip_max[MAX_HOST_ADDRESS_LEN] = {0};
    char dst_ip[LC_VM_IFACES_MAX][LC_VM_IFACE_ACL_MAX][MAX_HOST_ADDRESS_LEN];
    char dst_netmask[LC_VM_IFACES_MAX][LC_VM_IFACE_ACL_MAX][MAX_HOST_ADDRESS_LEN];
    char *pdst_ip[LC_VM_IFACES_MAX][LC_VM_IFACE_ACL_MAX];
    char *pdst_netmask[LC_VM_IFACES_MAX][LC_VM_IFACE_ACL_MAX];
    char value[VL2_MAX_APP_CD_LEN];
    char *acl_action;
    char domain[LCUUID_LEN] = {0};
    char vm_lcuuid[LCUUID_LEN] = {0};
    char ms_lcuuid[LCUUID_LEN] = {0};
    char vm_label[LC_VM_LABEL_LEN] = {0};
    int vl2_type, vm_flag;
    int vlantag;
    char bandwidth[VL2_MAX_INTERF_BW_LEN] = {0};
    uint64_t bandw, broadcast_bandw, broadcast_ceil_bandw;
    nx_json *root = NULL;
    nx_json *vif = NULL;
    nx_json *vifs_to_ms[VL2_MAX_VIF_NUM] = {NULL};
    uint32_t ms_ids[VL2_MAX_VIF_NUM] = {0};
    int res = 0;
    int i, j, vif_num = 0;

    LCPD_LOG(LOG_INFO, "The message type=%d, msg len=%d\n",
              m_head->type, m_head->length);

    m_opt = (struct msg_vinterfaces_opt *)(m_head + 1);

    snprintf(condition, VL2_MAX_APP_CD_LEN, "id in %s", m_opt->vinterface_ids);
    lc_vmdb_vif_load_all(intfs_info, "*", condition, &vif_num);
    if (!vif_num) {
        goto FINISH;
    }

    memset(src_addr, 0, sizeof(src_addr));
    for (i = 0; i < VL2_MAX_VIF_NUM; i++) {
        for (j = 0; j < VIF_MAX_IP_NUM; j++) {
            psrc_addr[i][j] = src_addr[i][j];
        }
    }
    memset(dst_ip, 0, sizeof(dst_ip));
    memset(dst_netmask, 0, sizeof(dst_netmask));
    for (i = 0; i < LC_VM_IFACES_MAX; i++) {
        for (j = 0; j < LC_VM_IFACE_ACL_MAX; j++) {
            pdst_ip[i][j] = dst_ip[i][j];
            pdst_netmask[i][j] = dst_netmask[i][j];
        }
    }

    sprintf(condition, "id=%u", intfs_info[0].if_deviceid);
    res = ext_vl2_db_lookup("vm", "launch_server", condition, server_ip);
    res = ext_vl2_db_lookup("vm", "domain", condition, domain);
    res = ext_vl2_db_lookup("vm", "lcuuid", condition, vm_lcuuid);
    res = ext_vl2_db_lookup("vm", "label", condition, vm_label);

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_ARRAY;

    for (i = 0; i < vif_num; ++i) {
        vifs_to_ms[i] = (nx_json *)malloc(sizeof(nx_json));
        if (NULL == vifs_to_ms[i]) {
            return -1;
        }
        memset(vifs_to_ms[i], 0, sizeof(nx_json));
        vifs_to_ms[i]->type = NX_JSON_OBJECT;
    }

    for (i = 0; i < vif_num; ++i) {
        vif = create_json(NX_JSON_OBJECT, NULL, root);
        if (intfs_info[i].if_state == LC_VIF_STATE_DETACHED) {
            append_vm_vif(vif,
                    LCPD_API_VPORTS_STATE_DETACH,
                    intfs_info[i].if_id,
                    intfs_info[i].if_index,
                    intfs_info[i].if_mac,
                    intfs_info[i].if_vlan,
                    vm_label,
                    0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL);
            continue;
        }
        sprintf(condition, "id=%u", intfs_info[i].if_deviceid);
        res = ext_vl2_db_lookup_ids_v2("vm", "flag", condition, &vm_flag, 1);
        if (vm_flag & LC_VM_FLAG_ISOLATE) {
            acl_action = LCPD_API_VPORTS_ACL_ISOLATE_SWITCH_OPEN;
        } else {
            acl_action = LCPD_API_VPORTS_ACL_ISOLATE_SWITCH_CLOSE;
        }
        memset(bandwidth, 0, sizeof(bandwidth));
        res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                                TABLE_VINTERFACE_COLUMN_BROADC_BANDW,
                                condition, bandwidth);
        sscanf(bandwidth, "%lu", &broadcast_bandw);
        memset(bandwidth, 0, sizeof(bandwidth));
        res = ext_vl2_db_lookup(TABLE_VINTERFACE,
                                TABLE_VINTERFACE_COLUMN_BROADC_CEIL_BANDW,
                                condition, bandwidth);
        sscanf(bandwidth, "%lu", &broadcast_ceil_bandw);

        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        sprintf(condition, "id=%u", intfs_info[i].if_id);
        memset(value, 0, sizeof(value));
        res = ext_vl2_db_lookup("vinterface", "ms_id", condition, value);
        sscanf(value, "%u", &ms_ids[i]);

        if (intfs_info[i].if_index == LC_VM_IFACE_CTRL) {
            //ctrl vlantag
            vlantag = 0;
            //ctrl bandwidth
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                    TABLE_DOMAIN_CONFIGURATION_OFS_CTRL_BANDWIDTH,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                    domain);
            res += ext_vl2_db_lookup(
                    TABLE_DOMAIN_CONFIGURATION,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                    condition, bandwidth);
            sscanf(bandwidth, "%lu", &bandw);
            //launch_server net
            snprintf(pdst_ip[i][0], sizeof(dst_ip[i][0]), "%s", server_ip);
            snprintf(pdst_netmask[i][0], sizeof(dst_netmask[i][0]), "255.255.255.255");
            //ctrl net
            if (vm_flag & LC_VM_TALK_WITH_CONTROLLER_FLAG) {
                memset(condition, 0, VL2_MAX_APP_CD_LEN);
                snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                        TABLE_DOMAIN_CONFIGURATION_CTRLLER_CTRL_IP_MIN,
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                        domain);
                res += ext_vl2_db_lookup(
                        TABLE_DOMAIN_CONFIGURATION,
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                        condition, dst_ip_min);
                memset(condition, 0, VL2_MAX_APP_CD_LEN);
                snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                        TABLE_DOMAIN_CONFIGURATION_CTRLLER_CTRL_IP_MAX,
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                        domain);
                res += ext_vl2_db_lookup(
                        TABLE_DOMAIN_CONFIGURATION,
                        TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                        condition, dst_ip_max);
                //convert net
                ip_net_to_prefix(dst_ip_min, dst_ip_max,
                                 pdst_ip[i][1], pdst_netmask[i][1]);
            }
            //load ctrl ip
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s=%u",
                    TABLE_VINTERFACE_COLUMN_ID,
                    intfs_info[i].if_id);
            res += ext_vl2_db_lookups_v2(TABLE_VINTERFACE,
                    TABLE_VINTERFACE_COLUMN_IP, condition, psrc_addr[i], 1);

            append_vm_vif(vif,
                    LCPD_API_VPORTS_STATE_ATTACH,
                    intfs_info[i].if_id, intfs_info[i].if_index,
                    intfs_info[i].if_mac, vlantag, vm_label,
                    0, bandw, broadcast_bandw, broadcast_ceil_bandw,
                    LCPD_API_VPORTS_ACL_TYPE_SRC_DST_IP,
                    acl_action, psrc_addr[i], pdst_ip[i], pdst_netmask[i]);
        } else if (intfs_info[i].if_index == LC_VM_IFACE_SERV) {
            //service bandwidth
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                    TABLE_DOMAIN_CONFIGURATION_OFS_SERV_BANDWIDTH,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                    domain);
            res += ext_vl2_db_lookup(
                    TABLE_DOMAIN_CONFIGURATION,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                    condition, bandwidth);
            sscanf(bandwidth, "%lu", &bandw);
            //service net
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                    TABLE_DOMAIN_CONFIGURATION_SERVICE_IP_MIN,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                    domain);
            res += ext_vl2_db_lookup(
                    TABLE_DOMAIN_CONFIGURATION,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                    condition, dst_ip_min);
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
                    TABLE_DOMAIN_CONFIGURATION_SERVICE_IP_MAX,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
                    domain);
            res += ext_vl2_db_lookup(
                    TABLE_DOMAIN_CONFIGURATION,
                    TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE,
                    condition, dst_ip_max);
            //convert net
            ip_net_to_prefix(dst_ip_min, dst_ip_max, dst_ip[i][0], dst_netmask[i][0]);

            //load server ip
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s=%u",
                    TABLE_VINTERFACE_COLUMN_ID,
                    intfs_info[i].if_id);
            res += ext_vl2_db_lookups_v2(TABLE_VINTERFACE,
                    TABLE_VINTERFACE_COLUMN_IP, condition, psrc_addr[i], 1);

            append_vm_vif(vif,
                    LCPD_API_VPORTS_STATE_ATTACH,
                    intfs_info[i].if_id, intfs_info[i].if_index,
                    intfs_info[i].if_mac, intfs_info[i].if_vlan,
                    vm_label,
                    0, bandw, broadcast_bandw, broadcast_ceil_bandw,
                    LCPD_API_VPORTS_ACL_TYPE_SRC_DST_IP,
                    acl_action, psrc_addr[i], pdst_ip[i], pdst_netmask[i]);
        } else {
            /* custom interfaces */
            memset(condition, 0, VL2_MAX_APP_CD_LEN);
            snprintf(condition, VL2_MAX_APP_CD_LEN,
                    "id=%u", intfs_info[i].if_subnetid);
            ext_vl2_db_lookup_ids_v2(TABLE_VL2, TABLE_VL2_COLUMN_NET_TYPE,
                    condition, &vl2_type, 1);
            if (vl2_type== VIF_TYPE_WAN) {
                if ((intfs_info[i].subtype & VIF_SUBTYPE_L2_INTERFACE)
                    != VIF_SUBTYPE_L2_INTERFACE) {
                    //load ip
                    memset(condition, 0, VL2_MAX_APP_CD_LEN);
                    snprintf(condition, VL2_MAX_DB_BUF_LEN, "%s=%u",
                        TABLE_IP_RESOURCE_COLUMN_VIFID, intfs_info[i].if_id);
                    res += ext_vl2_db_lookups_v2(
                            TABLE_IP_RESOURCE, TABLE_IP_RESOURCE_COLUMN_IP,
                            condition, psrc_addr[i], VIF_MAX_IP_NUM);

                    append_vm_vif(vif,
                            LCPD_API_VPORTS_STATE_ATTACH,
                            intfs_info[i].if_id, intfs_info[i].if_index,
                            intfs_info[i].if_mac, intfs_info[i].if_vlan,
                            vm_label,
                            intfs_info[i].if_bandw, intfs_info[i].if_bandw,
                            broadcast_bandw, broadcast_ceil_bandw,
                            LCPD_API_VPORTS_ACL_TYPE_SRC_IP,
                            acl_action, psrc_addr[i], NULL, NULL);
                } else {
                    append_vm_vif(vif,
                            LCPD_API_VPORTS_STATE_ATTACH,
                            intfs_info[i].if_id, intfs_info[i].if_index,
                            intfs_info[i].if_mac, intfs_info[i].if_vlan,
                            vm_label,
                            intfs_info[i].if_bandw, intfs_info[i].if_bandw,
                            broadcast_bandw, broadcast_ceil_bandw,
                            LCPD_API_VPORTS_ACL_TYPE_ALL_DROP,
                            acl_action, NULL, NULL, NULL);
                }
            } else {
                if ((intfs_info[i].subtype & VIF_SUBTYPE_L2_INTERFACE)
                    != VIF_SUBTYPE_L2_INTERFACE) {
                    append_vm_vif(vif,
                            LCPD_API_VPORTS_STATE_ATTACH,
                            intfs_info[i].if_id, intfs_info[i].if_index,
                            intfs_info[i].if_mac, intfs_info[i].if_vlan,
                            vm_label,
                            0, intfs_info[i].if_bandw,
                            broadcast_bandw, broadcast_ceil_bandw,
                            LCPD_API_VPORTS_ACL_TYPE_SRC_MAC,
                            acl_action, NULL, NULL, NULL);
                } else {
                    append_vm_vif(vif,
                            LCPD_API_VPORTS_STATE_ATTACH,
                            intfs_info[i].if_id, intfs_info[i].if_index,
                            intfs_info[i].if_mac, intfs_info[i].if_vlan,
                            vm_label,
                            0, intfs_info[i].if_bandw,
                            broadcast_bandw, broadcast_ceil_bandw,
                            LCPD_API_VPORTS_ACL_TYPE_ALL_DROP,
                            acl_action, NULL, NULL, NULL);
                }
            }
        }
        append_instance_vif_to_ms(vifs_to_ms[i],
                "VM", vm_lcuuid, intfs_info[i].if_index);
    }

    dump_json_msg(root, post, LG_API_BIG_RESULT_SIZE);
    nx_json_free(root);

    snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_VPORTS, server_ip,
            LCPD_AGEXEC_DEST_PORT, LCPD_API_VERSION);

    memset(condition, 0, VL2_MAX_APP_CD_LEN);
    snprintf(condition, VL2_MAX_APP_CD_LEN, "id in %s", m_opt->vinterface_ids);
    ext_vl2_db_update("vinterface", "errno", "0", condition);

    LCPD_LOG(LOG_INFO, "Send url:%s post:%s\n", url, post);
    result[0] = 0;
    res = lcpd_call_api(url, API_METHOD_POST, post, result);
    if (res) {  // curl failed or api failed
        LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
        parse_json_msg_vif_config_result(result, resp_vinterface_ids);
        goto FINISH;
    }

    for (i = 0; i < vif_num; ++i) {
        if (ms_ids[i] == 0) {
            continue;
        }
        memset(post, 0, LG_API_BIG_RESULT_SIZE);
        dump_json_msg(vifs_to_ms[i], post, LG_API_BIG_RESULT_SIZE);
        nx_json_free(vifs_to_ms[i]);

        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        memset(ms_lcuuid, 0, sizeof(ms_lcuuid));
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id=%u", ms_ids[i]);
        res = ext_vl2_db_lookup("micro_segment", "lcuuid", condition,
                ms_lcuuid);

        memset(url, 0, LG_API_URL_SIZE);
        snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_MS_VIF_ATTACH,
                LCPD_TALKER_LISTEN_IP, LCPD_TALKER_LISTEN_PORT,
                LCPD_API_VERSION, ms_lcuuid);
        LCPD_LOG(LOG_INFO, "Send url %s post:%s\n", url, post);
        result[0] = 0;
        res = lcpd_call_api(url, API_METHOD_POST, post, result);
        if (res != 0) {  // curl failed or api failed
            LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
            goto FINISH;
        }
    }

FINISH:
    if (res) {
        if (resp_vinterface_ids[0] == 0) {
            snprintf(resp_vinterface_ids, sizeof(resp_vinterface_ids),
                "(%s)", m_opt->vinterface_ids);
        }
        memset(condition, 0, VL2_MAX_APP_CD_LEN);
        snprintf(condition, VL2_MAX_APP_CD_LEN, "id in %s", resp_vinterface_ids);
        snprintf(value, VL2_MAX_APP_CD_LEN, "%d", LC_VIF_ERRNO_NETWORK);
        ext_vl2_db_update("vinterface", "errno", value, condition);

        lc_lcpd_vinterface_msg_reply(vinterface_ids, TALKER__RESULT__FAIL,
                                     m_head->seq, TALKER__INTERFACE_OPS__INTERFACE_CONFIG);
        return -1;

    } else {
        lc_lcpd_vinterface_msg_reply(m_opt->vinterface_ids, TALKER__RESULT__SUCCESS,
                                     m_head->seq, TALKER__INTERFACE_OPS__INTERFACE_CONFIG);
        return 0;
    }
}
