#include "lc_healthcheck.h"

int lc_chk_lcm(struct lc_msg_head *m_head);
int lc_chk_kernel(struct lc_msg_head *m_head);
int lc_chk_vmdriver(struct lc_msg_head *m_head);
int lc_chk_monitor(struct lc_msg_head *m_head);

int chk_msg_handler(struct lc_msg_head *m_head)
{
    if (!is_lc_msg(m_head)) {
        return -1;
    }

    if (!is_msg_to_chk(m_head)) {
        return -1;
    }
    LC_LOG(LOG_DEBUG, "%s type=%d, len=%d\n",
            __FUNCTION__, m_head->type, m_head->length);
    LC_LOG(LOG_INFO, "%s type = %d in len=%d\n",
            __FUNCTION__, m_head->type, m_head->length);

    switch (m_head->type) {
    case LC_MSG_LCM_REPLY_CHK_HEARTBEAT:
        lc_chk_lcm(m_head);
        break;
    case LC_MSG_KER_REPLY_CHK_HEARTBEAT:
        lc_chk_kernel(m_head);
        break;
    case LC_MSG_DRV_REPLY_CHK_HEARTBEAT:
        lc_chk_vmdriver(m_head);
        break;
    case LC_MSG_MON_REPLY_CHK_HEARTBEAT:
        lc_chk_monitor(m_head);
        break;
    default:
        return -1;
    }

    return 0;
}

int lc_chk_lcm(struct lc_msg_head *m_head)
{
    LC_LOG(LOG_INFO, "%s: heartbeat reply from lcm\n", __FUNCTION__);
    LC_HB_CNT_RST(lcm);

    return 0;
}

int lc_chk_kernel(struct lc_msg_head *m_head)
{
    LC_LOG(LOG_INFO, "%s: heartbeat reply from kernel\n", __FUNCTION__);
    LC_HB_CNT_RST(kernel);

    return 0;
}

int lc_chk_vmdriver(struct lc_msg_head *m_head)
{
    LC_LOG(LOG_INFO, "%s: heartbeat reply from vmdriver\n", __FUNCTION__);
    LC_HB_CNT_RST(vmdriver);

    return 0;
}

int lc_chk_monitor(struct lc_msg_head *m_head)
{
    LC_LOG(LOG_INFO, "%s: heartbeat reply from monitor\n", __FUNCTION__);
    LC_HB_CNT_RST(monitor);

    return 0;
}
