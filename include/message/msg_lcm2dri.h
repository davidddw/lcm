#ifndef _MESSAGE_MSG_LCM2DRI_H
#define _MESSAGE_MSG_LCM2DRI_H

#include <lc_netcomp.h>

/*
 * Msg to DRI level
 */

struct msg_request_pool_add {
    int     pool_id;
    char    pool_name[LC_NAMESIZE];
    char    pool_desc[LC_NAMEDES];
};

struct msg_reply_pool_add {
    int     pool_id;
    char    pool_name[LC_NAMESIZE];

#define LC_MSG_RPOOL_ADD_SUCCESS     0
#define LC_MSG_RPOOL_ADD_FAILURE     1
    int     result;

#define LC_MSG_RPOOL_ADD_ERR_EXIST       1
#define LC_MSG_RPOOL_ADD_ERR_ALLOC       2
    int     code;
};

struct msg_request_pool_change {
    int     pool_id;
    char    pool_name[LC_NAMESIZE];
    char    pool_desc[LC_NAMEDES];
};

struct msg_reply_pool_change {
    int     pool_id;
    char    pool_name[LC_NAMESIZE];

#define LC_MSG_RPOOL_CHG_SUCCESS     0
#define LC_MSG_RPOOL_CHG_FAILURE     1
    int     result;

#define LC_MSG_RPOOL_CHG_NOT_EXIST   1
    int     code;
};

struct msg_request_pool_del {
    int     pool_id;
    char    pool_name[LC_NAMESIZE];
};

struct msg_reply_pool_del {
    int     pool_id;
    char    pool_name[LC_NAMESIZE];

#define LC_MSG_RPOOL_DEL_SUCCESS     0
#define LC_MSG_RPOOL_DEL_FAILURE     1
    int     result;

#define LC_MSG_RPOOL_DEL_NOT_EXIST   1
    int     code;
};

struct msg_request_server_add {
    int     pool_id;
    char    server_ip[LC_NAMESIZE];
    char    server_user[LC_NAMESIZE];
    char    server_password[LC_NAMESIZE];
};

struct msg_request_server_change {
    int     pool_id;
    char    server_ip[LC_NAMESIZE];
    char    server_user[LC_NAMESIZE];
    char    server_password[LC_NAMESIZE];
};

struct msg_request_server_del {
    int     pool_id;
    char    server_ip[LC_NAMESIZE];
};

struct msg_request_host_join {
    int     pool_id;
#define LC_HOST_MASTER                1
#define LC_HOST_SLAVE                 2
    int     role;
    char    server_ip[LC_NAMESIZE];
    char    server_user[LC_NAMESIZE];
    char    server_password[LC_NAMESIZE];
};

struct msg_request_host_eject {
    char    server_ip[LC_NAMESIZE];
    char    server_user[LC_NAMESIZE];
    char    server_password[LC_NAMESIZE];
};

struct msg_reply_server_add {
    int     pool_id;
    char    pool_name[LC_NAMESIZE];
    char    server_ip[LC_NAMESIZE];

#define LC_MSG_SERVER_ADD_SUCCESS     0
#define LC_MSG_SERVER_ADD_FAILURE     1
    int     result;

#define LC_MSG_SERVER_ADD_POOL_NOT_EXIST     1
#define LC_MSG_SERVER_ADD_INVALID_POOL       2
#define LC_MSG_SERVER_ADD_EXIST              3
#define LC_MSG_SERVER_ADD_POOL_IS_FULL       4
#define LC_MSG_SERVER_ADD_NOT_FOUND          5
    int     code;
};

struct msg_reply_server_change {
    int     pool_id;
    char    pool_name[LC_NAMESIZE];
    char    server_ip[LC_NAMESIZE];

#define LC_MSG_SERVER_CHG_SUCCESS     0
#define LC_MSG_SERVER_CHG_FAILURE     1
    int     result;

#define LC_MSG_SERVER_CHG_POOL_NOT_EXIST     1
#define LC_MSG_SERVER_CHG_NOT_EXIST          2
    int     code;
};

struct msg_reply_server_del {
    int     pool_id;
    char    pool_name[LC_NAMESIZE];
    char    server_ip[LC_NAMESIZE];

#define LC_MSG_SERVER_DEL_SUCCESS     0
#define LC_MSG_SERVER_DEL_FAILURE     1
    int     result;

#define LC_MSG_SERVER_DEL_POOL_NOT_EXIST     1
#define LC_MSG_SERVER_DEL_INVALID_POOL       2
#define LC_MSG_SERVER_DEL_NOT_EXIST          3
    int     code;
};

struct msg_reply_host_join {
    int     pool_id;
    char    pool_name[LC_NAMESIZE];
    char    server_ip[LC_NAMESIZE];

#define LC_MSG_HOST_JOIN_SUCCESS      0
#define LC_MSG_HOST_JOIN_FAILURE      1
    int     result;

#define LC_MSG_HOST_JOIN_POOL_NOT_EXIST      1
#define LC_MSG_HOST_JOIN_HOST_NOT_EXIST      2
#define LC_MSG_HOST_JOIN_POOL_IS_FULL        3
#define LC_MSG_HOST_JOIN_IN_ANOTHER_POOL     4
    int     code;
};

struct msg_reply_host_eject {
    int     pool_id;
    char    pool_name[LC_NAMESIZE];
    char    server_ip[LC_NAMESIZE];

#define LC_MSG_HOST_EJECT_SUCCESS     0
#define LC_MSG_HOST_EJECT_FAILURE     1
    int     result;

#define LC_MSG_HOST_EJECT_HOST_NOT_EXIST      1
#define LC_MSG_HOST_EJECT_HOST_NOT_IN_POOL    2
    int     code;
};

/* used by LCM to send VM backup message to driver */
struct msg_request_vm_backup {
    uint32_t      vnet_id;
    uint32_t      vl2_id;
    uint32_t      vm_id;
    /*
     * TODO: backup operator
     */
    char          description[LC_NAMEDES];
};

struct msg_reply_vm_backup {

#define LC_MSG_VM_BACKUP_SUCCESS     0
#define LC_MSG_VM_BACKUP_FAILURE     1
    int           result;

#define LC_MSG_VM_BACKUP_VM_NOT_EXIST          1
#define LC_MSG_VM_BACKUP_SERVER_UNREACHABLE    2
#define LC_MSG_VM_BACKUP_EXPORT_FAILURE        3
    int           code;
};

/* used by LCM to send a vm snapshot request to driver */
struct msg_request_vm_snapshot {
    uint32_t      vnet_id;
    uint32_t      vl2_id;
    uint32_t      vm_id;
    /*
     * TODO: backup operator
     */
    char          description[LC_NAMEDES];
};

struct msg_reply_vm_snapshot {

#define LC_MSG_VM_SNAPSHOT_SUCCESS     0
#define LC_MSG_VM_SNAPSHOT_FAILURE     1
    int           result;

#define LC_MSG_VM_SNAPSHOT_VM_NOT_EXIST          1
#define LC_MSG_VM_SNAPSHOT_SERVER_UNREACHABLE    2
#define LC_MSG_VM_SNAPSHOT_XE_FAILURE            3
    int           code;
};

 /* used by LCM to send vm snapshot recovery request to driver */
struct msg_request_vm_snap_recovery {
    uint32_t      vnet_id;
    uint32_t      vl2_id;
    uint32_t      vm_id;
    /*
     * TODO: backup operator
     */
    char          sname[LC_NAMESIZE];
};

struct msg_reply_vm_snap_recovery {

#define LC_MSG_VM_SNAP_RECOVERY_SUCCESS     0
#define LC_MSG_VM_SNAP_RECOVERY_FAILURE     1
    int           result;

#define LC_MSG_VM_SNAP_RECOVERY_VM_NOT_EXIST          1
#define LC_MSG_VM_SNAP_RECOVERY_SNAPSHOT_NOT_EXIST    2
#define LC_MSG_VM_SNAP_RECOVERY_SERVER_UNREACHABLE    3
#define LC_MSG_VM_SNAP_RECOVERY_XE_FAILURE            4
    int           code;
};

#endif /* _MESSAGE_MSG_LCM2APP_H */
