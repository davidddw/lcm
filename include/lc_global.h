#ifndef _LC_GLOBAL_H
#define _LC_GLOBAL_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/un.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <pthread.h>
#include <getopt.h>
#include <syslog.h>

// #include "message.h"
#include "message/msg.h"
#include "lc_bus.h"

/* for atomic operations */
#include <alsa/iatomic.h>

#define LC_VEDGE_VNET_ID         1
#define LC_VEDGE_VL2_ID          1

#define MAX_THREAD               ( 18 + 1 )

/* Script path */
#define LC_SH_DIR                "/usr/local/livecloud/script"
#define LC_MAKE_ISO_STR          "/usr/local/livecloud/script/"\
                                 "lc_xen_makeiso.sh '%s' '%s' '%s' '%s' '%s' '%s' '%d' '%s' '%s' '%s' '%s'"
#define LC_MAKE_GWISO_STR        "/usr/local/livecloud/script/"\
                                 "lc_xen_gwiso.sh '%s' '%s' '%s' '%s' '%s' '%d' '%d' '%d' '%d' '%s' '%d' '%s' '%s'"
#define LC_ISO_UPLOAD_STR        "/usr/local/livecloud/script/"\
                                 "lc_xen_iso.sh %s %s %s upload %s.iso"
#define LC_ISO_DESTROY_STR       "/usr/local/livecloud/script/"\
                                 "lc_xen_iso.sh %s %s %s destroy %s.iso"
#define LC_LEARNING_STR          "/usr/local/livecloud/script/"\
                                 "lc_xen_vm_learning.sh -h %s -u %s -p %s"\
                                 " -n %d -l %d -m %d -v %s -i %s -t %s -y %d"
#define LC_BACKUP_CREATE_STR     "/usr/local/livecloud/script/"\
                                 "lc_xen_vm_backup.sh backup %s %s %s %s %s"\
                                 " %s %s %s %s"
#define LC_BACKUP_CREATE_STR1    "/usr/local/livecloud/script/"\
                                 "lc_xen_vm_backup.sh backup %s --with-snapshot %s %s %s %s %s"\
                                 " %s %s %s %s"
#define LC_BACKUP_DELETE_STR     "/usr/local/livecloud/script/"\
                                 "lc_xen_vm_backup.sh delete-backupdata "\
                                 "%s %s %s %s %s"
#define LC_BACKUP_RESTORE_STR    "/usr/local/livecloud/script/"\
                                 "lc_xen_vm_backup.sh restore %s %s %s %s %s"
#define LC_XEN_SYNC_STR          "/usr/local/livecloud/script/diskmgmt/" \
                                 "lc_xen_sr.sh setup-ha %s %s %s %s %s %s %s"
#define LC_XEN_SR_CREATE_SINGLE_STR "/usr/local/livecloud/script/diskmgmt/" \
                                    "lc_xen_sr.sh setup %s %s %s %s"
#define LC_STORAGE_UNINSTALL_PEER_STR "/usr/local/livecloud/script/diskmgmt/" \
                                      "lc_xen_sr.sh uninstall %s %s %s"
#define LC_STORAGE_UNINSTALL_SINGLE_STR "/usr/local/livecloud/script/diskmgmt/" \
                                        "lc_xen_sr.sh uninstall %s %s %s"
#define LC_VM_META_EXPORT_STR    "/usr/local/livecloud/script/" \
                                 "lc_xen_vm_metadata.sh export --name-label %s"
#define LC_VM_META_DELETE_STR    "/usr/local/livecloud/script/" \
                                 "lc_xen_vm_metadata.sh delete --name-label %s"
#define LC_XEN_VM_MIGRATE_S_STR  "python /usr/local/livecloud/script/" \
                                 "vm_migrate.py %s %s %s %s %s"
#define LC_VMWARE_CONSOLE_STR    "sh /usr/local/livecloud/script/" \
                                 "vmware_console.sh '%s' '%d' '%d' '%s'"
#define LC_VG_SIZE_STR           "sshpass -p %s ssh %s@%s " \
                                 "vgdisplay -C --noheadings --nosuffix " \
                                 "-o vg_size --units g LcVG-Storage"
#define LC_LV_TEMPLATE_SIZE_STR  "sshpass -p %s ssh %s@%s " \
                                 "lvdisplay -C --noheadings --nosuffix " \
                                 "-o lv_size --units g /dev/LcVG-Storage/SR_Template"
#define LC_GET_DOM0_MEMORY_STR   "sshpass -p %s ssh %s@%s " \
                                 "xe vm-list is-control-domain=true params=memory-static-max " \
                                 "power-state=running --minimal"
#define LC_XEN_CON_TEMPLATE_STR  "/usr/local/livecloud/script/" \
                                  "lc_xen_template_chk.sh connect %s %s %s %s %s"
#define LC_XEN_CHECK_TEMPLATE_STR "/usr/local/livecloud/script/" \
                                  "lc_xen_template_chk.sh check %s"
#define RESERVED_UNUSED_SIZE     2 /*2GiB for unused space*/
#define GW_DISK_SIZE             30 /*vGW is 30GiB*/
#define GW_VCPU_NUM              2
#define GW_VMEM_NUM              1024

#define LC_ISO_DIR               "/nfsiso"

/* DB constants */
#define LC_DB_VERSION            "2_2"

/* lc_bus server port */
#define LC_BUS_SERVER_PORT       20001
#define LC_BUS_BUFFER_SIZE       8192

/* lc_bus server port */
#define LC_BUS_SERVER_PORT       20001
#define LC_BUS_BUFFER_SIZE       8192

/* Kernel socket */
#define SOCK_KER_RCV_DRV         "/var/run/.sock_ker_drv"
#define SOCK_KER_RCV_APP         "/var/run/.sock_ker_app"
#define SOCK_KER_RCV_CTL         "/var/run/.sock_ker_ctl"

/* LCM socket */
#define SOCK_LCM_RCV_APP         "/tmp/.sock_lcm_healthchk"

/* Driver socket */
#define SOCK_DRV_RCV_APP         "/var/run/.sock_drv_app"
#define SOCK_VM_RCV_KER          "/var/run/.sock_vm_ker"

/* HealthCheck socket */
#define SOCK_CHK_RCV             "/var/run/.sock_chk"

/* APP socket */
#define SOCK_APP_RCV_KER_PREFIX  "/var/run/.sock_app_rcv_ker_"
#define SOCK_APP_RCV_LCM_PREFIX  "/var/run/.sock_app_rcv_lcm_"

/*Monitor socket*/
#define SOCK_MON_RCV_APP         "/var/run/.sock_mon_app"

/* test socket */
#define SOCK_CTL_RCV             "/var/run/.sock_ctl"

/* agexec socket */
#ifndef VMDRIVER_AGEXEC_PORT
#define VMDRIVER_AGEXEC_PORT     20004
#endif

/* agent report socket */
#ifndef AGENT_REPORT_LCPD_PORT
#define AGENT_REPORT_LCPD_PORT   20005
#endif

#define MAX_RCV_BUF_SIZE         (1 << 15)
#define MAX_LOG_BUF_SIZE         200
#define MAX_CMD_BUF_SIZE         1024
#define ERRDESC_LEN              256

#define MAX_APP_NUM              10

#define MAX_DATAQ_LEN            500

#define MIN_VLAN_ID              10
#define MAX_VLAN_ID              4094

#define MIN_VM_TPORT             25000
#define MAX_VM_TPORT             33000
#define MIN_VEDGE_TPORT          10900
#define MAX_VEDGE_TPORT          12899
#define MIN_VMWARE_CONSOLE_PORT  22901
#define MAX_VMWARE_CONSOLE_PORT  23299 /* two port group, real range 22901~23300 */

#define MIN_VEDGE_VRID           1
#define MAX_VEDGE_VRID           254

#define MASTER_PRIORITY          150
#define BACKUP_PRIORITY          100

#define LC_APP_ID_NULL           0
#define LC_APP_ID_VNET           1
#define LC_APP_ID_DRV            2

#define LC_VCD_PORT              20000
#define LC_VCD_ADDRESS           "127.0.0.1"
#define LC_VCD_PG_PREFIX         "LC_PG" /* no underscore */
#define LC_VCD_THUMBPRINT_LEN    64
#define LC_VCD_VM_PATH_LEN       128
#define LC_VCD_SSR_MAX_NUM       10

#define SSHPASS_STR              "sshpass -p %s ssh %s@%s %s"
#define LOG_PATH "/var/tmp/log_switch"

typedef struct lc_ker_sock_s {
    int sock_agent;
} lc_ker_sock_t;

typedef struct lc_app_sock_s {
    int sock_rcv_ker;
    int sock_rcv_lcm;
} lc_app_sock_t;

typedef struct lc_vm_sock_s {
    int sock_agexec;
} lc_vm_sock_t;

typedef struct lc_mon_sock_s {
    int sock_rcv_chk;
    int sock_rcv_agent;
    int sock_rcv_vcd;
} lc_mon_sock_t;

typedef struct lc_snf_sock_s {
    int sock_rcv_chk;
    int sock_rcv_agent;
    int sock_rcv_vcd;
    int sock_rcv_dfi;
    int sock_rcv_api;
} lc_snf_sock_t;

typedef struct lc_drv_sock_s {
    int sock_rcv;
} lc_drv_sock_t;

typedef struct lc_chk_sock_s {
    int sock_rcv;
} lc_chk_sock_t;

typedef struct lc_msg_queue_s {
    STAILQ_HEAD(,lc_msg_s) qhead;
    u_int32_t enq_cnt;
    u_int32_t deq_cnt;
    u_int32_t msg_cnt;
} lc_msg_queue_t;

typedef struct lc_dpq_queue_s {
    STAILQ_HEAD(,lc_data_s) qhead;
    u_int32_t enq_cnt;
    u_int32_t deq_cnt;
    u_int32_t dp_cnt;
} lc_dpq_queue_t;

typedef struct lc_pb_msg_queue_s {
    STAILQ_HEAD(,lc_pb_msg_s) qhead;
    u_int32_t enq_cnt;
    u_int32_t deq_cnt;
    u_int32_t msg_cnt;
} lc_pb_msg_queue_t;

typedef struct lc_msg_s{
    lc_mbuf_t mbuf;
    STAILQ_ENTRY(lc_msg_s) entries;
} lc_msg_t;

typedef struct lc_data_s{
    intptr_t data_ptr;
    STAILQ_ENTRY(lc_data_s) entries;
} lc_data_t;

typedef struct lc_pb_msg_s{
    lc_pb_mbuf_t mbuf;
    STAILQ_ENTRY(lc_pb_msg_s) entries;
} lc_pb_msg_t;

typedef size_t (*result_process)(void *buffer, size_t size, size_t nmemb, void *userp);

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#endif
