#include <pthread.h>
#include <assert.h>
#include "lc_livegate_api.h"
#include "lc_db_errno.h"
#include "lc_db_log.h"
#include "vl2/ext_vl2_handler.h"

// Functions open for inner call
static int get_gateway_of_rack(char *rack, char *gw_host[]);
static int pack_gre_lg_post(char *buf, int buf_len,
                            char *tunnel_protocol,
                            char *remote_ip);
static int pack_tunnel_qos_lg_post(char *buf, int buf_len,
                                   uint32_t min,
                                   uint32_t max);
static int pack_vif_lg_post(char *buf, int buf_len,
                            int vl2_id,
                            int vlantag,
                            int vif_id,
                            char *vif_mac);
static int pack_vl2_lg_post(char *buf, int buf_len,
                            int vl2_id,
                            int vlantag);
static int pack_uplink_lg_post(char *buf, int buf_len,
                               char *address,
                               char *netmask,
                               char *gateway);

#define INSERT_NOT_EXIST_POLICY          0x01
#define DELETE_VL2_TUNNEL_POLICY         0x02
#define DISABLE_VL2_TUNNEL_POLICY        0x04

#define CREATE_RACK_TUNNEL_CALL_TYPE     1
#define REMOVE_RACK_TUNNEL_CALL_TYPE     2
#define ENABLE_TUNNEL_CALL_TYPE          1
#define DISABLE_TUNNEL_CALL_TYPE         2
#define ADD_TUNNEL_POLICY_CALL_TYPE      1
#define DEL_TUNNEL_POLICY_CALL_TYPE      2
#define TABLE_VM_STATE_EXCEPTION         11
#define TABLE_VM_ERRNO_NETWORK           LC_VM_ERRNO_NETWORK
#define TABLE_VGW_STATE_EXCEPTION        3
#define TABLE_VGW_ERRNO_NETWORK          LC_VEDGE_ERRNO_NETWORK
#define ADD_SECU_CALL_TYPE               1
#define DEL_SECU_CALL_TYPE               2

static int get_gateway_of_rack(char *rackid, char *gw_host[])
{
    char conditions[VL2_MAX_DB_BUF_LEN];

    snprintf(conditions, VL2_MAX_DB_BUF_LEN,
            "%s=%s AND %s!='' AND %s IS NOT NULL",
            TABLE_COMPUTE_RESOURCE_COLUMN_RACKID, rackid,
            TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP,
            TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP);
    if (ext_vl2_db_lookups_v2(TABLE_COMPUTE_RESOURCE,
                TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP, conditions, gw_host, VL2_MAX_LCGS_NUM)) {
        VL2_SYSLOG(LOG_ERR,
                "@WARNING (APP): %s rack %s has no gateway server, %s.\n",
                __FUNCTION__, rackid, conditions);
        return _VL2_FALSE_;
    }

    return _VL2_TRUE_;
}

int config_gateway_network(host_t *compute_resource)
{
    /* used for nsp host join */
    char value[VL2_MAX_DB_BUF_LEN] = {0};
    char conditions[VL2_MAX_DB_BUF_LEN];
    char gw_hosts[VL2_MAX_LCGS_NUM][VL2_MAX_INTERF_NW_LEN] = {{0}};
    char *pgw_hosts[VL2_MAX_LCGS_NUM];
    char remote_hosts[VL2_MAX_RACKS_NUM * VL2_MAX_LCGS_NUM][VL2_MAX_INTERF_NW_LEN] = {{0}};
    char *premote_hosts[VL2_MAX_RACKS_NUM * VL2_MAX_LCGS_NUM];
    char url[LG_API_URL_SIZE] = {0};
    char url2[LG_API_URL_SIZE] = {0};
    char post[LG_API_BIG_RESULT_SIZE] = {0};
    char result[LG_API_BIG_RESULT_SIZE] = {0};
    char gw_ctrl_ip[MAX_HOST_ADDRESS_LEN] = {0};
    char vifmac[VL2_MAX_INTERF_DL_LEN] = {0};
    char tunnel_protocol[TUNNEL_PROTOCOL_MAX_LEN] = {0};
    char tmp_ip[MAX_HOST_ADDRESS_LEN] = {0};
    char tmp_peer_ip[MAX_HOST_ADDRESS_LEN] = {0};
    char domain[LCUUID_LEN] = {0};
    int vif_ids[RACK_MAX_VIF] = {0};
    int vl2_ids[RACK_MAX_VL2] = {0};
    int vlantag = 0, vdevice_type = 0, vdevice_id = 0, vl2_id = 0;
    int rackid = 0, res = 0;
    int i, err = 0, torswitch_type = 0;
    uint32_t min_rate, max_rate;
    torswitch_t torswitch;

    if (!compute_resource || compute_resource->uplink_ip[0] == 0) {
        return _VL2_FALSE_;
    }

    /* config uplink */
    if (compute_resource->uplink_netmask[0] && compute_resource->uplink_gateway[0]) {
        snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, compute_resource->ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
        snprintf(url2, LG_API_URL_SIZE, LCPD_API_UPLINK_PATCH_LG);
        strcat(url, url2);
        err = pack_uplink_lg_post(post, LG_API_URL_SIZE,
                compute_resource->uplink_ip, compute_resource->uplink_netmask, compute_resource->uplink_gateway);
        if (err) {
            return _VL2_FALSE_;
        }
        err = lcpd_call_api(url, API_METHOD_PATCH, post, NULL);
        if (err) {
            return _VL2_FALSE_;
        }
    } else {
        VL2_SYSLOG(LOG_INFO, "@INFO (APP): %s no uplink netmask or gateway.\n",
                   __FUNCTION__);
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d", TABLE_RACK_COLUMN_ID, compute_resource->rackid);
    err = ext_vl2_db_lookup_ids(TABLE_RACK, TABLE_RACK_COLUMN_TORSWITCH_TYPE, conditions, &torswitch_type);
    if (err) {
        VL2_SYSLOG(LOG_ERR, "%s failed get flag of rack%d", __FUNCTION__, compute_resource->rackid);
        return _VL2_FALSE_;
    }

    if (torswitch_type != TORSWITCH_TYPE_ETHERNET) {
        return _VL2_TRUE_;
    }

    for (i = 0; i < VL2_MAX_LCGS_NUM; i++) {
        pgw_hosts[i] = gw_hosts[i];
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "ip='%s'", compute_resource->ip);
    err = ext_vl2_db_lookup(TABLE_COMPUTE_RESOURCE, TABLE_COMPUTE_RESOURCE_COLUMN_DOMAIN, conditions, domain);
    if (err) {
        VL2_SYSLOG(LOG_ERR, "%s failed get domain of compute_resource %s", __FUNCTION__, compute_resource->ip);
        return _VL2_FALSE_;
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
            TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
            TABLE_DOMAIN_CONFIGURATION_TUNNEL_PROTOCOL,
            TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
            domain);
    if (ext_vl2_db_lookup(TABLE_DOMAIN_CONFIGURATION, TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE, conditions, tunnel_protocol)) {
        VL2_SYSLOG(LOG_ERR, "@error (app): %s cannot find tunnel protocol\n", __FUNCTION__);
        return _VL2_FALSE_;
    }

    /* get other hosts of the same rack */
    snprintf(conditions, VL2_MAX_DB_BUF_LEN,
            "%s=%d AND %s!='' AND %s IS NOT NULL AND %s!='%s'",
            TABLE_COMPUTE_RESOURCE_COLUMN_RACKID, compute_resource->rackid,
            TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP,
            TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP,
            TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP,
            compute_resource->uplink_ip);
    if (ext_vl2_db_lookups_v2(TABLE_COMPUTE_RESOURCE,
                TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP, conditions, pgw_hosts, VL2_MAX_LCGS_NUM)) {
        return _VL2_TRUE_;
    }

    /* config tunnel qos */
    memset(value, 0, sizeof(value));
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'",
            TABLE_UPLINK_QOS_COLUMN_UPLINK_IP, pgw_hosts[0]);
    if(ext_vl2_db_lookup(TABLE_UPLINK_QOS,
            TABLE_UPLINK_QOS_COLUMN_TUNNEL_MIN_RATE, conditions, value)) {
        return _VL2_TRUE_;
    }
    min_rate = atoi(value);

    memset(value, 0, sizeof(value));
    ext_vl2_db_lookup(TABLE_UPLINK_QOS,
            TABLE_UPLINK_QOS_COLUMN_TUNNEL_MAX_RATE, conditions, value);
    max_rate = atoi(value);

    if (max_rate) {
        min_rate *= ONE_TRILLION_BANDWIDTH;
        max_rate *= ONE_TRILLION_BANDWIDTH;
        snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, compute_resource->ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
        snprintf(url2, LG_API_URL_SIZE, LCPD_API_TUNNEL_QOS_POST_LG);
        strcat(url, url2);
        err = pack_tunnel_qos_lg_post(post, LG_API_URL_SIZE, min_rate, max_rate);
        if (err) {
            return _VL2_FALSE_;
        }
        err = lcpd_call_api(url, API_METHOD_POST, post, NULL);
        if (err) {
            return _VL2_FALSE_;
        }
    }
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s,%s,%s",
            TABLE_UPLINK_QOS_COLUMN_UPLINK_IP,
            TABLE_UPLINK_QOS_COLUMN_TUNNEL_MIN_RATE,
            TABLE_UPLINK_QOS_COLUMN_TUNNEL_MAX_RATE);
    snprintf(value, VL2_MAX_DB_BUF_LEN, "'%s',%d,%d",
            compute_resource->uplink_ip, min_rate, max_rate);
    err = ext_vl2_db_insert(TABLE_UPLINK_QOS, conditions, value);
    if (err) {
        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s insert uplink_qos %s: [%d,%d] "
                "failed.\n", __FUNCTION__, compute_resource->uplink_ip, min_rate, max_rate);
        return _VL2_FALSE_;
    }

    /* config tunnels */
    for (i = 0; i < VL2_MAX_RACKS_NUM * VL2_MAX_LCGS_NUM; i++) {
        premote_hosts[i] = remote_hosts[i];
    }
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'",
            TABLE_RACK_TUNNEL_COLUMN_LOCAL_UPLINK_IP,
            pgw_hosts[0]);
    if (ext_vl2_db_lookups_v2(TABLE_RACK_TUNNEL,
                    TABLE_RACK_TUNNEL_COLUMN_REMOTE_UPLINK_IP, conditions, premote_hosts,
                    VL2_MAX_RACKS_NUM * VL2_MAX_LCGS_NUM)) {
        return _VL2_TRUE_;
    }
    for (i = 0; i < VL2_MAX_RACKS_NUM * VL2_MAX_LCGS_NUM && premote_hosts[i][0] != '\0'; i++) {
        /* config local host */
        snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, compute_resource->ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
        snprintf(url2, LG_API_URL_SIZE, LCPD_API_GRE_POST_LG);
        strcat(url, url2);
        err = pack_gre_lg_post(post, LG_API_URL_SIZE, tunnel_protocol, premote_hosts[i]);
        if (err) {
            return _VL2_FALSE_;
        }
        err = lcpd_call_api(url, API_METHOD_POST, post, NULL);
        if (err) {
            return _VL2_FALSE_;
        }
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s,%s",
                TABLE_RACK_TUNNEL_COLUMN_LOCAL_UPLINK_IP,
                TABLE_RACK_TUNNEL_COLUMN_REMOTE_UPLINK_IP);
        snprintf(value, VL2_MAX_DB_BUF_LEN, "'%s','%s'",
                compute_resource->uplink_ip, premote_hosts[i]);
        err = ext_vl2_db_insert(TABLE_RACK_TUNNEL, conditions, value);
        if (err) {
            VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s insert rack_tunnel %s->%s "
                    "failed.\n", __FUNCTION__, compute_resource->uplink_ip, premote_hosts[i]);
            return _VL2_FALSE_;
        }
        /* config remote host */
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'",
                TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP, premote_hosts[i]);
        err += ext_vl2_db_lookup(TABLE_COMPUTE_RESOURCE,
                TABLE_COMPUTE_RESOURCE_COLUMN_IP, conditions, gw_ctrl_ip);
        if (err) {
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'",
                    TABLE_NETWORK_DEVICE_COLUMN_TUNNEL_IP, premote_hosts[i]);
            if(ext_vl2_db_network_device_load(&torswitch, conditions)) {
                VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s %s is neither a host uplink ip nor switch tunnel ip.\n",
                        __FUNCTION__, premote_hosts[i]);
                return _VL2_FALSE_;
            }

            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d",
                    TABLE_NETWORK_DEVICE_COLUMN_ID, torswitch.id);
            err = ext_vl2_db_lookup_ids_v2(TABLE_NETWORK_DEVICE, TABLE_NETWORK_DEVICE_COLUMN_RACKID, conditions, &rackid, 1);
            if (err) {
                VL2_SYSLOG(LOG_ERR, "%s failed get rack of %s", __FUNCTION__, conditions);
                return _VL2_FALSE_;
            }

            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d", TABLE_RACK_COLUMN_ID, rackid);
            err = ext_vl2_db_lookup_ids_v2(TABLE_RACK, TABLE_RACK_COLUMN_TORSWITCH_TYPE, conditions, &torswitch_type, 1);
            if (err) {
                VL2_SYSLOG(LOG_ERR, "%s failed get switch type of rack%d", __FUNCTION__, rackid);
                return _VL2_FALSE_;
            }

            //insert db
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s,%s,%s",
                    TABLE_RACK_TUNNEL_COLUMN_LOCAL_UPLINK_IP,
                    TABLE_RACK_TUNNEL_COLUMN_REMOTE_UPLINK_IP,
                    TABLE_RACK_TUNNEL_COLUMN_SWITCH_IP);
            snprintf(value, VL2_MAX_DB_BUF_LEN, "'%s','%s','%s'",
                    premote_hosts[i], compute_resource->uplink_ip, torswitch.ip);
            err = ext_vl2_db_insert(TABLE_RACK_TUNNEL, conditions, value);
            if (err) {
                VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s insert rack_tunnel %s->%s "
                        "failed.\n", __FUNCTION__, premote_hosts[i], compute_resource->uplink_ip);
                return _VL2_FALSE_;
            }

            if (torswitch_type == TORSWITCH_TYPE_OPENFLOW) {
                LCPD_LOG(LOG_ERR, "switchtype OPENFLOW is deprecated");
                assert(0);
                return _VL2_FALSE_;
            } else if (torswitch_type == TORSWITCH_TYPE_TUNNEL) {
                snprintf(url, LG_API_URL_SIZE, SDN_API_PREFIX SDN_API_SWITCH_TUNNEL,
                        SDN_API_DEST_IP_SDNCTRL, SDN_CTRL_LISTEN_PORT, SDN_API_VERSION,
                        torswitch.ip, compute_resource->uplink_ip);
                LCPD_LOG(LOG_INFO, "Send url:%s\n", url);
                result[0] = 0;
                res = lcpd_call_api(url, API_METHOD_POST, NULL, result);
                if (res) {  // curl failed or api failed
                    LCPD_LOG(
                        LOG_ERR,
                        "failed to build tunnel in switch mip=%s, remote_ip=%s\n",
                        torswitch.ip, compute_resource->uplink_ip);
                    LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
                    snprintf(tmp_ip, MAX_HOST_ADDRESS_LEN, "%s", torswitch.tunnel_ip);
                    snprintf(tmp_peer_ip, MAX_HOST_ADDRESS_LEN, "%s", compute_resource->uplink_ip);
                    goto ERROR;
                }
            }
        } else {
            snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, gw_ctrl_ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
            snprintf(url2, LG_API_URL_SIZE, LCPD_API_GRE_POST_LG);
            strcat(url, url2);
            err = pack_gre_lg_post(post, LG_API_URL_SIZE, tunnel_protocol, compute_resource->uplink_ip);
            if (err) {
                return _VL2_FALSE_;
            }
            err = lcpd_call_api(url, API_METHOD_POST, post, NULL);
            if (err) {
                return _VL2_FALSE_;
            }
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s,%s",
                    TABLE_RACK_TUNNEL_COLUMN_LOCAL_UPLINK_IP,
                    TABLE_RACK_TUNNEL_COLUMN_REMOTE_UPLINK_IP);
            snprintf(value, VL2_MAX_DB_BUF_LEN, "'%s','%s'",
                    premote_hosts[i], compute_resource->uplink_ip);
            err = ext_vl2_db_insert(TABLE_RACK_TUNNEL, conditions, value);
            if (err) {
                VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s insert rack_tunnel %s->%s "
                        "failed.\n", __FUNCTION__, premote_hosts[i], compute_resource->uplink_ip);
                return _VL2_FALSE_;
            }
        }
    }

    /* config vif tunnel policy */
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'",
            TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP,
            pgw_hosts[0]);
    if(ext_vl2_db_lookup_ids_v2(TABLE_TUNNEL_POLICY,
        TABLE_TUNNEL_POLICY_COLUMN_VIFID, conditions, vif_ids, RACK_MAX_VIF)) {
        return _VL2_TRUE_;
    }
    for (i = 0; i < RACK_MAX_VIF && vif_ids[i]; i++) {
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s=%d",
                TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP, pgw_hosts[0],
                TABLE_TUNNEL_POLICY_COLUMN_VIFID, vif_ids[i]);
        err = ext_vl2_db_lookup(TABLE_TUNNEL_POLICY,
                TABLE_TUNNEL_POLICY_COLUMN_VIFMAC, conditions, vifmac);
        err += ext_vl2_db_lookup_ids_v2(TABLE_TUNNEL_POLICY,
                TABLE_TUNNEL_POLICY_COLUMN_VDEVICETYPE, conditions, &vdevice_type, 1);
        err += ext_vl2_db_lookup_ids_v2(TABLE_TUNNEL_POLICY,
                TABLE_TUNNEL_POLICY_COLUMN_VDEVICEID, conditions, &vdevice_id, 1);
        err += ext_vl2_db_lookup_ids_v2(TABLE_TUNNEL_POLICY,
                TABLE_TUNNEL_POLICY_COLUMN_VLANTAG, conditions, &vlantag, 1);
        err += ext_vl2_db_lookup_ids_v2(TABLE_TUNNEL_POLICY,
                TABLE_TUNNEL_POLICY_COLUMN_VL2ID, conditions, &vl2_id, 1);
        if (err) {
            VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s tunnel_policy with "
                    "id-%d has no mac/device_type/device_id/vlantag.\n",
                    __FUNCTION__, vif_ids[i]);
            return _VL2_FALSE_;
        }
        snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, compute_resource->ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
        snprintf(url2, LG_API_URL_SIZE, LCPD_API_VIF_VL2_POST_LG);
        strcat(url, url2);
        err = pack_vif_lg_post(post, LG_API_URL_SIZE, vl2_id, vlantag, vif_ids[i], vifmac);
        if (err) {
            return _VL2_FALSE_;
        }
        err = lcpd_call_api(url, API_METHOD_POST, post, NULL);
        if (err) {
            return _VL2_FALSE_;
        }
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s,%s,%s,%s,%s,%s,%s",
                TABLE_TUNNEL_POLICY_COLUMN_VIFID,
                TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP,
                TABLE_TUNNEL_POLICY_COLUMN_VDEVICETYPE,
                TABLE_TUNNEL_POLICY_COLUMN_VDEVICEID,
                TABLE_TUNNEL_POLICY_COLUMN_VL2ID,
                TABLE_TUNNEL_POLICY_COLUMN_VLANTAG,
                TABLE_TUNNEL_POLICY_COLUMN_VIFMAC);
        snprintf(value, VL2_MAX_DB_BUF_LEN, "%d,'%s',%d,%d,%d,%d,'%s'",
                vif_ids[i], compute_resource->uplink_ip, vdevice_type, vdevice_id,
                vl2_id, vlantag, vifmac);
        ext_vl2_db_insert(TABLE_TUNNEL_POLICY, conditions, value);
    }

    /* config vl2 tunnel policy */
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'",
            TABLE_VL2_TUNNEL_POLICY_COLUMN_UPLINK_IP,
            pgw_hosts[0]);
    if(ext_vl2_db_lookup_ids_v2(TABLE_VL2_TUNNEL_POLICY,
        TABLE_VL2_TUNNEL_POLICY_COLUMN_VL2ID, conditions, vl2_ids, RACK_MAX_VL2)) {
        return _VL2_TRUE_;
    }
    for (i = 0; i < RACK_MAX_VL2 && vl2_ids[i]; i++) {
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s=%d",
                TABLE_VL2_TUNNEL_POLICY_COLUMN_UPLINK_IP, pgw_hosts[0],
                TABLE_VL2_TUNNEL_POLICY_COLUMN_VL2ID, vl2_ids[i]);
        ext_vl2_db_lookup_ids_v2(TABLE_VL2_TUNNEL_POLICY,
                TABLE_VL2_TUNNEL_POLICY_COLUMN_VLANTAG, conditions, &vlantag, 1);
        snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, compute_resource->ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
        snprintf(url2, LG_API_URL_SIZE, LCPD_API_VIF_VL2_POST_LG);
        strcat(url, url2);
        err = pack_vl2_lg_post(post, LG_API_URL_SIZE, vl2_ids[i], vlantag);
        if (err) {
            return _VL2_FALSE_;
        }
        err = lcpd_call_api(url, API_METHOD_POST, post, NULL);
        if (err) {
            return _VL2_FALSE_;
        }
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s,%s,%s",
                TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP,
                TABLE_TUNNEL_POLICY_COLUMN_VL2ID,
                TABLE_TUNNEL_POLICY_COLUMN_VLANTAG);
        snprintf(value, VL2_MAX_DB_BUF_LEN, "'%s',%d,%d",
                compute_resource->uplink_ip, vl2_ids[i], vlantag);
        ext_vl2_db_insert(TABLE_VL2_TUNNEL_POLICY, conditions, value);
    }

    return _VL2_TRUE_;
ERROR:
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s='%s'",
            TABLE_RACK_TUNNEL_COLUMN_LOCAL_UPLINK_IP, tmp_ip,
            TABLE_RACK_TUNNEL_COLUMN_REMOTE_UPLINK_IP, tmp_peer_ip);
    err = ext_vl2_db_delete(TABLE_RACK_TUNNEL, conditions);
    if (err) {
        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s delete rack_tunnel %s->%s failed.\n",
                __FUNCTION__, tmp_ip, tmp_peer_ip);
    }
    return _VL2_FALSE_;
}

int config_gateway_uplink(host_t *compute_resource)
{
    char value[VL2_MAX_DB_BUF_LEN] = {0};
    char conditions[VL2_MAX_DB_BUF_LEN] = {0};
    char peer_buf[VL2_MAX_RACKS * VL2_MAX_LCGS_NUM][VL2_MAX_INTERF_NW_LEN];
    char *peers[VL2_MAX_RACKS * VL2_MAX_LCGS_NUM] = {0};
    int  ids[RACK_MAX_VIF] = {0};
    int  vl2_ids[RACK_MAX_VL2] = {0};
    char mac[VL2_MAX_INTERF_DL_LEN] = {0};
    char tunnel_protocol[TUNNEL_PROTOCOL_MAX_LEN] = {0};
    char url[LG_API_URL_SIZE] = {0};
    char url2[LG_API_URL_SIZE] = {0};
    char post[LG_API_URL_SIZE] = {0};
    char domain[LCUUID_LEN] = {0};
    uint32_t min_rate, max_rate;
    int vl2_id, vlantag = 0;
    int i, err = 0, torswitch_type = 0;

    if (!compute_resource || compute_resource->uplink_ip[0] == 0) {
        VL2_SYSLOG(LOG_INFO, "@INFO (APP): %s no uplink ip.\n",
                   __FUNCTION__);
        return _VL2_FALSE_;
    }

    memset(peer_buf, 0, sizeof(peer_buf));
    for (i = 0; i < VL2_MAX_RACKS * VL2_MAX_LCGS_NUM; ++i) {
        peers[i] = peer_buf[i];
    }

    /* config uplink */
    if (compute_resource->uplink_netmask[0] && compute_resource->uplink_gateway[0]) {
        snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, compute_resource->ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
        snprintf(url2, LG_API_URL_SIZE, LCPD_API_UPLINK_PATCH_LG);
        strcat(url, url2);
        err = pack_uplink_lg_post(post, LG_API_URL_SIZE,
                compute_resource->uplink_ip, compute_resource->uplink_netmask, compute_resource->uplink_gateway);
        if (err) {
            VL2_SYSLOG(LOG_ERR, "%s: pack_uplink_lg_post failed.\n",
                       __FUNCTION__);
            return _VL2_FALSE_;
        }
        err = lcpd_call_api(url, API_METHOD_PATCH, post, NULL);
        if (err) {
            VL2_SYSLOG(LOG_ERR, "%s: lcpd_call_api failed.\n",
                       __FUNCTION__);
            return _VL2_FALSE_;
        }
    } else {
        VL2_SYSLOG(LOG_INFO, "@INFO (APP): %s no uplink netmask or gateway.\n",
                   __FUNCTION__);
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d", TABLE_RACK_COLUMN_ID, compute_resource->rackid);
    err = ext_vl2_db_lookup_ids(TABLE_RACK, TABLE_RACK_COLUMN_TORSWITCH_TYPE, conditions, &torswitch_type);
    if (err) {
        VL2_SYSLOG(LOG_ERR, "%s failed get flag of rack%d", __FUNCTION__, compute_resource->rackid);
        return _VL2_FALSE_;
    }

    if (torswitch_type != TORSWITCH_TYPE_ETHERNET) {
        VL2_SYSLOG(LOG_INFO, "@INFO (APP): %s torswitch is not "
                "ETHERNET, skip tunnel config.\n", __FUNCTION__);
        return _VL2_TRUE_;
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'", TABLE_COMPUTE_RESOURCE_COLUMN_IP, compute_resource->ip);
    err = ext_vl2_db_lookup(TABLE_COMPUTE_RESOURCE, TABLE_COMPUTE_RESOURCE_COLUMN_DOMAIN, conditions, domain);
    if (err) {
        VL2_SYSLOG(LOG_ERR, "%s failed get domain of compute_resource %s", __FUNCTION__, compute_resource->ip);
        return _VL2_FALSE_;
    }

    /* config tunnel qos */

    memset(value, 0, sizeof(value));
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'",
            TABLE_UPLINK_QOS_COLUMN_UPLINK_IP,
            compute_resource->uplink_ip);
    ext_vl2_db_lookup(TABLE_UPLINK_QOS,
            TABLE_UPLINK_QOS_COLUMN_TUNNEL_MIN_RATE, conditions, value);
    min_rate = atoi(value);

    memset(value, 0, sizeof(value));
    ext_vl2_db_lookup(TABLE_UPLINK_QOS,
            TABLE_UPLINK_QOS_COLUMN_TUNNEL_MAX_RATE, conditions, value);
    max_rate = atoi(value);

    if (max_rate) {
        min_rate *= ONE_TRILLION_BANDWIDTH;
        max_rate *= ONE_TRILLION_BANDWIDTH;
        snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, compute_resource->ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
        snprintf(url2, LG_API_URL_SIZE, LCPD_API_TUNNEL_QOS_POST_LG);
        strcat(url, url2);
        err = pack_tunnel_qos_lg_post(post, LG_API_URL_SIZE, min_rate, max_rate);
        if (err) {
            return _VL2_FALSE_;
        }
        err = lcpd_call_api(url, API_METHOD_PATCH, post, NULL);
        if (err) {
            return _VL2_FALSE_;
        }
    }

    /* config tunnels */

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
            TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
            TABLE_DOMAIN_CONFIGURATION_TUNNEL_PROTOCOL,
            TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
            domain);
    if (ext_vl2_db_lookup(TABLE_DOMAIN_CONFIGURATION, TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE, conditions, tunnel_protocol)) {
        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s cannot find tunnel protocol\n", __FUNCTION__);
        return _VL2_FALSE_;
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'",
            TABLE_RACK_TUNNEL_COLUMN_LOCAL_UPLINK_IP,
            compute_resource->uplink_ip);
    ext_vl2_db_lookups(TABLE_RACK_TUNNEL,
            TABLE_RACK_TUNNEL_COLUMN_REMOTE_UPLINK_IP, conditions, peers);
    for (i = 0; i < VL2_MAX_RACKS * VL2_MAX_LCGS_NUM && peers[i] && peers[i][0]; ++i) {
        snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, compute_resource->ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
        snprintf(url2, LG_API_URL_SIZE, LCPD_API_GRE_POST_LG);
        strcat(url, url2);
        err = pack_gre_lg_post(post, LG_API_URL_SIZE, tunnel_protocol, peers[i]);
        if (err) {
            return _VL2_FALSE_;
        }
        err = lcpd_call_api(url, API_METHOD_POST, post, NULL);
        if (err) {
            return _VL2_FALSE_;
        }
    }

    /* config tunnel policies */

    memset(ids, 0, sizeof(ids));
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'",
            TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP,
            compute_resource->uplink_ip);
    ext_vl2_db_lookup_ids_v2(TABLE_TUNNEL_POLICY,
            TABLE_TUNNEL_POLICY_COLUMN_VIFID, conditions, ids, RACK_MAX_VIF);
    for (i = 0; i < RACK_MAX_VIF && ids[i]; ++i) {
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d",
                TABLE_TUNNEL_POLICY_COLUMN_VIFID, ids[i]);
        err += ext_vl2_db_lookup_ids_v2(TABLE_TUNNEL_POLICY,
                TABLE_TUNNEL_POLICY_COLUMN_VL2ID, conditions, &vl2_id, 1);
        err += ext_vl2_db_lookup_ids_v2(TABLE_TUNNEL_POLICY,
                TABLE_TUNNEL_POLICY_COLUMN_VLANTAG, conditions, &vlantag, 1);
        err += ext_vl2_db_lookup(TABLE_TUNNEL_POLICY,
                TABLE_TUNNEL_POLICY_COLUMN_VIFMAC, conditions, mac);

        if (vl2_id && vlantag && mac[0]) {
            snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, compute_resource->ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
            snprintf(url2, LG_API_URL_SIZE, LCPD_API_VIF_VL2_POST_LG);
            strcat(url, url2);
            err = pack_vif_lg_post(post, LG_API_URL_SIZE, vl2_id, vlantag, ids[i], mac);
            if (err) {
                return _VL2_FALSE_;
            }
            err = lcpd_call_api(url, API_METHOD_POST, post, NULL);
            if (err) {
                return _VL2_FALSE_;
            }
        }
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'",
            TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP,
            compute_resource->uplink_ip);
    ext_vl2_db_lookup_ids_v2(TABLE_VL2_TUNNEL_POLICY,
            TABLE_TUNNEL_POLICY_COLUMN_VL2ID, conditions, vl2_ids, RACK_MAX_VL2);
    for (i = 0; i < RACK_MAX_VL2 && vl2_ids[i]; ++i) {
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d AND (%s=%d OR %s=0)",
                TABLE_VL2_VLAN_COLUMN_VL2ID, vl2_ids[i],
                TABLE_VL2_VLAN_COLUMN_RACKID, compute_resource->rackid,
                TABLE_VL2_VLAN_COLUMN_RACKID);
        err += ext_vl2_db_lookup_ids_v2(TABLE_VL2_VLAN,
                TABLE_VL2_VLAN_COLUMN_VLANTAG, conditions, &vlantag, 1);

        if (vlantag) {
            snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, compute_resource->ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
            snprintf(url2, LG_API_URL_SIZE, LCPD_API_VIF_VL2_POST_LG);
            strcat(url, url2);
            err = pack_vl2_lg_post(post, LG_API_URL_SIZE, vl2_ids[i], vlantag);
            if (err) {
                return _VL2_FALSE_;
            }
            err = lcpd_call_api(url, API_METHOD_POST, post, NULL);
            if (err) {
                return _VL2_FALSE_;
            }
        }
    }

    if (err) {
        VL2_SYSLOG(LOG_ERR, "@ERR (APP): %s error err=%d.\n",
                __FUNCTION__, err);
        return _VL2_FALSE_;
    }

    return _VL2_TRUE_;
}

int get_rack_of_vif(int vifid, char *rackid)
{
    char value[VL2_MAX_DB_BUF_LEN] = {0};
    char conditions[VL2_MAX_DB_BUF_LEN] = {0};
    int devicetype = 0, deviceid = 0;
    int err;

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "id=%d", vifid);
    err = ext_vl2_db_lookup_ids_v2("vinterface", "devicetype", conditions, &devicetype, 1);
    err += ext_vl2_db_lookup_ids_v2("vinterface", "deviceid", conditions, &deviceid, 1);
    if (err) {
        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s can not get devicetype/deviceid of interface, "
                "%s, err=%d.\n", __FUNCTION__, conditions, err);
        return _VL2_FALSE_;
    }

    if (devicetype == VIF_DEVICE_TYPE_HWDEV) {
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "id=%d", deviceid);
        err = ext_vl2_db_lookup("third_party_device", "rack_name", conditions, value);
        if (err) {
            VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s can not get rack name of %s, "
                    "%s, err=%d.\n", __FUNCTION__,
                    "hwdev", conditions, err);
            return _VL2_FALSE_;
        }

        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "name='%s'", value);
        err = ext_vl2_db_lookup("rack", "id", conditions, rackid);
        if (err) {
            VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s can not get rack of hwdev, "
                    "%s, err=%d.\n", __FUNCTION__, conditions, err);
            return _VL2_FALSE_;
        }
    } else {
        if (devicetype == VIF_DEVICE_TYPE_VGW || devicetype == VIF_DEVICE_TYPE_VGATEWAY) {
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "id=%d", deviceid);
            err = ext_vl2_db_lookup("vnet", "gw_launch_server", conditions, value);
        } else if (devicetype == VIF_DEVICE_TYPE_VM) {
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "id=%d", deviceid);
            err = ext_vl2_db_lookup("vm", "launch_server", conditions, value);
        }
        if (err) {
            VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s can not get host-ip of %s, "
                    "%s, err=%d.\n", __FUNCTION__,
                    (devicetype == VIF_DEVICE_TYPE_VGW ||
                     devicetype == VIF_DEVICE_TYPE_VGATEWAY
                     ? "vnet" : "vm"), conditions, err);
            return _VL2_FALSE_;
        }

        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "ip='%s'", value);
        err = ext_vl2_db_lookup("host_device", "rackid", conditions, rackid);
        if (err) {
            VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s can not get rack of host, "
                    "%s, err=%d.\n", __FUNCTION__, conditions, err);
            return _VL2_FALSE_;
        }
    }

    return _VL2_TRUE_;
}

int get_rack_of_hwdev(char *hwdev_id, char *rackid)
{
    char value[VL2_MAX_DB_BUF_LEN];
    char conditions[VL2_MAX_DB_BUF_LEN];
    int  err;

    memset(value, 0, sizeof(value));
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "id=%s", hwdev_id);
    err = ext_vl2_db_lookup("third_party_device", "rack_name", conditions, value);
    if (err) {
        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s can not get rack name of %s, "
                "%s, err=%d.\n", __FUNCTION__,
                "hwdev", conditions, err);
        return _VL2_FALSE_;
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "name='%s'", value);
    err = ext_vl2_db_lookup("rack", "id", conditions, rackid);
    if (err) {
        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s can not get rack of hwdev, "
                "%s, err=%d.\n", __FUNCTION__, conditions, err);
        return _VL2_FALSE_;
    }

    return _VL2_TRUE_;
}

int get_rack_of_vdevice(char *vdevice_id, bool is_vgw, char *rackid)
{
    char value[VL2_MAX_DB_BUF_LEN];
    char conditions[VL2_MAX_DB_BUF_LEN];
    int  err;

    memset(value, 0, sizeof(value));
    if (is_vgw) {
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "id=%s", vdevice_id);
        err = ext_vl2_db_lookup("vnet", "gw_launch_server", conditions, value);
    } else {
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "id=%s", vdevice_id);
        err = ext_vl2_db_lookup("vm", "launch_server", conditions, value);
    }
    if (err) {
        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s can not get host-ip of %s, "
                "%s, err=%d.\n", __FUNCTION__,
                (is_vgw ? "vnet" : "vm"), conditions, err);
        return _VL2_FALSE_;
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "ip='%s'", value);
    err = ext_vl2_db_lookup("host_device", "rackid", conditions, rackid);
    if (err) {
        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s can not get rack of host, "
                "%s, err=%d.\n", __FUNCTION__, conditions, err);
        return _VL2_FALSE_;
    }

    return _VL2_TRUE_;
}

static int pack_gre_lg_post(char *buf, int buf_len,
                            char *tunnel_protocol,
                            char *remote_ip)
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    int len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_STRING, LCPD_API_GRE_LG_PROTOCOL, root);
    tmp->text_value = tunnel_protocol;

    tmp = create_json(NX_JSON_STRING, LCPD_API_GRE_RYU_REMOTE_IP, root);
    tmp->text_value = remote_ip;

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

static int pack_uplink_lg_post(char *buf, int buf_len,
                               char *address,
                               char *netmask,
                               char *gateway)
{
    nx_json *root = NULL;
    nx_json *tmp = NULL, *ip = NULL;
    int len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_STRING, LCPD_API_UPLINK_PATCH_LG_NAME, root);
    tmp->text_value = LCPD_API_UPLINK_PATCH_LG_NAME_UPLINK;

    ip = create_json(NX_JSON_OBJECT, LCPD_API_UPLINK_PATCH_LG_IP, root);

    tmp = create_json(NX_JSON_STRING, LCPD_API_UPLINK_PATCH_LG_ADDRESS, ip);
    tmp->text_value = address;

    tmp = create_json(NX_JSON_STRING, LCPD_API_UPLINK_PATCH_LG_NETMASK, ip);
    tmp->text_value = netmask;

    tmp = create_json(NX_JSON_STRING, LCPD_API_UPLINK_PATCH_LG_GATEWAY, root);
    tmp->text_value = gateway;

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

static int pack_tunnel_qos_lg_post(char *buf, int buf_len,
                                   uint32_t min,
                                   uint32_t max)
{
    nx_json *root = NULL;
    nx_json *tmp = NULL, *qos = NULL;
    int len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_STRING, LCPD_API_TUNNEL_QOS_LG_NAME, root);
    tmp->text_value = LCPD_API_TUNNEL_QOS_LG_NAME_TUNNEL;

    qos = create_json(NX_JSON_OBJECT, LCPD_API_TUNNEL_QOS_LG_QOS, root);

    tmp = create_json(NX_JSON_INTEGER, LCPD_API_TUNNEL_QOS_LG_MIN_BANDWIDTH, qos);
    tmp->int_value = min;

    tmp = create_json(NX_JSON_INTEGER, LCPD_API_TUNNEL_QOS_LG_MAX_BANDWIDTH, qos);
    tmp->int_value = max;

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

static int pack_vif_lg_post(char *buf, int buf_len,
                            int vl2_id,
                            int vlantag,
                            int vif_id,
                            char *vif_mac)
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    int len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_INTEGER, LCPD_API_VIF_VL2_LG_SUBNET_ID, root);
    tmp->int_value = vl2_id;

    tmp = create_json(NX_JSON_INTEGER, LCPD_API_VIF_VL2_LG_VLANTAG, root);
    tmp->int_value = vlantag;

    tmp = create_json(NX_JSON_INTEGER, LCPD_API_VIF_VL2_LG_VIF_ID, root);
    tmp->int_value = vif_id;

    tmp = create_json(NX_JSON_STRING, LCPD_API_VIF_VL2_LG_VIF_MAC, root);
    tmp->text_value = vif_mac;

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

int pack_vgateway_wan_update(char *buf, int buf_len,
                             vinterface_info_t *wansinfo)
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    nx_json *ips_arr = NULL;
    nx_json *qos_obj = NULL;
    nx_json *ip_obj = NULL;
    int j,len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;
    tmp = create_json(NX_JSON_INTEGER, "IF_INDEX", root);
    tmp->int_value = wansinfo->ifindex;
    tmp = create_json(NX_JSON_INTEGER, "ISP", root);
    tmp->int_value = wansinfo->ispid;
    tmp = create_json(NX_JSON_STRING, "GATEWAY", root);
    tmp->text_value = wansinfo->gateway;
    ips_arr = create_json(NX_JSON_ARRAY, "IPS", root);
    for (j=0; j < IP_MAX_NUM; j++) {
        if (wansinfo->ips[j].address[0] == 0) {
            break;
        }
        ip_obj = create_json(NX_JSON_OBJECT, NULL, ips_arr);
        tmp = create_json(NX_JSON_STRING, "ADDRESS", ip_obj);
        tmp->text_value = wansinfo->ips[j].address;
        tmp = create_json(NX_JSON_STRING, "NETMASK", ip_obj);
        tmp->text_value = wansinfo->ips[j].netmask;
    }
    qos_obj = create_json(NX_JSON_OBJECT, "QOS", root);
    tmp = create_json(NX_JSON_INTEGER, "MIN_BANDWIDTH", qos_obj);
    tmp->int_value = wansinfo->qos.minbandwidth;
    tmp = create_json(NX_JSON_INTEGER, "MAX_BANDWIDTH", qos_obj);
    tmp->int_value = wansinfo->qos.maxbandwidth;

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

int pack_vgateway_lan_update(char *buf, int buf_len,
                             vinterface_info_t *lansinfo)
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    nx_json *ips_arr = NULL;
    nx_json *qos_obj = NULL;
    nx_json *ip_obj = NULL;
    int j,len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_INTEGER, "IF_INDEX", root);
    tmp->int_value = lansinfo->ifindex;
    ips_arr = create_json(NX_JSON_ARRAY, "IPS", root);
    for (j=0; j< IP_MAX_NUM; j++) {
        if (lansinfo->ips[j].address[0] == 0) {
            break;
        }
        ip_obj = create_json(NX_JSON_OBJECT, NULL, ips_arr);
        tmp = create_json(NX_JSON_STRING, "ADDRESS", ip_obj);
        tmp->text_value = lansinfo->ips[j].address;
        tmp = create_json(NX_JSON_STRING, "NETMASK", ip_obj);
        tmp->text_value = lansinfo->ips[j].netmask;
    }
    tmp = create_json(NX_JSON_INTEGER, "VLANTAG", root);
    tmp->int_value = lansinfo->vlantag;
    qos_obj = create_json(NX_JSON_OBJECT, "QOS", root);
    tmp = create_json(NX_JSON_INTEGER, "MIN_BANDWIDTH", qos_obj);
    tmp->int_value = lansinfo->qos.minbandwidth;
    tmp = create_json(NX_JSON_INTEGER, "MAX_BANDWIDTH", qos_obj);
    tmp->int_value = lansinfo->qos.maxbandwidth;

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

int pack_vgateway_update(char *buf, int buf_len,
                     char *router_id,
                     int wan_count,
                     vinterface_info_t *wansinfo,
                     int lan_count,
                     vinterface_info_t *lansinfo,
                     int conn_max,
                     int new_conn_per_sec,
                     int role)
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    nx_json *wans_arr = NULL;
    nx_json *lans_arr = NULL;
    nx_json *wans_arr_wan_obj = NULL;
    nx_json *lans_arr_lan_obj = NULL;
    nx_json *ips_arr = NULL;
    nx_json *qos_obj = NULL;
    nx_json *ip_obj = NULL;
    nx_json *conn_limit_obj = NULL;
    int i,j,len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_INTEGER, "ID", root);
    tmp->int_value = atoi(router_id);

    wans_arr = create_json(NX_JSON_ARRAY, "WANS", root);
    for (i=0; i < wan_count; i++) {
        wans_arr_wan_obj = create_json(NX_JSON_OBJECT, NULL, wans_arr);
        tmp = create_json(NX_JSON_INTEGER, "IF_INDEX", wans_arr_wan_obj);
        tmp->int_value = wansinfo[i].ifindex;
        tmp = create_json(NX_JSON_STRING, "STATE", wans_arr_wan_obj);
        if (wansinfo[i].state == VIF_STATE_ATTACHED) {
            tmp->text_value = "ATTACH";
        } else {
            tmp->text_value = "DETACH";
            continue;
        }
        tmp = create_json(NX_JSON_INTEGER, "ISP", wans_arr_wan_obj);
        tmp->int_value = wansinfo[i].ispid;
        tmp = create_json(NX_JSON_STRING, "GATEWAY", wans_arr_wan_obj);
        tmp->text_value = wansinfo[i].gateway;
        tmp = create_json(NX_JSON_STRING, "MAC", wans_arr_wan_obj);
        tmp->text_value = wansinfo[i].mac;
        tmp = create_json(NX_JSON_INTEGER, "VLANTAG", wans_arr_wan_obj);
        tmp->int_value = wansinfo[i].vlantag;
        ips_arr = create_json(NX_JSON_ARRAY, "IPS", wans_arr_wan_obj);
        for (j=0; j < IP_MAX_NUM; j++) {
            if (wansinfo[i].ips[j].address[0] == 0) {
                break;
            }
            ip_obj = create_json(NX_JSON_OBJECT, NULL, ips_arr);
            tmp = create_json(NX_JSON_STRING, "ADDRESS", ip_obj);
            tmp->text_value = wansinfo[i].ips[j].address;
            tmp = create_json(NX_JSON_STRING, "NETMASK", ip_obj);
            tmp->text_value = wansinfo[i].ips[j].netmask;
        }
        qos_obj = create_json(NX_JSON_OBJECT, "QOS", wans_arr_wan_obj);
        tmp = create_json(NX_JSON_INTEGER, "MIN_BANDWIDTH", qos_obj);
        tmp->int_value = wansinfo[i].qos.minbandwidth;
        tmp = create_json(NX_JSON_INTEGER, "MAX_BANDWIDTH", qos_obj);
        tmp->int_value = wansinfo[i].qos.maxbandwidth;
        qos_obj = create_json(NX_JSON_OBJECT, "BROADCAST_QOS", wans_arr_wan_obj);
        tmp = create_json(NX_JSON_INTEGER, "MIN_BANDWIDTH", qos_obj);
        tmp->int_value = wansinfo[i].broadcast_qos.minbandwidth;
        tmp = create_json(NX_JSON_INTEGER, "MAX_BANDWIDTH", qos_obj);
        tmp->int_value = wansinfo[i].broadcast_qos.maxbandwidth;
    }

    lans_arr = create_json(NX_JSON_ARRAY, "LANS", root);
    for (i=0; i < lan_count; i++) {
        lans_arr_lan_obj = create_json(NX_JSON_OBJECT, NULL, lans_arr);
        tmp = create_json(NX_JSON_INTEGER, "IF_INDEX", lans_arr_lan_obj);
        tmp->int_value = lansinfo[i].ifindex;
        tmp = create_json(NX_JSON_STRING, "STATE", lans_arr_lan_obj);
        if (lansinfo[i].state == VIF_STATE_ATTACHED) {
            tmp->text_value = "ATTACH";
        } else {
            tmp->text_value = "DETACH";
            continue;
        }
        tmp = create_json(NX_JSON_STRING, "MAC", lans_arr_lan_obj);
        tmp->text_value = lansinfo[i].mac;
        ips_arr = create_json(NX_JSON_ARRAY, "IPS", lans_arr_lan_obj);
        for (j=0; j< IP_MAX_NUM; j++) {
            if (lansinfo[i].ips[j].address[0] == 0) {
                break;
            }
            ip_obj = create_json(NX_JSON_OBJECT, NULL, ips_arr);
            tmp = create_json(NX_JSON_STRING, "ADDRESS", ip_obj);
            tmp->text_value = lansinfo[i].ips[j].address;
            tmp = create_json(NX_JSON_STRING, "NETMASK", ip_obj);
            tmp->text_value = lansinfo[i].ips[j].netmask;
        }
        tmp = create_json(NX_JSON_INTEGER, "VLANTAG", lans_arr_lan_obj);
        tmp->int_value = lansinfo[i].vlantag;
        qos_obj = create_json(NX_JSON_OBJECT, "QOS", lans_arr_lan_obj);
        tmp = create_json(NX_JSON_INTEGER, "MIN_BANDWIDTH", qos_obj);
        tmp->int_value = lansinfo[i].qos.minbandwidth;
        tmp = create_json(NX_JSON_INTEGER, "MAX_BANDWIDTH", qos_obj);
        tmp->int_value = lansinfo[i].qos.maxbandwidth;
    }

    if (role == LC_ROLE_GATEWAY) {
        conn_limit_obj = create_json(NX_JSON_OBJECT, "CONN_LIMIT", root);
        tmp = create_json(NX_JSON_INTEGER, "CONN_MAX", conn_limit_obj);
        tmp->int_value = conn_max;
        tmp = create_json(NX_JSON_INTEGER, "NEW_CONN_PER_SEC", conn_limit_obj);
        tmp->int_value = new_conn_per_sec;
    }

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

static int pack_vl2_lg_post(char *buf, int buf_len,
                            int vl2_id,
                            int vlantag)
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    int len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_INTEGER, LCPD_API_VIF_VL2_LG_SUBNET_ID, root);
    tmp->int_value = vl2_id;

    tmp = create_json(NX_JSON_INTEGER, LCPD_API_VIF_VL2_LG_VLANTAG, root);
    tmp->int_value = vlantag;

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

static int config_tunnel(rack_network_t *rack, rack_network_t *peer_rack)
{
    int err = _VL2_TRUE_, id;
    uint32_t min_rate = 0, max_rate = 0;
    char value[VL2_MAX_DB_BUF_LEN] = {0};
    char conditions[VL2_MAX_DB_BUF_LEN] = {0};
    char gw_ctrl_ip[MAX_HOST_ADDRESS_LEN] = {0};
    char peer_gw_ctrl_ip[MAX_HOST_ADDRESS_LEN] = {0};
    char peer_gw_passwd[HOST_PASSWD_MAX_LEN] = {0};
    char url[LG_API_URL_SIZE] = {0};
    char url2[LG_API_URL_SIZE] = {0};
    char post[LG_API_BIG_RESULT_SIZE] = {0};
    char result[LG_API_BIG_RESULT_SIZE] = {0};
    char tunnel_protocol[TUNNEL_PROTOCOL_MAX_LEN] = {0};
    char tmp_ip[MAX_HOST_ADDRESS_LEN] = {0};
    char tmp_peer_ip[MAX_HOST_ADDRESS_LEN] = {0};
    char domain[LCUUID_LEN] = {0};
    int i,j, res = 0;


    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%s", TABLE_RACK_COLUMN_ID, rack->id);
    if (ext_vl2_db_lookup(TABLE_RACK, TABLE_RACK_COLUMN_DOMAIN, conditions, domain)) {
        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s cannot find domain for rack %s\n", __FUNCTION__, rack->id);
        return _VL2_FALSE_;
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' and %s='%s'",
            TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME,
            TABLE_DOMAIN_CONFIGURATION_TUNNEL_PROTOCOL,
            TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN,
            domain);
    if (ext_vl2_db_lookup(TABLE_DOMAIN_CONFIGURATION, TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE, conditions, tunnel_protocol)) {
        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s cannot find tunnel protocol\n", __FUNCTION__);
        return _VL2_FALSE_;
    }

    if (rack->torswitch_type != TORSWITCH_TYPE_ETHERNET) {
        for (i = 0; i< RACK_MAX_SWITCH_NUM && rack->torswitch[i].id; i++) {
            if (peer_rack->torswitch_type != TORSWITCH_TYPE_ETHERNET) {
                for (j = 0; j < RACK_MAX_SWITCH_NUM && peer_rack->torswitch[j].id; j++) {
                    //check exist
                    if (rack->torswitch[i].tunnel_ip[0] == '\0' ||
                        peer_rack->torswitch[j].tunnel_ip[0] == '\0') {
                        VL2_SYSLOG(LOG_DEBUG, "@INFO (APP): %s tunnel ip is empty: torswitch ip=%s, tunnel ip=%s; "
                                "torswitch ip=%s, tunnel ip=%s\n",
                                __FUNCTION__, rack->torswitch[i].ip, rack->torswitch[i].tunnel_ip,
                                peer_rack->torswitch[j].ip, peer_rack->torswitch[j].tunnel_ip);
                        continue;
                    }
                    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s='%s' AND %s='%s'",
                            TABLE_RACK_TUNNEL_COLUMN_LOCAL_UPLINK_IP, rack->torswitch[i].tunnel_ip,
                            TABLE_RACK_TUNNEL_COLUMN_REMOTE_UPLINK_IP, peer_rack->torswitch[j].tunnel_ip,
                            TABLE_RACK_TUNNEL_COLUMN_SWITCH_IP, rack->torswitch[i].ip);
                    err = ext_vl2_db_lookup_ids_v2(TABLE_RACK_TUNNEL,
                            TABLE_RACK_TUNNEL_COLUMN_ID, conditions, &id, 1);
                    if (err == 0 && id) {
                        VL2_SYSLOG(LOG_DEBUG, "@INFO (APP): %s tunnel %s->%s already exist on %s.\n",
                                __FUNCTION__, rack->torswitch[i].tunnel_ip,
                                peer_rack->torswitch[j].tunnel_ip, rack->torswitch[i].ip);
                        continue;
                    }
                    //insert db
                    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s,%s,%s",
                            TABLE_RACK_TUNNEL_COLUMN_LOCAL_UPLINK_IP,
                            TABLE_RACK_TUNNEL_COLUMN_REMOTE_UPLINK_IP,
                            TABLE_RACK_TUNNEL_COLUMN_SWITCH_IP);
                    snprintf(value, VL2_MAX_DB_BUF_LEN, "'%s','%s','%s'",
                            rack->torswitch[i].tunnel_ip, peer_rack->torswitch[j].tunnel_ip, rack->torswitch[i].ip);
                    err = ext_vl2_db_insert(TABLE_RACK_TUNNEL, conditions, value);
                    if (err) {
                        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s insert rack_tunnel %s->%s "
                                "failed.\n", __FUNCTION__, rack->torswitch[i].tunnel_ip, peer_rack->torswitch[j].tunnel_ip);
                        return _VL2_FALSE_;
                    }

                    if (rack->torswitch_type == TORSWITCH_TYPE_OPENFLOW) {
                        LCPD_LOG(LOG_ERR, "switchtype OPENFLOW is deprecated");
                        assert(0);
                        return _VL2_FALSE_;
                    } else if (rack->torswitch_type == TORSWITCH_TYPE_TUNNEL) {
                        snprintf(url, LG_API_URL_SIZE, SDN_API_PREFIX SDN_API_SWITCH_TUNNEL,
                                SDN_API_DEST_IP_SDNCTRL, SDN_CTRL_LISTEN_PORT, SDN_API_VERSION,
                                rack->torswitch[i].ip, peer_rack->torswitch[j].tunnel_ip);
                        LCPD_LOG(LOG_INFO, "Send url:%s\n", url);
                        result[0] = 0;
                        res = lcpd_call_api(url, API_METHOD_POST, NULL, result);
                        if (res) {  // curl failed or api failed
                            LCPD_LOG(
                                LOG_ERR,
                                "failed to build tunnel in switch mip=%s, remote_ip=%s\n",
                                rack->torswitch[i].ip, peer_rack->torswitch[j].tunnel_ip);
                            LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
                            snprintf(tmp_ip, MAX_HOST_ADDRESS_LEN, "%s", rack->torswitch[i].tunnel_ip);
                            snprintf(tmp_peer_ip, MAX_HOST_ADDRESS_LEN, "%s", peer_rack->torswitch[j].tunnel_ip);
                            goto ERROR;
                        }
                    }
                }
            } else {
                for (j=0; j<VL2_MAX_LCGS_NUM && peer_rack->gw_hosts[j][0]!='\0'; j++) {
                    if (rack->torswitch[i].tunnel_ip[0] == '\0') {
                        VL2_SYSLOG(LOG_DEBUG, "@INFO (APP): %s tunnel ip is empty: "
                                "torswitch ip=%s, tunnel ip=%s\n",
                                __FUNCTION__, rack->torswitch[i].ip, rack->torswitch[i].tunnel_ip);
                        continue;
                    }
                    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s='%s' AND %s='%s'",
                            TABLE_RACK_TUNNEL_COLUMN_LOCAL_UPLINK_IP, rack->torswitch[i].tunnel_ip,
                            TABLE_RACK_TUNNEL_COLUMN_REMOTE_UPLINK_IP, peer_rack->gw_hosts[j],
                            TABLE_RACK_TUNNEL_COLUMN_SWITCH_IP, rack->torswitch[i].ip);
                    err = ext_vl2_db_lookup_ids_v2(TABLE_RACK_TUNNEL,
                            TABLE_RACK_TUNNEL_COLUMN_ID, conditions, &id, 1);
                    if (err == 0 && id) {
                        VL2_SYSLOG(LOG_DEBUG, "@INFO (APP): %s tunnel %s->%s already exist on %s.\n",
                                __FUNCTION__, rack->torswitch[i].tunnel_ip, peer_rack->gw_hosts[j],
                                rack->torswitch[i].ip);
                        continue;
                    }

                    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'",
                            TABLE_HOST_DEVICE_COLUMN_UPLINK_IP, peer_rack->gw_hosts[j]);
                    err = ext_vl2_db_lookup(TABLE_HOST_DEVICE,
                            TABLE_HOST_DEVICE_COLUMN_IP, conditions, peer_gw_ctrl_ip);
                    err += ext_vl2_db_lookup(TABLE_HOST_DEVICE,
                            TABLE_HOST_DEVICE_COLUMN_PASSWD, conditions, peer_gw_passwd);
                    if (err) {
                        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s host-device with "
                                "%s has no IP/PASSWD.\n", __FUNCTION__, conditions);
                        return _VL2_FALSE_;
                    }
                    //insert db
                    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s,%s,%s",
                            TABLE_RACK_TUNNEL_COLUMN_LOCAL_UPLINK_IP,
                            TABLE_RACK_TUNNEL_COLUMN_REMOTE_UPLINK_IP,
                            TABLE_RACK_TUNNEL_COLUMN_SWITCH_IP);
                    snprintf(value, VL2_MAX_DB_BUF_LEN, "'%s','%s','%s'",
                            rack->torswitch[i].tunnel_ip, peer_rack->gw_hosts[j], rack->torswitch[i].ip);
                    err = ext_vl2_db_insert(TABLE_RACK_TUNNEL, conditions, value);
                    if (err) {
                        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s insert rack_tunnel %s->%s "
                                "failed.\n", __FUNCTION__, rack->torswitch[i].tunnel_ip, peer_rack->gw_hosts[j]);
                        return _VL2_FALSE_;
                    }

                    if (rack->torswitch_type == TORSWITCH_TYPE_OPENFLOW) {
                        LCPD_LOG(LOG_ERR, "switchtype OPENFLOW is deprecated");
                        assert(0);
                        return _VL2_FALSE_;
                    } else if (rack->torswitch_type == TORSWITCH_TYPE_TUNNEL) {
                        snprintf(url, LG_API_URL_SIZE, SDN_API_PREFIX SDN_API_SWITCH_TUNNEL,
                                SDN_API_DEST_IP_SDNCTRL, SDN_CTRL_LISTEN_PORT, SDN_API_VERSION,
                                rack->torswitch[i].ip, peer_rack->gw_hosts[j]);
                        LCPD_LOG(LOG_INFO, "Send url:%s\n", url);
                        result[0] = 0;
                        res = lcpd_call_api(url, API_METHOD_POST, NULL, result);
                        if (res) {  // curl failed or api failed
                            LCPD_LOG(
                                LOG_ERR,
                                "failed to build tunnel in switch mip=%s, remote_ip=%s\n",
                                rack->torswitch[i].ip, peer_rack->gw_hosts[j]);
                            LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
                            snprintf(tmp_ip, MAX_HOST_ADDRESS_LEN, "%s", rack->torswitch[i].tunnel_ip);
                            snprintf(tmp_peer_ip, MAX_HOST_ADDRESS_LEN, "%s", peer_rack->gw_hosts[j]);
                            goto ERROR;
                        }
                    }
                }
            }
        }

        return _VL2_TRUE_;
    }

    for (i=0; i<VL2_MAX_LCGS_NUM && rack->gw_hosts[i][0]!='\0'; i++) {
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'",
                TABLE_HOST_DEVICE_COLUMN_UPLINK_IP, rack->gw_hosts[i]);
        err = ext_vl2_db_lookup(TABLE_HOST_DEVICE,
                TABLE_HOST_DEVICE_COLUMN_IP, conditions, gw_ctrl_ip);
        if (err) {
            VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s host device with "
                    "%s has no IP.\n", __FUNCTION__, conditions);
            return _VL2_FALSE_;
        }

        if (peer_rack->torswitch_type != TORSWITCH_TYPE_ETHERNET) {
            for (j = 0; j < RACK_MAX_SWITCH_NUM && peer_rack->torswitch[j].id; j++) {
                if (peer_rack->torswitch[i].tunnel_ip[0] == '\0') {
                    VL2_SYSLOG(LOG_DEBUG, "@INFO (APP): %s peer tunnel ip is empty: "
                            "torswitch ip=%s, tunnel ip=%s\n",
                            __FUNCTION__, peer_rack->torswitch[i].ip, peer_rack->torswitch[i].tunnel_ip);
                    continue;
                }
                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s='%s'",
                        TABLE_RACK_TUNNEL_COLUMN_LOCAL_UPLINK_IP, rack->gw_hosts[i],
                        TABLE_RACK_TUNNEL_COLUMN_REMOTE_UPLINK_IP, peer_rack->torswitch[j].tunnel_ip);
                err = ext_vl2_db_lookup_ids_v2(TABLE_RACK_TUNNEL,
                        TABLE_RACK_TUNNEL_COLUMN_ID, conditions, &id, 1);
                if (err == 0 && id) {
                    VL2_SYSLOG(LOG_DEBUG, "@INFO (APP): %s tunnel %s->%s already exist.\n",
                                __FUNCTION__, rack->gw_hosts[i], peer_rack->torswitch[j].tunnel_ip);
                    continue;
                }

                snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, gw_ctrl_ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
                snprintf(url2, LG_API_URL_SIZE, LCPD_API_GRE_POST_LG);
                strcat(url, url2);
                err = pack_gre_lg_post(post, LG_API_URL_SIZE, tunnel_protocol, peer_rack->torswitch[j].tunnel_ip);
                if (err) {
                    return _VL2_FALSE_;
                }

                err = lcpd_call_api(url, API_METHOD_POST, post, NULL);
                if (err) {
                    VL2_SYSLOG(LOG_ERR, "call api failed: %s", url);
                    return _VL2_FALSE_;
                }

                //insert db
                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s,%s",
                        TABLE_RACK_TUNNEL_COLUMN_LOCAL_UPLINK_IP,
                        TABLE_RACK_TUNNEL_COLUMN_REMOTE_UPLINK_IP);
                snprintf(value, VL2_MAX_DB_BUF_LEN, "'%s','%s'",
                        rack->gw_hosts[i], peer_rack->torswitch[j].tunnel_ip);
                err = ext_vl2_db_insert(TABLE_RACK_TUNNEL, conditions, value);
                if (err) {
                    VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s insert rack_tunnel %s->%s "
                            "failed.\n", __FUNCTION__, rack->gw_hosts[i], peer_rack->torswitch[j].tunnel_ip);
                    return _VL2_FALSE_;
                }
            }
        } else {
            for (j=0; j<VL2_MAX_LCGS_NUM && peer_rack->gw_hosts[j][0]!='\0'; j++) {
                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s='%s'",
                        TABLE_RACK_TUNNEL_COLUMN_LOCAL_UPLINK_IP, rack->gw_hosts[i],
                        TABLE_RACK_TUNNEL_COLUMN_REMOTE_UPLINK_IP, peer_rack->gw_hosts[j]);
                err = ext_vl2_db_lookup_ids_v2(TABLE_RACK_TUNNEL,
                        TABLE_RACK_TUNNEL_COLUMN_ID, conditions, &id, 1);
                if (err == 0 && id) {
                    VL2_SYSLOG(LOG_DEBUG, "@INFO (APP): %s tunnel %s->%s already exist.\n",
                            __FUNCTION__, rack->gw_hosts[i], peer_rack->gw_hosts[j]);
                    continue;
                }

                snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, gw_ctrl_ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
                snprintf(url2, LG_API_URL_SIZE, LCPD_API_GRE_POST_LG);
                strcat(url, url2);
                err = pack_gre_lg_post(post, LG_API_URL_SIZE, tunnel_protocol, peer_rack->gw_hosts[j]);
                if (err) {
                    return _VL2_FALSE_;
                }

                err = lcpd_call_api(url, API_METHOD_POST, post, NULL);
                if (err) {
                    VL2_SYSLOG(LOG_ERR, "call api failed: %s", url);
                    return _VL2_FALSE_;
                }

                //insert
                VL2_SYSLOG(LOG_DEBUG, "@INFO (APP): %s will create tunnel %s->%s.\n",
                        __FUNCTION__, rack->gw_hosts[i], peer_rack->gw_hosts[j]);

                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s,%s",
                        TABLE_RACK_TUNNEL_COLUMN_LOCAL_UPLINK_IP,
                        TABLE_RACK_TUNNEL_COLUMN_REMOTE_UPLINK_IP);
                snprintf(value, VL2_MAX_DB_BUF_LEN, "'%s','%s'",
                        rack->gw_hosts[i], peer_rack->gw_hosts[j]);
                err = ext_vl2_db_insert(TABLE_RACK_TUNNEL, conditions, value);
                if (err) {
                    VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s insert rack_tunnel %s->%s "
                            "failed.\n", __FUNCTION__, rack->gw_hosts[i], peer_rack->gw_hosts[j]);
                    return _VL2_FALSE_;
                }
            }
        }

        /* tunnel qos */
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'",
                TABLE_UPLINK_QOS_COLUMN_UPLINK_IP, rack->gw_hosts[i]);
        err = ext_vl2_db_lookup_ids_v2(TABLE_UPLINK_QOS,
                TABLE_UPLINK_QOS_COLUMN_TUNNEL_MAX_RATE, conditions, (int *)&max_rate, 1);
        if (err == 0) {
            VL2_SYSLOG(LOG_DEBUG, "@INFO (APP): %s uplink qos of %s "
                    "already configured.\n", __FUNCTION__, rack->gw_hosts[i]);
        } else {
            min_rate = TUNNEL_QOS_MIN_RATE_DEF * ONE_TRILLION_BANDWIDTH;
            max_rate = TUNNEL_QOS_MAX_RATE_DEF * ONE_TRILLION_BANDWIDTH;
            VL2_SYSLOG(LOG_DEBUG, "@INFO (APP): %s will config uplink tunnel qos "
                    "%s: [%d,%d].\n", __FUNCTION__, rack->gw_hosts[i], min_rate, max_rate);

            snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, gw_ctrl_ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
            snprintf(url2, LG_API_URL_SIZE, LCPD_API_TUNNEL_QOS_POST_LG);
            strcat(url, url2);
            err = pack_tunnel_qos_lg_post(post, LG_API_URL_SIZE, min_rate, max_rate);
            if (err) {
                return _VL2_FALSE_;
            }
            err = lcpd_call_api(url, API_METHOD_PATCH, post, NULL);
            if (err) {
                VL2_SYSLOG(LOG_ERR, "call api failed: %s", url);
                return _VL2_FALSE_;
            }

            /* insert uplink_qos db */
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s,%s,%s",
                    TABLE_UPLINK_QOS_COLUMN_UPLINK_IP,
                    TABLE_UPLINK_QOS_COLUMN_TUNNEL_MIN_RATE,
                    TABLE_UPLINK_QOS_COLUMN_TUNNEL_MAX_RATE);
            snprintf(value, VL2_MAX_DB_BUF_LEN, "'%s',%d,%d",
                    rack->gw_hosts[i], min_rate, max_rate);
            err = ext_vl2_db_insert(TABLE_UPLINK_QOS, conditions, value);
            if (err) {
                VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s insert uplink_qos %s: [%d,%d] "
                        "failed.\n", __FUNCTION__, rack->gw_hosts[i], min_rate, max_rate);
                return _VL2_FALSE_;
            }
        }
    }

    return _VL2_TRUE_;
ERROR:
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s='%s'",
            TABLE_RACK_TUNNEL_COLUMN_LOCAL_UPLINK_IP, tmp_ip,
            TABLE_RACK_TUNNEL_COLUMN_REMOTE_UPLINK_IP, tmp_peer_ip);
    err = ext_vl2_db_delete(TABLE_RACK_TUNNEL, conditions);
    if (err) {
        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s delete rack_tunnel %s->%s failed.\n",
                __FUNCTION__, tmp_ip, tmp_peer_ip);
    }
    return _VL2_FALSE_;
}

static int config_vif_tunnel_policies(rack_network_t *rack,
                                      char *vl2_id,
                                      int vif_id,
                                      char *vif_type,
                                      int call_type)
{
    int err = _VL2_TRUE_;
    int vlantag = 0, vdevice_type = 0, vdevice_id = 0;
    char value[VL2_MAX_DB_BUF_LEN] = {0};
    char conditions[VL2_MAX_DB_BUF_LEN] = {0};
    char columns[VL2_MAX_DB_BUF_LEN] = {0};

    char vifmac[VL2_MAX_INTERF_DL_LEN] = {0};
    char gw_ctrl_ip[MAX_HOST_ADDRESS_LEN] = {0};
    char url[LG_API_URL_SIZE] = {0};
    char url2[LG_API_URL_SIZE] = {0};
    char post[LG_API_URL_SIZE] = {0};
    tunnel_policy_t tunnel_policy = {0};
    int i, reconfig_tunnel_policy;
    char domain[LCUUID_LEN] = {0};

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%s",
            TABLE_RACK_COLUMN_ID, rack->id);
    if (ext_vl2_db_lookup(TABLE_RACK, TABLE_RACK_COLUMN_DOMAIN, conditions, domain)) {
        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s cannot find domain for rack %s\n", __FUNCTION__, rack->id);
        return _VL2_FALSE_;
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d",
            TABLE_VINTERFACE_COLUMN_ID, vif_id);
    err = ext_vl2_db_lookup(TABLE_VINTERFACE,
            TABLE_VINTERFACE_COLUMN_MAC, conditions, vifmac);
    err += ext_vl2_db_lookup_ids_v2(TABLE_VINTERFACE,
            TABLE_VINTERFACE_COLUMN_DEVICETYPE, conditions, &vdevice_type, 1);
    err += ext_vl2_db_lookup_ids_v2(TABLE_VINTERFACE,
            TABLE_VINTERFACE_COLUMN_DEVICEID, conditions, &vdevice_id, 1);
    err += ext_vl2_db_lookup_ids_v2(TABLE_VINTERFACE,
            TABLE_VINTERFACE_COLUMN_VLANTAG, conditions, &vlantag, 1);
    if (err) {
        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s vinterface with "
                "id-%d has no mac/device_type/device_id/vlantag.\n",
                __FUNCTION__, vif_id);
        return _VL2_FALSE_;
    }

    if (rack->torswitch_type == TORSWITCH_TYPE_OPENFLOW) {
        LCPD_LOG(LOG_ERR, "switchtype OPENFLOW is deprecated");
        assert(0);
        return _VL2_FALSE_;
    } else if (rack->torswitch_type == TORSWITCH_TYPE_TUNNEL) {
        for (i = 0; i< RACK_MAX_SWITCH_NUM && rack->torswitch[i].id; i++) {
            if (call_type == ADD_TUNNEL_POLICY_CALL_TYPE) {
                /* update tunnel_policy db */
                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s=%d",
                        TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP, rack->torswitch[i].ip,
                        TABLE_TUNNEL_POLICY_COLUMN_VIFID, vif_id);

                err = ext_vl2_db_lookup(TABLE_TUNNEL_POLICY,
                        TABLE_TUNNEL_POLICY_COLUMN_VIFID, conditions, value);
                if (err) { /* not found */
                    snprintf(columns, VL2_MAX_DB_BUF_LEN, "%s,%s,%s,%s,%s,%s,%s",
                            TABLE_TUNNEL_POLICY_COLUMN_VIFID,
                            TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP,
                            TABLE_TUNNEL_POLICY_COLUMN_VDEVICETYPE,
                            TABLE_TUNNEL_POLICY_COLUMN_VDEVICEID,
                            TABLE_TUNNEL_POLICY_COLUMN_VL2ID,
                            TABLE_TUNNEL_POLICY_COLUMN_VLANTAG,
                            TABLE_TUNNEL_POLICY_COLUMN_VIFMAC);
                    snprintf(value, VL2_MAX_DB_BUF_LEN, "%d,'%s',%d,%d,%s,%d,'%s'",
                            vif_id, rack->torswitch[i].ip, vdevice_type, vdevice_id,
                            vl2_id, vlantag, vifmac);
                    err = ext_vl2_db_insert(TABLE_TUNNEL_POLICY, columns, value);
                } else {
                    snprintf(columns, VL2_MAX_DB_BUF_LEN, "%s='%s',%s=%d,%s=%d,%s=%s,%s=%d,%s='%s'",
                            TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP, rack->torswitch[i].ip,
                            TABLE_TUNNEL_POLICY_COLUMN_VDEVICETYPE, vdevice_type,
                            TABLE_TUNNEL_POLICY_COLUMN_VDEVICEID, vdevice_id,
                            TABLE_TUNNEL_POLICY_COLUMN_VL2ID, vl2_id,
                            TABLE_TUNNEL_POLICY_COLUMN_VLANTAG, vlantag,
                            TABLE_TUNNEL_POLICY_COLUMN_VIFMAC, vifmac);
                    err = ext_vl2_db_updates(TABLE_TUNNEL_POLICY,
                            columns, conditions);
                }
                if (err) {
                    VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s update tunnel policy db failed."
                            "columns=[%s], value=[%s], condition=[%s].\n",
                            __FUNCTION__, columns, value, conditions);
                    return _VL2_FALSE_;
                }
            } else {
                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s=%d",
                        TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP, rack->torswitch[i].ip,
                        TABLE_TUNNEL_POLICY_COLUMN_VIFID, vif_id);
                err = ext_vl2_db_delete(TABLE_TUNNEL_POLICY, conditions);
                if (err) {
                    VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s delete tunnel policy db failed."
                            "condition=[%s].\n", __FUNCTION__, conditions);
                    return _VL2_FALSE_;
                }
            }
        }
        return _VL2_TRUE_;
    }

    for (i = 0; i < VL2_MAX_LCGS_NUM && rack->gw_hosts[i][0] != '\0'; i++) {
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'",
                TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP, rack->gw_hosts[i]);
        err += ext_vl2_db_lookup(TABLE_COMPUTE_RESOURCE,
                TABLE_COMPUTE_RESOURCE_COLUMN_IP, conditions, gw_ctrl_ip);
        if (err) {
            VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s compute-resource with "
                    "%s has no IP.\n", __FUNCTION__, conditions);
            return _VL2_FALSE_;
        }

        if (call_type == ADD_TUNNEL_POLICY_CALL_TYPE) {
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s=%d",
                    TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP, rack->gw_hosts[i],
                    TABLE_TUNNEL_POLICY_COLUMN_VIFID, vif_id);
            err = ext_vl2_db_tunnel_policy_load(&tunnel_policy, conditions);
            reconfig_tunnel_policy = 0;
            if (!err && tunnel_policy.vlantag == vlantag) {
                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d",
                        TABLE_VINTERFACE_COLUMN_ID, vif_id);
                err = ext_vl2_db_lookup(TABLE_VINTERFACE,
                        TABLE_VINTERFACE_COLUMN_IFTYPE, conditions, value);
                if (err || atoi(value) != IFTYPE_VGATEWAY_LAN) {
                    return _VL2_TRUE_;
                }
                err = ext_vl2_db_lookup(TABLE_VINTERFACE,
                        TABLE_VINTERFACE_COLUMN_DEVICETYPE, conditions, value);
                if (err || atoi(value) != DEVICETYPE_VGATEWAY) {
                    return _VL2_TRUE_;
                }
                err = ext_vl2_db_lookup(TABLE_VINTERFACE,
                        TABLE_VINTERFACE_COLUMN_DEVICEID, conditions, value);
                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%s",
                        TABLE_VNET_COLUMN_ID, value);
                err = ext_vl2_db_lookup(TABLE_VNET,
                        TABLE_VNET_COLUMN_ROLE, conditions, value);
                if (err || atoi(value) != ROLE_VALVE) {
                    return _VL2_TRUE_;
                }
                reconfig_tunnel_policy = 1;
            }

            snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, gw_ctrl_ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
            snprintf(url2, LG_API_URL_SIZE, LCPD_API_VIF_VL2_POST_LG);
            strcat(url, url2);
            err = pack_vif_lg_post(post, LG_API_URL_SIZE, atoi(vl2_id), vlantag, vif_id, vifmac);
            err = lcpd_call_api(url, API_METHOD_POST, post, NULL);
            if (err) {
                VL2_SYSLOG(LOG_ERR, "request "
                        "LG_API_SET_VIF_TUNNEL_POLICY %s failed, vifid=%d.\n",
                        url, vif_id);
                return _VL2_FALSE_;
            }

            if (reconfig_tunnel_policy) {
                return _VL2_TRUE_;
            }

            /* update tunnel_policy db */
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s=%d",
                    TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP, rack->gw_hosts[i],
                    TABLE_TUNNEL_POLICY_COLUMN_VIFID, vif_id);

            err = ext_vl2_db_lookup(TABLE_TUNNEL_POLICY,
                    TABLE_TUNNEL_POLICY_COLUMN_VIFID, conditions, value);
            if (err) { /* not found */
                snprintf(columns, VL2_MAX_DB_BUF_LEN, "%s,%s,%s,%s,%s,%s,%s",
                        TABLE_TUNNEL_POLICY_COLUMN_VIFID,
                        TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP,
                        TABLE_TUNNEL_POLICY_COLUMN_VDEVICETYPE,
                        TABLE_TUNNEL_POLICY_COLUMN_VDEVICEID,
                        TABLE_TUNNEL_POLICY_COLUMN_VL2ID,
                        TABLE_TUNNEL_POLICY_COLUMN_VLANTAG,
                        TABLE_TUNNEL_POLICY_COLUMN_VIFMAC);
                snprintf(value, VL2_MAX_DB_BUF_LEN, "%d,'%s',%d,%d,%s,%d,'%s'",
                        vif_id, rack->gw_hosts[i], vdevice_type, vdevice_id,
                        vl2_id, vlantag, vifmac);
                err = ext_vl2_db_insert(TABLE_TUNNEL_POLICY, columns, value);
            } else {
                snprintf(columns, VL2_MAX_DB_BUF_LEN, "%s='%s',%s=%d,%s=%d,%s=%s,%s=%d,%s='%s'",
                        TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP, rack->gw_hosts[i],
                        TABLE_TUNNEL_POLICY_COLUMN_VDEVICETYPE, vdevice_type,
                        TABLE_TUNNEL_POLICY_COLUMN_VDEVICEID, vdevice_id,
                        TABLE_TUNNEL_POLICY_COLUMN_VL2ID, vl2_id,
                        TABLE_TUNNEL_POLICY_COLUMN_VLANTAG, vlantag,
                        TABLE_TUNNEL_POLICY_COLUMN_VIFMAC, vifmac);
                err = ext_vl2_db_updates(TABLE_TUNNEL_POLICY,
                        columns, conditions);
            }
            if (err) {
                VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s update tunnel policy db failed."
                        "columns=[%s], value=[%s], condition=[%s].\n",
                        __FUNCTION__, columns, value, conditions);
                return _VL2_FALSE_;
            }

        } else if (call_type == DEL_TUNNEL_POLICY_CALL_TYPE) {
            snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, gw_ctrl_ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
            snprintf(url2, LG_API_URL_SIZE, LCPD_API_VIF_DEL_LG, atoi(vl2_id), vif_id);
            strcat(url, url2);
            err = lcpd_call_api(url, API_METHOD_DEL, NULL, NULL);

            /* delete from tunnel_policy db */
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s=%d",
                    TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP, rack->gw_hosts[i],
                    TABLE_TUNNEL_POLICY_COLUMN_VIFID, vif_id);
            err += ext_vl2_db_delete(TABLE_TUNNEL_POLICY, conditions);
            if (err) {
                VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s delete tunnel policy db failed."
                        "condition=[%s].\n", __FUNCTION__, conditions);
                return _VL2_FALSE_;
            }
        }
    }

    return _VL2_TRUE_;
}

static int config_vl2_tunnel_policies(rack_network_t *rack, char *vl2_id, int call_type)
{
    int err = _VL2_TRUE_;
    int vlantag = 0;
    char conditions[VL2_MAX_DB_BUF_LEN] = {0};
    char value[VL2_MAX_DB_BUF_LEN] = {0};
    char columns[VL2_MAX_DB_BUF_LEN] = {0};

    char gw_ctrl_ip[MAX_HOST_ADDRESS_LEN] = {0};
    char url[LG_API_URL_SIZE] = {0};
    char url2[LG_API_URL_SIZE] = {0};
    char post[LG_API_BIG_RESULT_SIZE] = {0};
    char result[LG_API_BIG_RESULT_SIZE] = {0};
    vl2_tunnel_policy_t vl2_tunnel_policy = {0};
    int i, res = 0;

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%s AND (%s=%s OR %s=0)",
            TABLE_VL2_VLAN_COLUMN_VL2ID, vl2_id,
            TABLE_VL2_VLAN_COLUMN_RACKID, rack->id,
            TABLE_VL2_VLAN_COLUMN_RACKID);
    err = ext_vl2_db_lookup_ids_v2(TABLE_VL2_VLAN,
            TABLE_VL2_VLAN_COLUMN_VLANTAG, conditions, &vlantag, 1);
    if (err) {
        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s vl2_vlan with "
                "%s has no vlantag.\n", __FUNCTION__, conditions);
        return _VL2_FALSE_;
    }

    if (rack->torswitch_type != TORSWITCH_TYPE_ETHERNET) {
        for (i = 0; i< RACK_MAX_SWITCH_NUM && rack->torswitch[i].id; i++) {
            if (rack->torswitch[i].tunnel_ip[0] == '\0') {
                VL2_SYSLOG(LOG_DEBUG, "@INFO (APP): %s tunnel ip is empty: "
                        "torswitch ip=%s, tunnel ip=%s\n",
                        __FUNCTION__, rack->torswitch[i].ip, rack->torswitch[i].tunnel_ip);
                continue;
            }
            if (call_type == ADD_TUNNEL_POLICY_CALL_TYPE) {
                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s=%s",
                        TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP, rack->torswitch[i].ip,
                        TABLE_VL2_TUNNEL_POLICY_COLUMN_VL2ID, vl2_id);
                err = ext_vl2_db_vl2_tunnel_policy_load(&vl2_tunnel_policy, conditions);
                if (!err && vl2_tunnel_policy.vlantag == vlantag) {
                    return _VL2_TRUE_;
                }

                if (rack->torswitch_type == TORSWITCH_TYPE_OPENFLOW) {
                    LCPD_LOG(LOG_ERR, "switchtype OPENFLOW is deprecated");
                    assert(0);
                    return _VL2_FALSE_;
                } else if (rack->torswitch_type == TORSWITCH_TYPE_TUNNEL) {
                    snprintf(url, LG_API_URL_SIZE, SDN_API_PREFIX SDN_API_SWITCH_SUBNET,
                            SDN_API_DEST_IP_SDNCTRL, SDN_CTRL_LISTEN_PORT, SDN_API_VERSION,
                            rack->torswitch[i].ip, atoi(vl2_id));
                    LCPD_LOG(LOG_INFO, "Send url:%s\n", url);
                    result[0] = 0;
                    res = lcpd_call_api(url, API_METHOD_POST, NULL, result);
                    if (res) {  // curl failed or api failed
                        LCPD_LOG(
                            LOG_ERR,
                            "failed to add subnet in switch mip=%s, vl2id=%d, vlantag=%d\n",
                            rack->torswitch[i].ip, atoi(vl2_id), vlantag);
                        LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
                        return _VL2_FALSE_;
                    }
                }
                /* update tunnel_policy db */
                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s=%s",
                        TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP, rack->torswitch[i].ip,
                        TABLE_VL2_TUNNEL_POLICY_COLUMN_VL2ID, vl2_id);
                err = ext_vl2_db_lookup(TABLE_VL2_TUNNEL_POLICY,
                        TABLE_VL2_TUNNEL_POLICY_COLUMN_VL2ID, conditions, value);
                if (err) { /* not found */
                    snprintf(columns, VL2_MAX_DB_BUF_LEN, "%s,%s,%s",
                            TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP,
                            TABLE_TUNNEL_POLICY_COLUMN_VL2ID,
                            TABLE_TUNNEL_POLICY_COLUMN_VLANTAG);
                    snprintf(value, VL2_MAX_DB_BUF_LEN, "'%s',%s,%d",
                            rack->torswitch[i].ip, vl2_id, vlantag);
                    err = ext_vl2_db_insert(TABLE_VL2_TUNNEL_POLICY, columns, value);
                } else {
                    snprintf(columns, VL2_MAX_DB_BUF_LEN, "%s='%s',%s=%s,%s=%d",
                            TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP, rack->torswitch[i].ip,
                            TABLE_TUNNEL_POLICY_COLUMN_VL2ID, vl2_id,
                            TABLE_TUNNEL_POLICY_COLUMN_VLANTAG, vlantag);
                    err = ext_vl2_db_updates(TABLE_VL2_TUNNEL_POLICY,
                            columns, conditions);
                }
                if (err) {
                    VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s update tunnel policy db failed."
                            "columns=[%s], value=[%s], condition=[%s].\n",
                            __FUNCTION__, columns, value, conditions);
                    return _VL2_FALSE_;
                }
            } else {
                if (rack->torswitch_type == TORSWITCH_TYPE_OPENFLOW) {
                    LCPD_LOG(LOG_ERR, "switchtype OPENFLOW is deprecated");
                    assert(0);
                    return _VL2_FALSE_;
                } else if (rack->torswitch_type == TORSWITCH_TYPE_TUNNEL) {
                    snprintf(url, LG_API_URL_SIZE, SDN_API_PREFIX SDN_API_SWITCH_SUBNET,
                            SDN_API_DEST_IP_SDNCTRL, SDN_CTRL_LISTEN_PORT, SDN_API_VERSION,
                            rack->torswitch[i].ip, atoi(vl2_id));
                    LCPD_LOG(LOG_INFO, "Send url:%s\n", url);
                    result[0] = 0;
                    res = lcpd_call_api(url, API_METHOD_DEL, NULL, result);
                    if (res) {  // curl failed or api failed
                        LCPD_LOG(
                            LOG_ERR,
                            "failed to del subnet in switch mip=%s, vl2id=%d, vlantag=%d\n",
                            rack->torswitch[i].ip, atoi(vl2_id), vlantag);
                        LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
                        return _VL2_FALSE_;
                    }
                }

                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s=%s",
                        TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP, rack->torswitch[i].ip,
                        TABLE_VL2_TUNNEL_POLICY_COLUMN_VL2ID, vl2_id);
                err = ext_vl2_db_delete(TABLE_VL2_TUNNEL_POLICY, conditions);
                if (err) {
                    VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s delete vl2 tunnel policy db failed."
                            "condition=[%s].\n", __FUNCTION__, conditions);
                    return _VL2_FALSE_;
                }
            }
        }

        return _VL2_TRUE_;
    }

    for (i = 0; i < VL2_MAX_LCGS_NUM && rack->gw_hosts[i][0] != '\0'; i++) {
        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s'",
                TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP, rack->gw_hosts[i]);
        err = ext_vl2_db_lookup(TABLE_COMPUTE_RESOURCE,
                TABLE_COMPUTE_RESOURCE_COLUMN_IP, conditions, gw_ctrl_ip);
        if (err) {
            VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s compute-resource with "
                    "%s has no IP.\n", __FUNCTION__, conditions);
            return _VL2_FALSE_;
        }

        if (call_type == ADD_TUNNEL_POLICY_CALL_TYPE) {
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s=%s",
                    TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP, rack->gw_hosts[i],
                    TABLE_VL2_TUNNEL_POLICY_COLUMN_VL2ID, vl2_id);
            err = ext_vl2_db_vl2_tunnel_policy_load(&vl2_tunnel_policy, conditions);
            if (!err && vl2_tunnel_policy.vlantag == vlantag) {
                return _VL2_TRUE_;
            }

            snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, gw_ctrl_ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
            snprintf(url2, LG_API_URL_SIZE, LCPD_API_VIF_VL2_POST_LG);
            strcat(url, url2);
            err = pack_vl2_lg_post(post, LG_API_URL_SIZE, atoi(vl2_id), vlantag);
            if (err) {
                return _VL2_FALSE_;
            }
            err = lcpd_call_api(url, API_METHOD_POST, post, NULL);
            if (err) {
                VL2_SYSLOG(LOG_ERR, "request "
                        "LG_API_SET_VL2_TUNNEL_POLICY %s failed, vl2_id=%s.\n",
                        url, vl2_id);
                return _VL2_FALSE_;
            }

            /* update tunnel_policy db */
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s=%s",
                    TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP, rack->gw_hosts[i],
                    TABLE_VL2_TUNNEL_POLICY_COLUMN_VL2ID, vl2_id);
            err = ext_vl2_db_lookup(TABLE_VL2_TUNNEL_POLICY,
                    TABLE_VL2_TUNNEL_POLICY_COLUMN_VL2ID, conditions, value);
            if (err) { /* not found */
                snprintf(columns, VL2_MAX_DB_BUF_LEN, "%s,%s,%s",
                        TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP,
                        TABLE_TUNNEL_POLICY_COLUMN_VL2ID,
                        TABLE_TUNNEL_POLICY_COLUMN_VLANTAG);
                snprintf(value, VL2_MAX_DB_BUF_LEN, "'%s',%s,%d",
                        rack->gw_hosts[i], vl2_id, vlantag);
                err = ext_vl2_db_insert(TABLE_VL2_TUNNEL_POLICY, columns, value);
            } else {
                snprintf(columns, VL2_MAX_DB_BUF_LEN, "%s='%s',%s=%s,%s=%d",
                        TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP, rack->gw_hosts[i],
                        TABLE_TUNNEL_POLICY_COLUMN_VL2ID, vl2_id,
                        TABLE_TUNNEL_POLICY_COLUMN_VLANTAG, vlantag);
                err = ext_vl2_db_updates(TABLE_VL2_TUNNEL_POLICY,
                        columns, conditions);
            }
            if (err) {
                VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s update tunnel policy db failed."
                        "columns=[%s], value=[%s], condition=[%s].\n",
                        __FUNCTION__, columns, value, conditions);
                return _VL2_FALSE_;
            }

        } else if (call_type == DEL_TUNNEL_POLICY_CALL_TYPE) {
            snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX, gw_ctrl_ip, LCPD_API_DEST_PORT, LCPD_API_VERSION);
            snprintf(url2, LG_API_URL_SIZE, LCPD_API_VL2_DEL_LG, atoi(vl2_id));
            strcat(url, url2);
            err = lcpd_call_api(url, API_METHOD_DEL, NULL, NULL);

            /* delete from tunnel_policy db */
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s='%s' AND %s=%s",
                    TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP, rack->gw_hosts[i],
                    TABLE_VL2_TUNNEL_POLICY_COLUMN_VL2ID, vl2_id);
            err += ext_vl2_db_delete(TABLE_VL2_TUNNEL_POLICY, conditions);
            if (err) {
                VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s delete vl2 tunnel policy db failed."
                        "condition=[%s].\n", __FUNCTION__, conditions);
                return _VL2_FALSE_;
            }
        }
    }

    return err ? _VL2_FALSE_ : _VL2_TRUE_;
}

static int check_tunnel_policies(char *vifid, char *vl2_id, int call_type)
{
    char conditions[VL2_MAX_DB_BUF_LEN] = {0};

    int  ids[VL2_MAX_DB_BUF_LEN] = {0};
    int  rack_ids[VL2_MAX_RACKS_NUM][VL2_MAX_DB_BUF_LEN] = {{0}};
    int  rack_idnum[VL2_MAX_RACKS_NUM] = {0};
    rack_network_t rack[VL2_MAX_RACKS_NUM];
    char *pgw_hosts[VL2_MAX_RACKS_NUM][VL2_MAX_LCGS_NUM];
    int *ptorswitch_ids[RACK_MAX_SWITCH_NUM] = {0};
    bool rack_exist = false;
    int  n_rack = 1;
    int  i, j, err;

    memset(rack, 0, sizeof(rack));
    for (i = 0; i < VL2_MAX_RACKS_NUM; i++) {
        for (j = 0; j < VL2_MAX_LCGS_NUM; j++) {
            pgw_hosts[i][j] = rack[i].gw_hosts[j];
        }
    }

    /* vifid -> vm -> host -> rack */
    err = get_rack_of_vif(atoi(vifid), rack[0].id);
    if (err != _VL2_TRUE_) {
        return _VL2_FALSE_;
    }
    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%s", TABLE_RACK_COLUMN_ID, rack[0].id);
    err = ext_vl2_db_lookup_ids(TABLE_RACK, TABLE_RACK_COLUMN_TORSWITCH_TYPE,
            conditions, &rack[0].torswitch_type);
    if (err) {
        VL2_SYSLOG(LOG_ERR, "%s fail to get switch type of rack%s",__FUNCTION__,rack[0].id);
        return _VL2_FALSE_;
    }

    if (rack[0].torswitch_type != TORSWITCH_TYPE_TUNNEL) {
        err = get_gateway_of_rack(rack[0].id, pgw_hosts[0]);
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%s and %s!=%s and %s=%d",
            TABLE_VINTERFACE_COLUMN_VL2ID, vl2_id, TABLE_VINTERFACE_COLUMN_ID, vifid,
            TABLE_VINTERFACE_COLUMN_STATE, VIF_STATE_ATTACHED);
    err = ext_vl2_db_lookup_ids_v2(TABLE_VINTERFACE, TABLE_VINTERFACE_COLUMN_ID, conditions, ids,
            sizeof(ids) / sizeof(ids[0]));

    for (i = 0; i < VL2_MAX_DB_BUF_LEN && ids[i]; ++i) {
        err = get_rack_of_vif(ids[i], rack[n_rack].id);
        if (err != _VL2_TRUE_) {
            return _VL2_FALSE_;
        }

        for (j = 0; j < n_rack; j++) {
            if (strcmp(rack[j].id, rack[n_rack].id) == 0) {
                rack_exist = true;
                break;
            }
        }

        rack_ids[j][rack_idnum[j]] = ids[i];
        rack_idnum[j]++;
        if (rack_exist) {
            rack_exist = false;
            continue;
        }

        snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%s", TABLE_RACK_COLUMN_ID, rack[n_rack].id);
        err = ext_vl2_db_lookup_ids(TABLE_RACK, TABLE_RACK_COLUMN_TORSWITCH_TYPE, conditions, &rack[n_rack].torswitch_type);
        if (err) {
            VL2_SYSLOG(LOG_ERR, "%s failed get switch type of rack%s", __FUNCTION__,rack[n_rack].id);
            return _VL2_FALSE_;
        }
        if (rack[n_rack].torswitch_type != TORSWITCH_TYPE_TUNNEL) {
            err = get_gateway_of_rack(rack[n_rack].id, pgw_hosts[n_rack]);
        }

        if (++n_rack > VL2_MAX_RACKS_NUM) {
            break;
        }
    }

    for (i = 0; i < n_rack; i++) {
        if (rack[i].torswitch_type != TORSWITCH_TYPE_ETHERNET) {
            for (j = 0; j < RACK_MAX_SWITCH_NUM; j++) {
                ptorswitch_ids[j] = &rack[i].torswitch[j].id;
            }
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%s and %s is not NULL and %s != ''",
                    TABLE_NETWORK_DEVICE_COLUMN_RACKID, rack[i].id,
                    TABLE_NETWORK_DEVICE_COLUMN_TUNNEL_IP, TABLE_NETWORK_DEVICE_COLUMN_TUNNEL_IP);
            err = ext_vl2_db_lookup_ids_v3(TABLE_NETWORK_DEVICE, TABLE_NETWORK_DEVICE_COLUMN_ID,
                    conditions, ptorswitch_ids, RACK_MAX_SWITCH_NUM);
            if (err) {
                VL2_SYSLOG(LOG_ERR, "%s failed get torswitch of rack%s", __FUNCTION__,rack[i].id);
                return _VL2_FALSE_;
            }

            for (j = 0; j < RACK_MAX_SWITCH_NUM && rack[i].torswitch[j].id; j++) {
                snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d",
                        TABLE_NETWORK_DEVICE_COLUMN_ID, rack[i].torswitch[j].id);
                err = ext_vl2_db_lookup(TABLE_NETWORK_DEVICE,
                        TABLE_NETWORK_DEVICE_COLUMN_MIP, conditions, rack[i].torswitch[j].ip);
                err += ext_vl2_db_lookup(TABLE_NETWORK_DEVICE,
                        TABLE_NETWORK_DEVICE_COLUMN_TUNNEL_IP, conditions, rack[i].torswitch[j].tunnel_ip);
                err += ext_vl2_db_lookup(TABLE_NETWORK_DEVICE,
                        TABLE_NETWORK_DEVICE_COLUMN_USERNAME, conditions, rack[i].torswitch[j].username);
                err += ext_vl2_db_lookup(TABLE_NETWORK_DEVICE,
                        TABLE_NETWORK_DEVICE_COLUMN_PASSWD, conditions, rack[i].torswitch[j].passwd);
                err += ext_vl2_db_lookup(TABLE_NETWORK_DEVICE,
                        TABLE_NETWORK_DEVICE_COLUMN_ENABLE, conditions, rack[i].torswitch[j].enable);
                if (rack[i].torswitch_type == TORSWITCH_TYPE_OPENFLOW) {
                    LCPD_LOG(LOG_ERR, "switchtype OPENFLOW is deprecated");
                    assert(0);
                } else if (rack[i].torswitch_type == TORSWITCH_TYPE_TUNNEL) {
                    err += ext_vl2_db_lookup(TABLE_NETWORK_DEVICE,
                            TABLE_NETWORK_DEVICE_COLUMN_BRAND, conditions, rack[i].torswitch[j].brand);
                }
                if (err) {
                    VL2_SYSLOG(LOG_ERR, "%s failed get switch manageip/publicport/tunnelip/tunnelnetmask/tunnelgateway/username/password/brand of rack%s",
                            __FUNCTION__,rack[i].id);
                    return _VL2_FALSE_;
                }
            }
        }
    }

    if (n_rack == 1) {
        return _VL2_TRUE_;
    }

    if (call_type == ADD_TUNNEL_POLICY_CALL_TYPE) {
        for (i = 0; i < (n_rack-1); i++) {
            for (j = n_rack-1; j > i; j--) {
                err = config_tunnel(&rack[i], &rack[j]);
                err += config_tunnel(&rack[j], &rack[i]);
                if (err != _VL2_TRUE_) {
                    return _VL2_FALSE_;
                }
            }
        }
    }

    err = config_vif_tunnel_policies(&rack[0], vl2_id, atoi(vifid), "DATA", call_type);
    if (ADD_TUNNEL_POLICY_CALL_TYPE == call_type) {
        err += config_vl2_tunnel_policies(
                &rack[0], vl2_id, call_type);
    } else {
        if (!rack_idnum[0]) {
            err += config_vl2_tunnel_policies(
                    &rack[0], vl2_id, call_type);
        }
    }
    if (err != _VL2_TRUE_) {
        return _VL2_FALSE_;
    }

    if (2 == n_rack && !rack_idnum[0]) {
        /*noly one add 2 rack, must release policys on the other rack*/
    } else {
        call_type = ADD_TUNNEL_POLICY_CALL_TYPE;
    }

    for (i = 0; i < n_rack; i++) {
        if (!rack_idnum[i]) {
            continue;
        }

        /* config/release tun-flow policies for other vdevices in vl2 */
        for (j = 0; j < VL2_MAX_DB_BUF_LEN && rack_ids[i][j]; ++j) {
            err = config_vif_tunnel_policies(&rack[i], vl2_id, rack_ids[i][j], "DATA", call_type);
            if (err != _VL2_TRUE_) {
                return _VL2_FALSE_;
            }
        }

        err = config_vl2_tunnel_policies(&rack[i], vl2_id, call_type);
        if (err != _VL2_TRUE_) {
            return _VL2_FALSE_;
        }
    }

    return _VL2_TRUE_;
}

int add_vl2_tunnel_policies(char *interface_id)
{
    char conditions[VL2_MAX_DB_BUF_LEN] = {0};
    char vl2_id[VL2_MAX_POTS_ID_LEN] = {0};
    char old_vl2_id[VL2_MAX_POTS_ID_LEN] = {0};

    VL2_SYSLOG(LOG_DEBUG, "@INFO (APP): %s <interface=%s>\n",
            __FUNCTION__, interface_id);

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%s", TABLE_VINTERFACE_COLUMN_ID, interface_id);
    if (ext_vl2_db_lookup(TABLE_VINTERFACE,
                TABLE_VINTERFACE_COLUMN_VL2ID, conditions, vl2_id)) {
        VL2_SYSLOG(LOG_ERR, "@ERROR (APP): %s cannot find subnetid of %s\n", __FUNCTION__, interface_id);
        return _VL2_FALSE_;
    }

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%s",
            TABLE_TUNNEL_POLICY_COLUMN_VIFID, interface_id);
    ext_vl2_db_lookup(TABLE_TUNNEL_POLICY,
                TABLE_TUNNEL_POLICY_COLUMN_VL2ID, conditions, old_vl2_id);

    if (old_vl2_id[0] && atoi(vl2_id) != atoi(old_vl2_id)) {
        check_tunnel_policies(interface_id, old_vl2_id, DEL_TUNNEL_POLICY_CALL_TYPE);
    }

    return check_tunnel_policies(interface_id, vl2_id, ADD_TUNNEL_POLICY_CALL_TYPE);
}

int del_vl2_tunnel_policies(char *interface_id)
{
    VL2_SYSLOG(LOG_DEBUG, "@INFO (APP): %s <interface=%s>\n",
            __FUNCTION__, interface_id);
    char conditions[VL2_MAX_DB_BUF_LEN] = {0};
    char vl2_id[VL2_MAX_POTS_ID_LEN] = {0};

    snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%s",
            TABLE_TUNNEL_POLICY_COLUMN_VIFID, interface_id);
    if(ext_vl2_db_lookup(TABLE_TUNNEL_POLICY,
                TABLE_TUNNEL_POLICY_COLUMN_VL2ID, conditions, vl2_id)) {
        return _VL2_TRUE_;
    }
    VL2_SYSLOG(LOG_DEBUG, "%s,vifid=%s, vl2id=%s",__FUNCTION__,interface_id,vl2_id);

    return check_tunnel_policies(interface_id, vl2_id, DEL_TUNNEL_POLICY_CALL_TYPE);
}
