/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Kai WANG
 * Finish Date: 2013-02-28
 *
 */

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#include "lc_snf.h"
#include "lc_db.h"
#include "lcs_utils.h"
#include "ovs_rsync.h"
#include "ovs_process.h"

static const struct data_message_handler dfi_hook = {
    .timer_open        = ovs_timer_open,
    .timer_close       = ovs_timer_close,
    .data_process      = ovs_data_process,
};

void *ovs_dfi_thread(void *msg)
{
    int rv = 0;
    struct user_acl user_setted_acl = {
        .magic  = DFI_VERSION_MAGIC,
        .state  = ACL_OPEN,
        .space  = ACL_IN_USER,
        .option = ACL_FLOW_ANY | ACL_RESET,
    };

    memset(&user_setted_acl.key, 0, sizeof(user_setted_acl.key));
    memset(&user_setted_acl.key_wc, 0, sizeof(user_setted_acl.key_wc));

    pthread_detach(pthread_self());

    lc_db_thread_init();

    rv += rsync_dfi_ctrl_message("", 0, MSG_TP_CTRL_REGISTER);
    rv += rsync_dfi_ctrl_message(&user_setted_acl,
            sizeof(struct user_acl), MSG_TP_CTRL_REQUEST);
    if (rv < 0) {
        exit(1);
    }

    rsync_dfi_data_message(&dfi_hook);

    return NULL;
}
