#include "lc_db.h"
#include "lc_db_net.h"
#include "lc_db_log.h"
#include "lc_global.h"
#include "lc_utils.h"
#include "lc_kernel_mon.h"
#include "lc_kernel.h"
#include "lc_livegate_api.h"
#include "vl2/ext_vl2_handler.h"

#define SWITCH_MONITOR_INTERVAL  60
#define MONITOR_HOUR             3600
#define SWITCH_MAX_NUM           128

static int pack_master_ctrl_ip_post(char *buf, int buf_len,
                                    char *module,
                                    char *domain,
                                    char *ip)
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    //int len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_STRING, LCPD_API_MASTER_CTRL_IPS_MODULE, root);
    tmp->text_value = module;

    tmp = create_json(NX_JSON_STRING, LCPD_API_MASTER_CTRL_IPS_DOMAIN, root);
    tmp->text_value = domain;

    tmp = create_json(NX_JSON_STRING, LCPD_API_MASTER_CTRL_IPS_IP, root);
    tmp->text_value = ip;

    //len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

void run_network_device_monitor()
{
    torswitch_t torswitchs[SWITCH_MAX_NUM];
    host_t hosts[MAX_HOST_NUM];
    char conditions[VL2_MAX_DB_BUF_LEN] = {0};
    char cmd[MAX_CMD_BUF_SIZE] = {0};
    struct msg_torswitch_opt *ts = (struct msg_torswitch_opt *)(cmd + sizeof(struct lc_msg_head));
    char result[LG_API_BIG_RESULT_SIZE] = {0};
    char url[LG_API_URL_SIZE] = {0};
    char post[LG_API_URL_SIZE] = {0};
    uint64_t boot_time;
    uint32_t timestamp = 0;
    int n_switch, n_host, torswitch_type;
    int i, ret = 0;
    int res = 0;

    while (1) {
        memset(torswitchs, 0, sizeof(torswitchs));
        /* get all switchs */
        ext_vl2_db_network_device_load_all(torswitchs, sizeof(torswitch_t),
                SWITCH_MAX_NUM, &n_switch, "rackid != 0");

        for (i = 0; i < n_switch; ++i) {
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "%s=%d",
                    TABLE_RACK_COLUMN_ID, torswitchs[i].rackid);
            if (ext_vl2_db_lookup_ids_v2(TABLE_RACK, TABLE_RACK_COLUMN_TORSWITCH_TYPE,
                        conditions, &torswitch_type, 1)) {
                VL2_SYSLOG(LOG_ERR,
                        "failed get switch type of rack%d", torswitchs[i].rackid);
                continue;
            }

            if (torswitch_type != TORSWITCH_TYPE_TUNNEL) {
                continue;
            }

            snprintf(url, LG_API_URL_SIZE, SDN_API_PREFIX SDN_API_SWITCH_BOOT_TIME,
                    SDN_API_DEST_IP_SDNCTRL, SDN_CTRL_LISTEN_PORT, SDN_API_VERSION,
                    torswitchs[i].ip);

            LCPD_LOG(LOG_INFO, "Send url:%s\n", url);
            result[0] = 0;
            res = lcpd_call_api(url, API_METHOD_GET, NULL, result);
            if (res) {  // curl failed or api failed
                LCPD_LOG(LOG_ERR, "failed to get boot time of switch mip = %s\n", torswitchs[i].ip);
                LCPD_LOG(LOG_ERR, "Recv result:%s\n", result);
                continue;
            } else {
                parse_json_msg_get_boot_time(&boot_time, result);
            }

            if (boot_time <= torswitchs[i].boot_time) {
                continue;
            }

            memset(cmd, 0, sizeof(cmd));
            snprintf(ts->ip, VL2_MAX_INTERF_NW_LEN, "%s", torswitchs[i].ip);
            ret = lc_torswitch_boot((struct lc_msg_head *)cmd);
        }

        if (!timestamp) {
            memset(hosts, 0, sizeof(hosts));
            snprintf(conditions, VL2_MAX_DB_BUF_LEN, "state=%d and type=%d",
                     TABLE_COMPUTE_RESOURCE_COLUMN_STATE_COMPLETE, TABLE_COMPUTE_RESOURCE_COLUMN_TYPE_XCP);
            lc_db_compute_resource_load_all_v2(hosts, "ip,domain", conditions, sizeof(host_t), &n_host);

            for (i = 0; i < n_host; i++) {
                snprintf(url, LG_API_URL_SIZE, LCPD_API_PREFIX LCPD_API_MASTER_CTRL_IPS, hosts[i].ip,
                        LCPD_AGEXEC_DEST_PORT, LCPD_API_VERSION);
                ret = pack_master_ctrl_ip_post(post, sizeof(post), "lcpd", hosts[i].domain, master_ctrl_ip);
                if (ret) {
                    continue;
                }
                LCPD_LOG(LOG_INFO, "Send url:%s post:%s\n", url, post);
                ret = lcpd_call_api(url, API_METHOD_POST, post, NULL);
                if (ret) {
                    LCPD_LOG(LOG_ERR, "@ERROR (APP): %s request "
                            "LCPD_API_MASTER_CTRL_IPS failed, host=%s.\n",
                            __FUNCTION__, hosts[i].ip);
                    continue;
                }
            }
        }

        sleep(SWITCH_MONITOR_INTERVAL);
        timestamp += SWITCH_MONITOR_INTERVAL;
        if (timestamp >= MONITOR_HOUR) {
            timestamp = 0;
        }
    }
}
