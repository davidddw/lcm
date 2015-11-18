#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "lc_global.h"
#include "lc_netcomp.h"
#include "lc_vnet.h"
#include "lc_vnet_worker.h"
#include "framework/types.h"
#include "framework/utils.h"
#include "message/msg.h"
#include "lc_db.h"
#include "lc_db_net.h"
#include "lc_db_log.h"
#include "lc_db_errno.h"
#include "vl2/ext_vl2_handler.h"
#include "lc_kernel.h"
#include "cloudmessage.pb-c.h"
#include "talker.pb-c.h"

#define ID2INDEX(x) ((x) - 1)
#define INDEX2ID(x) ((x) + 1)

#define LC_VM_GATEWAY_ID    999

#define DEFAULT_PORT_NUM    64

static int lc_vl2_add(struct lc_msg_head *m_head);
static int lc_vl2_change(struct lc_msg_head *m_head);

static int lc_network_msg_reply(vl2_t *vl2_info, uint32_t result, uint32_t seq, int ops, uint32_t receiver)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__NetworksOpsReply vl2_ops = TALKER__NETWORKS_OPS_REPLY__INIT;
    int hdr_len = get_message_header_pb_len();
    int msg_len;
    char buf[MAX_MSG_BUFFER];

    memset(buf, 0, MAX_MSG_BUFFER);

    vl2_ops.id = vl2_info->id;
    vl2_ops.err = vl2_info->err;
    vl2_ops.result = result;
    vl2_ops.ops = ops;
    vl2_ops.has_id = vl2_ops.has_err = 1;
    vl2_ops.has_result = vl2_ops.has_ops = 1;

    msg.networks_ops_reply = &vl2_ops;

    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__LCPD;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seq;
    fill_message_header(&hdr, buf);

    lc_bus_lcpd_publish_unicast(buf, (hdr_len + msg_len), receiver);
    return 0;
}

int application_msg_handler(struct lc_msg_head *m_head)
{
    int ret;

    if (!is_lc_msg(m_head)) {
        return -1;
    }

    switch (m_head->type) {
    case LC_MSG_VL2_ADD:
        ret = lc_vl2_add(m_head);
        return ret;

    case LC_MSG_VL2_CHANGE:
        ret = lc_vl2_change(m_head);
        return ret;

    default:
        return -1;
    }
}

static int lc_vl2_add (struct lc_msg_head *m_head)
{
    vl2_t vl2;
    struct msg_vl2_opt *mvl2;
    uint32_t vlantag = 0;

    memset(&vl2, 0, sizeof(vl2_t));

    mvl2 = (struct msg_vl2_opt *)(m_head + 1);
    LCPD_LOG(LOG_INFO, "vl2 (%d) add\n",mvl2->vl2_id);
    vl2.id = mvl2->vl2_id;

    if (lc_vmdb_vl2_load(&vl2, mvl2->vl2_id)) {
        LCPD_LOG(LOG_ERR, "vl2 (%d) add failure, vl2 not exist\n", mvl2->vl2_id);
        vl2.err |= LC_VL2_ERRNO_DB_ERROR;
        goto error;
    }

    lc_vmdb_vl2_update_state(LC_VL2_STATE_FINISH, vl2.id);
    LCPD_LOG(LOG_INFO, "The vl2 (%d/%s) added finish\n",mvl2->vl2_id, vl2.name);

    if (vl2.vlan_request) {
        if (lc_db_alloc_vlantag(&vlantag, vl2.id, 0, vl2.vlan_request, false)) {
            LCPD_LOG(LOG_ERR, "The vl2 (%d) add failure, can not alloc "
                    "vlantag in all racks, expected vlan is %u.\n",
                    mvl2->vl2_id, vl2.vlan_request);
            vl2.err |= LC_VL2_ERRNO_NO_VLAN_ID;
            goto error;
        }
    }
    lc_network_msg_reply(&vl2, TALKER__RESULT__SUCCESS, m_head->seq,
             TALKER__NETWORKS_OPS__NETWORKS_VL2_CREATE, LC_BUS_QUEUE_TALKER);
    return 0;

error:

    lc_vmdb_vl2_update_errno(vl2.err, vl2.id);
    lc_vmdb_vl2_update_state(LC_VL2_STATE_ABNORMAL, vl2.id);
    lc_network_msg_reply(&vl2, TALKER__RESULT__FAIL, m_head->seq,
             TALKER__NETWORKS_OPS__NETWORKS_VL2_CREATE, LC_BUS_QUEUE_TALKER);
    return -1;
}

/* alloc vlantag for a vl2 in all racks, when VMware vm created. */
static int lc_vl2_change (struct lc_msg_head *m_head)
{
    intf_t intfs_info[VL2_MAX_VIF_NUM];
    vl2_t vl2 = {0};
    vnet_t vnet_info = {0};
    vm_t vm_info = {0};
    struct msg_vl2_opt *mvl2 = NULL;
    char interface_id[VL2_MAX_POTS_ID_LEN] = {0};
    char vlantag_s[VL2_MAX_POTS_VI_LEN] = {0};
    char condition[VL2_MAX_APP_CD_LEN] = {0};
    int i, vif_num = 0, res = 0;
    uint32_t vlantag = 0;
    lc_mbuf_t msg;
    struct msg_vm_opt *m_vm_opt;
    struct msg_vm_opt_start *m_vm_start;
    intf_core_t *iface = 0;
    struct msg_hwdev_opt *m_hwdev_opt;
    struct msg_hwdev_opt_start *m_hwdev_start;

    memset(intfs_info, 0, sizeof(intfs_info));
    memset(&msg, 0, sizeof(lc_mbuf_t));

    mvl2 = (struct msg_vl2_opt *)(m_head + 1);
    LCPD_LOG(LOG_INFO, "The vl2 (%d) change\n",mvl2->vl2_id);
    vl2.id = mvl2->vl2_id;

    if (lc_vmdb_vl2_load(&vl2, mvl2->vl2_id)) {
        LCPD_LOG(LOG_ERR, "The vl2 (%d) change failure, vl2 not exist\n",mvl2->vl2_id);
        vl2.err |= LC_VL2_ERRNO_DB_ERROR;
        goto error;
    }

    snprintf(condition, VL2_MAX_APP_CD_LEN, "subnetid=%u", mvl2->vl2_id);
    if (lc_vmdb_vif_load_all(intfs_info, "*",
                condition, &vif_num) == 0) {
        for (i = 0; i < vif_num; ++i) {
            if (intfs_info[i].if_devicetype == LC_VIF_DEVICE_TYPE_VM &&
                !is_server_mode_xen(intfs_info[i].if_devicetype, intfs_info[i].if_deviceid)) {
                continue;
            }
            snprintf(interface_id, VL2_MAX_NETS_VI_LEN, "%u", intfs_info[i].if_id);
            del_vl2_tunnel_policies(interface_id);
        }
    }

    if (lc_db_alloc_vlantag(&vlantag, vl2.id, mvl2->rack_id, mvl2->vlantag, true)) {
        LCPD_LOG(LOG_ERR, "The vl2 (%d) change failure, can not alloc "
                "vlantag in rack-%d, expected vlan is %u.\n",
                 mvl2->vl2_id, mvl2->rack_id, 0);
        vl2.err |= LC_VL2_ERRNO_NO_VLAN_ID;
        goto error;
    }

    /* VMware: nothing to do */

    if (lc_vmdb_vif_update_vlantag_by_vl2id(vlantag, mvl2->vl2_id)) {
        LCPD_LOG(LOG_ERR, "Update vif vlantag in vl2-%d error.\n", mvl2->vl2_id);
        vl2.err |= LC_VL2_ERRNO_DB_ERROR;
        goto error;
    }

    /* enable vlan config */
    snprintf(vlantag_s, VL2_MAX_POTS_VI_LEN, "%u", vlantag);
    for (i = 0; i < vif_num; ++i) {
        snprintf(interface_id, VL2_MAX_NETS_VI_LEN, "%u", intfs_info[i].if_id);
        //res += enable_tunnel(interface_id, NETWORK_DEVICE_TYPE_VDEV, 0);

        if (intfs_info[i].if_devicetype == LC_VIF_DEVICE_TYPE_VM &&
            !is_server_mode_xen(intfs_info[i].if_devicetype, intfs_info[i].if_deviceid)) {
            continue;
        }

        if (intfs_info[i].if_devicetype == LC_VIF_DEVICE_TYPE_VM) {
            msg.hdr.type = LC_MSG_VINTERFACE_ATTACH;
            msg.hdr.length = sizeof(struct msg_vm_opt) + sizeof(struct msg_vm_opt_start);
            m_vm_opt = (struct msg_vm_opt *)msg.data;
            m_vm_start = (struct msg_vm_opt_start *)m_vm_opt->data;
            iface = &(m_vm_start->vm_ifaces[0]);

            memset(&vm_info, 0, sizeof(vm_info));
            if (lc_vmdb_vm_load(&vm_info, intfs_info[i].if_deviceid) == 0) {
                m_vm_opt->vm_id = vm_info.vm_id;
                strncpy(m_vm_start->vm_server, vm_info.launch_server, MAX_HOST_ADDRESS_LEN);
                iface->if_id = intfs_info[i].if_id;
                iface->if_index = intfs_info[i].if_index;
                iface->if_subnetid = intfs_info[i].if_subnetid;
                iface->if_vlan = intfs_info[i].if_vlan;
                iface->if_bandw = intfs_info[i].if_bandw;
                request_kernel_attach_interface(&msg.hdr, 1);
            } else {
                res += -1;
                vl2.err |= LC_VL2_ERRNO_AGEXEC_ERROR;
            }
#if 0
        } else if (intfs_info[i].if_devicetype == LC_VIF_DEVICE_TYPE_GW) {
            memset(&vnet_info, 0, sizeof(vnet_info));
            if (lc_vmdb_vnet_load(&vnet_info, intfs_info[i].if_deviceid) == 0) {
                enable_vlan(interface_id, vnet_info.launch_server, vlantag_s,
                        NETWORK_DEVICE_TYPE_VDEV, 0);
            } else {
                res += -1;
                vl2.err |= LC_VL2_ERRNO_AGEXEC_ERROR;
            }
#endif
        } else if (intfs_info[i].if_devicetype == LC_VIF_DEVICE_TYPE_VGATEWAY) {
            memset(&vnet_info, 0, sizeof(vnet_info));
            if (lc_vmdb_vnet_load(&vnet_info, intfs_info[i].if_deviceid) == 0) {
                nsp_vgws_migrate(&vnet_info, vnet_info.launch_server);
            } else {
                res += -1;
                vl2.err |= LC_VL2_ERRNO_AGEXEC_ERROR;
            }
        } else if (intfs_info[i].if_devicetype == LC_VIF_DEVICE_TYPE_HWDEV) {
            msg.hdr.type = LC_MSG_HWDEV_IF_ATTACH;
            msg.hdr.length = sizeof(struct msg_hwdev_opt) + sizeof(struct msg_hwdev_opt_start);
            m_hwdev_opt = (struct msg_hwdev_opt *)msg.data;
            m_hwdev_start = (struct msg_hwdev_opt_start *)m_hwdev_opt->data;
            m_hwdev_opt->vl2_id = mvl2->vl2_id;
            m_hwdev_opt->hwdev_id= intfs_info[i].if_deviceid;
            m_hwdev_opt->vl2_type = VIF_TYPE_LAN;
            m_hwdev_start->switch_port = intfs_info[i].if_port;
            m_hwdev_start->hwdev_iface.if_id = intfs_info[i].if_id;
            m_hwdev_start->hwdev_iface.if_bandw = intfs_info[i].if_bandw;
            request_kernel_add_hwdevice(&msg.hdr);
        } else {
            LCPD_LOG(LOG_ERR, "Unkown interface %d devicetype.\n", intfs_info[i].if_id);
            res += -1;
            vl2.err |= LC_VL2_ERRNO_DB_ERROR;
        }
    }
    if (res != 0) {
        LCPD_LOG(LOG_ERR, "The vl2 (%d/%s) config vlan of vl2 error.\n",
                mvl2->vl2_id, vl2.name);
        goto error;
    }

    LCPD_LOG(LOG_INFO, "The vl2 (%d/%s) change finish\n",
              mvl2->vl2_id, vl2.name);
    lc_vmdb_vl2_update_state(LC_VL2_STATE_FINISH, vl2.id);
    lc_network_msg_reply(&vl2, TALKER__RESULT__SUCCESS, m_head->seq,
             TALKER__NETWORKS_OPS__NETWORKS_VL2_MODIFY, LC_BUS_QUEUE_TALKER);
    return 0;

error:
    lc_vmdb_vl2_update_errno(vl2.err, vl2.id);
    lc_vmdb_vl2_update_state(LC_VL2_STATE_ABNORMAL, vl2.id);
    lc_network_msg_reply(&vl2, TALKER__RESULT__FAIL, m_head->seq,
             TALKER__NETWORKS_OPS__NETWORKS_VL2_MODIFY, LC_BUS_QUEUE_TALKER);
    return -1;
}
