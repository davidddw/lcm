/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Kai WANG
 * Finish Date: 2013-02-28
 *
 */

#ifndef __OVS_RSYNC_H__
#define __OVS_RSYNC_H__

#include "ovs_stdinc.h"
#include "flow_acl.h"

#define DFI_CTRL_PORT    20006
#define DFI_DATA_PORT    20006

enum {
    MSG_MG_DFI           = DFI_VERSION_MAGIC | 0x5344, // SD
    MSG_MG_ACL           = DFI_VERSION_MAGIC | 0x5341, // SA
};

enum {
    MSG_TP_CTRL_REGISTER = 0xC0,
    MSG_TP_CTRL_REQUEST  = 0xC1,
    MSG_TP_DATA_REPLY    = 0xD2,
};


#define MAX_MSG_SIZE size_in_block(60000)
struct message {
    u32 magic;
    u32 type;
    u32 size;
    u32 seq;
    u8 data[MAX_MSG_SIZE];
};

struct data_message_handler {
    int  (*timer_open)(void);
    void (*timer_close)(void);
    int  (*data_process)(void *, u32, __be32);
};

int create_dfi_ctrl_socket(void);
int create_dfi_data_socket(void);
void exit_dfi_ctrl_socket(void);
void exit_dfi_data_socket(void);
int  rsync_dfi_ctrl_message(void *ctrl, u32 size, u32 type);
int  rsync_dfi_data_message(const struct data_message_handler *ovs);

#endif /* __OVS_RSYNC_H__ */
