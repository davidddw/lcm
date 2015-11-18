#include "lcs_utils.h"
#include "lc_postman.h"
#include "lc_bus.h"
#include "lc_header.h"
#include "lc_db.h"
#include "lc_snf.h"
#include <stdio.h>
#include <string.h>

int lc_bus_lcsnf_publish_unicast(char *msg, int len, uint32_t queue_id)
{
    return lc_bus_publish_unicast((char *)msg, len, queue_id);
}

int send_email_through_postman(int rtype, int etype, monitor_host_t *hostp, vm_info_t *vm_info, char *email_msg, int level)
{
    header_t hdr;
    char cmd[MAX_CMD_BUF_SIZE];
    time_t current;
    char subject[50];
    char mail_buf[MAX_CMD_BUF_SIZE];
    int is_admin = 0;
    send_request_t send_req;
    char *to = NULL, *objtype = NULL;
    int mlen = sizeof(mail_buf);
    int hlen = 0;
    int ret = 0;
    int addrlen = 0;

    if (vm_info) {
        if (vm_info->vm_type == LC_VM_TYPE_VM) {
            objtype = LC_SYSLOG_OBJECTTYPE_VM;
        } else {
            objtype = LC_SYSLOG_OBJECTTYPE_VGW;
        }
/*        lc_monitor_db_syslog("", objtype, vm_info->vm_id,
                    vm_info->vm_label, email_msg, level);*/
    } else {
/*        lc_monitor_db_syslog("", LC_SYSLOG_OBJECTTYPE_HOST,
                         hostp->cr_id, hostp->host_address,
                                         email_msg, level);*/
    }

    if (rtype == POSTMAN__RESOURCE__HOST) {
        to = hostp->email_address;
    } else if (rtype == POSTMAN__RESOURCE__VM) {
        to = vm_info->email_address;
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
    send_req.policy.aggregate_id = POSTMAN__AGGREGATE_ID__AGG_HOST_USAGE_ALARM_EVENT;
    send_req.policy.aggregate_window = 10;
    send_req.policy.min_event_interval = 86400;
    send_req.policy.min_dest_interval = 0;
    send_req.policy.issue_timestamp = current;
    if (rtype == POSTMAN__RESOURCE__HOST) {
        send_req.id.event_type = etype;
        send_req.id.resource_type = POSTMAN__RESOURCE__HOST;
        send_req.id.resource_id = hostp->cr_id;
    } else if (rtype == POSTMAN__RESOURCE__VM) {
        send_req.id.event_type = etype;
        send_req.id.resource_type = POSTMAN__RESOURCE__VM;
        send_req.id.resource_id = vm_info->vm_id;
    }
    strcpy(subject, "Controller Message");
    strcpy(send_req.mail.from, hostp->from);
    strcpy(send_req.mail.to, to);
    strcpy(send_req.mail.subject, subject);
    if (level == LC_SYSLOG_LEVEL_WARNING) {
        send_req.mail.mail_type = POSTMAN__MAIL_TYPE__ALARM_MAIL;
    } else {
        send_req.mail.mail_type = POSTMAN__MAIL_TYPE__NOTIFY_MAIL;
    }
    if (rtype == POSTMAN__RESOURCE__HOST) {
        strcpy(send_req.mail.head_template, "head_alarm_to_admin");
        strcpy(send_req.mail.tail_template, "tail_alarm_to_admin");
        strcpy(send_req.mail.resource_type, "Host");
        strcpy(send_req.mail.resource_name, hostp->host_address);
    } else if (rtype == POSTMAN__RESOURCE__VM) {
        strcpy(send_req.mail.head_template, "head_alarm_to_user");
        strcpy(send_req.mail.tail_template, "tail_alarm_to_user");
        strcpy(send_req.mail.resource_type, "VM");
        strcpy(send_req.mail.resource_name, vm_info->vm_name);
    }
    if (hostp->email_enable) {
        if (0 == strcmp(to, hostp->email_address)) {
            is_admin = 1;
        } else {
            strcpy(send_req.mail.cc, hostp->email_address);
        }
    }
    if (hostp->email_address_ccs[0]) {
        addrlen = strlen(send_req.mail.cc);
        snprintf(send_req.mail.cc + addrlen,
                 MAIL_ADDR_LEN - addrlen,",%s", hostp->email_address_ccs);
    }

    strncpy(send_req.mail.alarm_message, email_msg, LC_EMAIL_ALARM_MESSAGE_LEN);
    send_req.mail.alarm_begin = current;

    /* publish send_request to postman */

    hlen = get_message_header_pb_len();
    mlen = construct_send_request(&send_req, mail_buf + hlen, mlen - hlen);

    hdr.sender = HEADER__MODULE__LCSNFD;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = mlen;
    hdr.seq = 0;
    fill_message_header(&hdr, mail_buf);

    ret = lc_bus_lcsnf_publish_unicast(mail_buf, mlen + hlen, LC_BUS_QUEUE_POSTMAND);
    if (ret) {
        SNF_SYSLOG(LOG_ERR, "%s error send mail to postman", __FUNCTION__);
    }

    return 0;
}

int lcs_get_domain_from_config()
{
    FILE *fp = NULL;
    char buf[MAX_LOG_MSG_LEN] = {0};
    char *s = NULL;
    char *key, *value;
    int flag = 0;

    fp = fopen(LCS_CONFIG_FILE, "r");
    if (NULL == fp) {
        SNF_SYSLOG(LOG_ERR, "%s error open config file", __FUNCTION__);
        return 1;
    }

    while (fgets(buf, MAX_LOG_MSG_LEN, fp) != NULL) {
        if (buf[0] == '#' || buf[0] == '\n') {
            continue;
        }
        if (buf[0] == '[') {
            for (s = buf + strlen(buf) - 1; *s == ']' ||  *s == ' ' ||  *s == '\n'; s--) {
                *s = '\0';
            }
            for (s = buf; *s == '[' || *s == ' '; s++) {
                //do nothing
            }
            if (strcmp(s, LCS_SIGN) && strcmp(s, LCS_GLOBAL_SIGN)) {
                flag = 0;
            } else {
                flag = 1;
            }
            continue;
        }

        if (!flag) {
            continue;
        }

        key = strtok(buf, "=");
        for (s = key + strlen(key) - 1; *s == ' '; s--) {
            *s = '\0';
        }
        for (; *key == ' '; key++) {
            //do nothing
        }

        value = strtok(NULL, "=");
        for (s = value + strlen(value) - 1; *s == ' ' ||  *s == '\n'; s--) {
            *s = '\0';
        }
        for (; *value == ' '; value++) {
            //do nothing
        }

        if (!strcmp(key, LCS_KEY_DOMAIN_LCUUID)) {
            strncpy(lcsnf_domain.lcuuid, value, sizeof(lcsnf_domain.lcuuid));
        }
        if (!strcmp(key, LCS_KEY_MASTER_CTRL_IP)) {
            strncpy(lcsnf_domain.master_ctrl_ip, value, sizeof(lcsnf_domain.master_ctrl_ip));
        }
        if (!strcmp(key, LCS_KEY_LOCAL_CTRL_IP)) {
            strncpy(lcsnf_domain.local_ctrl_ip, value, sizeof(lcsnf_domain.local_ctrl_ip));
        }
        if (!strcmp(key, LCS_KEY_MYSQL_MASTER_IP)) {
            strncpy(lcsnf_domain.mysql_master_ip, value, sizeof(lcsnf_domain.mysql_master_ip));
        }
    }

    fclose(fp);
    if (!lcsnf_domain.lcuuid[0]) {
        SNF_SYSLOG(LOG_ERR, "No domain.lcuuid in conf");
        return 1;
    }
    if (!strcmp(lcsnf_domain.lcuuid, LCS_DOMAIN_LCUUID_DEFAULT)) {
        SNF_SYSLOG(LOG_ERR, "Wrong domain.lcuuid in conf");
        return 1;
    }
    if (!strcmp(lcsnf_domain.master_ctrl_ip, "127.0.0.1")) {
        strncpy(lcsnf_domain.master_ctrl_ip, lcsnf_domain.local_ctrl_ip, sizeof(lcsnf_domain.master_ctrl_ip));
    }
    return 0;
}
