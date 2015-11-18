#ifndef _LC_ALLOC_DB_H
#define _LC_ALLOC_DB_H

#include <sys/queue.h>
#include "lc_allocator.h"

typedef struct lc_node alloc_node_t;
typedef struct lc_head alloc_head_t;
typedef struct lc_vgw alloc_vdc_t;
typedef struct lc_vm  alloc_vm_t;
typedef struct lc_host alloc_cr_t;
typedef struct lc_storage alloc_sr_t;
typedef struct lc_ip alloc_ip_t;

typedef struct alloc_pool {
    SLIST_ENTRY(alloc_pool) chain;

    uint32_t    poolid;
    uint32_t    poolctype;
    uint32_t    pooliptype;
    char        domain[LC_DOMAIN_LEN];
} alloc_pool_t;
SLIST_HEAD(alloc_poolh, alloc_pool);

typedef struct alloc_sr_con {
    uint32_t    srid;
} alloc_sr_con_t;

typedef struct alloc_vif {
    uint32_t    vifid;
    uint32_t    ifindex;
} alloc_vif_t;

typedef struct alloc_ip_res {
    char        address[LC_IPV4_LEN];
    uint32_t    netmask;
    uint32_t    vlantag;
    char        gateway[LC_IPV4_LEN];
} alloc_ip_res_t;

typedef struct alloc_vnet {
    uint32_t    vnetid;
    uint32_t    vdcid;
    uint32_t    haflag;
    char        server[LC_IPV4_LEN];
} alloc_vnet_t;

typedef struct alloc_vdisk {
    uint32_t    vdiskid;
    uint32_t    vdisize;
    char        srname[LC_NAMESIZE];
} alloc_vdisk_t;

/* DB load error code */
#define ALLOC_DB_OK                           0
#define ALLOC_DB_COMMON_ERROR                 1
#define ALLOC_DB_EMPTY_SET                    2

#define ALLOC_DB_FLAG_ELEMENT                 0x1
#define ALLOC_DB_FLAG_SLIST                   0x2

#define TABLE_ALLOC_SYS_CONF                  "sys_configuration"
#define TABLE_ALLOC_SYS_CONF_PARAM            "param_name"
#define TABLE_ALLOC_SYS_CONF_PARAM_CPU_RATIO  "vcpu_over_allocation_ratio"
#define TABLE_ALLOC_SYS_CONF_VALUE            "value"
#define TABLE_ALLOC_SYS_CONF_VALUE_LEN        64

#define TABLE_ALLOC_POOL                      "pool"
#define TABLE_ALLOC_POOL_ID                   "id"
#define TABLE_ALLOC_POOL_TYPE                 "type"
#define TABLE_ALLOC_POOL_TYPE_SERVER          1
#define TABLE_ALLOC_POOL_TYPE_GATEWAY         2
#define TABLE_ALLOC_POOL_TYPE_NSP             3
#define TABLE_ALLOC_POOL_CTYPE                "ctype"
#define TABLE_ALLOC_POOL_CTYPE_XEN            1
#define TABLE_ALLOC_POOL_CTYPE_VMWARE         2
#define TABLE_ALLOC_POOL_IPTYPE               "exclusive_ip"
#define TABLE_ALLOC_POOL_IPTYPE_GLOBAL        0
#define TABLE_ALLOC_POOL_IPTYPE_EXCLUSIVE     1
#define TABLE_ALLOC_POOL_DOMIAN               "domain"

#define TABLE_ALLOC_CR                        "compute_resource"
#define TABLE_ALLOC_CR_ID                     "id"
#define TABLE_ALLOC_CR_STATE                  "state"
#define TABLE_ALLOC_CR_STATE_COMPLETE         4
#define TABLE_ALLOC_CR_POOLID                 "poolid"
#define TABLE_ALLOC_CR_IP                     "ip"
#define TABLE_ALLOC_CR_CPU_TOTAL              "cpu_info"
#define TABLE_ALLOC_CR_CPU_USED               "cpu_used"
#define TABLE_ALLOC_CR_CPU_ALLOC              "cpu_alloc"
#define TABLE_ALLOC_CR_MEM_TOTAL              "mem_total"
#define TABLE_ALLOC_CR_MEM_USED               "mem_usage"
#define TABLE_ALLOC_CR_MEM_ALLOC              "mem_alloc"
#define TABLE_ALLOC_CR_DISK_TOTAL             "disk_total"
#define TABLE_ALLOC_CR_DISK_USED              "disk_usage"
#define TABLE_ALLOC_CR_DISK_ALLOC             "disk_alloc"
#define TABLE_ALLOC_CR_RACKID                 "rackid"
#define TABLE_ALLOC_CR_DOMAIN                 "domain"
#define TABLE_ALLOC_CR_SRV_FLAG               "service_flag"
#define TABLE_ALLOC_CR_SRV_FLAG_NULL          0
#define TABLE_ALLOC_CR_SRV_FLAG_HA            2

#define TABLE_ALLOC_CON_SR                    "storage_connection"
#define TABLE_ALLOC_CON_SR_SRID               "storage_id"
#define TABLE_ALLOC_CON_SR_HOST               "host_address"
#define TABLE_ALLOC_CON_SR_ACTIVE             "active"
#define TABLE_ALLOC_CON_SR_ACTIVE_N           0
#define TABLE_ALLOC_CON_SR_ACTIVE_Y           1

#define TABLE_ALLOC_SR                        "storage"
#define TABLE_ALLOC_SR_ID                     "id"
#define TABLE_ALLOC_SR_NAME                   "name"
#define TABLE_ALLOC_SR_DISK_TOTAL             "disk_total"
#define TABLE_ALLOC_SR_DISK_USED              "disk_used"
#define TABLE_ALLOC_SR_DISK_ALLOC             "disk_alloc"

#define TABLE_ALLOC_CON_HASR                  "ha_storage_connection"
#define TABLE_ALLOC_CON_HASR_SRID             "storage_id"
#define TABLE_ALLOC_CON_HASR_HOST             "host_address"
#define TABLE_ALLOC_CON_HASR_ACTIVE           "active"
#define TABLE_ALLOC_CON_HASR_ACTIVE_N         0
#define TABLE_ALLOC_CON_HASR_ACTIVE_Y         1

#define TABLE_ALLOC_HASR                      "ha_storage"
#define TABLE_ALLOC_HASR_ID                   "id"
#define TABLE_ALLOC_HASR_NAME                 "name"
#define TABLE_ALLOC_HASR_DISK_TOTAL           "disk_total"
#define TABLE_ALLOC_HASR_DISK_USED            "disk_used"
#define TABLE_ALLOC_HASR_DISK_ALLOC           "disk_alloc"

#define TABLE_ALLOC_VDC                       "vnet"
#define TABLE_ALLOC_VDC_ID                    "id"
#define TABLE_ALLOC_VDC_ERRNO                 "errno"
#define TABLE_ALLOC_VDC_ERRNO_NO_IP           0x00000040
#define TABLE_ALLOC_VDC_ERRNO_NO_DISK         0x00010000
#define TABLE_ALLOC_VDC_ERRNO_NO_MEMORY       0x00020000
#define TABLE_ALLOC_VDC_ERRNO_NO_CPU          0x00040000
#define TABLE_ALLOC_VDC_ERRNO_DB_ERROR        0x00000080
#define TABLE_ALLOC_VDC_ERRNO_CREATE_ERROR    0x00100000
#define TABLE_ALLOC_VDC_SYS_SR_NAME           "vdi_sys_sr_name"
#define TABLE_ALLOC_VDC_PUBIF1_IPNUM          "public_if1ip_num"
#define TABLE_ALLOC_VDC_PUBIF2_IPNUM          "public_if2ip_num"
#define TABLE_ALLOC_VDC_PUBIF3_IPNUM          "public_if3ip_num"
#define TABLE_ALLOC_VDC_PUBIF1_ISP            "public_if1isp"
#define TABLE_ALLOC_VDC_PUBIF2_ISP            "public_if2isp"
#define TABLE_ALLOC_VDC_PUBIF3_ISP            "public_if3isp"
#define TABLE_ALLOC_VDC_SERVER                "gw_launch_server"
#define TABLE_ALLOC_VDC_POOLID                "gw_poolid"
#define TABLE_ALLOC_VDC_DOMAIN                "domain"
#define TABLE_ALLOC_VDC_EX_SERVER             "exclude_server"
#define TABLE_ALLOC_VDC_CPU_NUM               2
#define TABLE_ALLOC_VDC_MEM_SIZE              1024
#define TABLE_ALLOC_VDC_DISK_SIZE             30

#define TABLE_ALLOC_VM                        "vm"
#define TABLE_ALLOC_VM_ID                     "id"
#define TABLE_ALLOC_VM_ERRNO                  "errno"
#define TABLE_ALLOC_VM_ERRNO_DB_ERROR         0x00000080
#define TABLE_ALLOC_VM_ERRNO_NO_DISK          0x00010000
#define TABLE_ALLOC_VM_ERRNO_NO_MEMORY        0x00040000
#define TABLE_ALLOC_VM_ERRNO_NO_CPU           0x00080000
#define TABLE_ALLOC_VM_ERRNO_NO_RESOURCE      0x00100000
#define TABLE_ALLOC_VM_ERRNO_CREATE_ERROR     0x00400000
#define TABLE_ALLOC_VM_ERR_NO_VDISK           0x08000000
#define TABLE_ALLOC_VM_CPU_NUM                "vcpu_num"
#define TABLE_ALLOC_VM_MEM_SIZE               "mem_size"
#define TABLE_ALLOC_VM_SYS_VDI_SIZE           "vdi_sys_size"
#define TABLE_ALLOC_VM_USR_VDI_SIZE           "vdi_user_size"
#define TABLE_ALLOC_VM_EX_SERVER              "exclude_server"
#define TABLE_ALLOC_VM_SYS_SR_NAME            "vdi_sys_sr_name"
#define TABLE_ALLOC_VM_USR_SR_NAME            "vdi_user_sr_name"
#define TABLE_ALLOC_VM_SERVER                 "launch_server"
#define TABLE_ALLOC_VM_POOLID                 "poolid"
#define TABLE_ALLOC_VM_DOMAIN                 "domain"

#define TABLE_ALLOC_VIF                       "vinterface"
#define TABLE_ALLOC_VIF_ID                    "id"
#define TABLE_ALLOC_VIF_IFINDEX               "ifindex"
#define TABLE_ALLOC_VIF_DEVICETYPE            "devicetype"
#define TABLE_ALLOC_VIF_DEVICETYPE_VM         1
#define TABLE_ALLOC_VIF_DEVICETYPE_GW         2
#define TABLE_ALLOC_VIF_DEVICEID              "deviceid"
#define TABLE_ALLOC_VIF_MAX_PUB_IFINDEX       3
#define TABLE_ALLOC_VIF_IP                    "ip"
#define TABLE_ALLOC_VIF_NETMASK               "netmask"
#define TABLE_ALLOC_VIF_VLAN                  "vlantag"
#define TABLE_ALLOC_VIF_GATEWAY               "gateway"

#define TABLE_ALLOC_VNET                      "vnet"
#define TABLE_ALLOC_VNET_ID                   "id"
#define TABLE_ALLOC_VNET_VDCID                "vdcid"
#define TABLE_ALLOC_VNET_SERVER               "gw_launch_server"
#define TABLE_ALLOC_VNET_HA_FLAG              "haflag"
#define TABLE_ALLOC_VNET_HA_FLAG_SINGLE       1
#define TABLE_ALLOC_VNET_HA_FLAG_HA           2

#define TABLE_ALLOC_VDISK                     "vdisk"
#define TABLE_ALLOC_VDISK_VMID                "vmid"
#define TABLE_ALLOC_VDISK_SIZE                "size"
#define TABLE_ALLOC_VDISK_SR_NAME             "vdi_sr_name"

int updatedb(char* table, char* set, char* where);
int db2allocpool(void* object, int flag, char* where);
int db2alloccr(void* object, int flag, char* where, int htype, int iptype);
int db2allocsrcon(void* object, int flag, char* where);
int db2allochasrcon(void* object, int flag, char* where);
int db2allocsr(void* object, char* where);
int db2allochasr(void* object, char* where);
int db2allocvdc(void* object, char* where);
int db2allocvm(void* object, int flag, char* where);
int db2allocvif(void* object, char* where);
int db2allocvnet(void* object, char* where);
int db2allocvdisk(void* object, char* where);
int db2sysconf(char* value, char* where);
#endif
