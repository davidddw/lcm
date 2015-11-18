#ifndef __LC_ALLOCATOR_H
#define __LC_ALLOCATOR_H 1

#include <stdint.h>
#include <sys/queue.h>

#ifndef LC_NAMESIZE
#define LC_NAMESIZE                             80
#endif
#ifndef LC_IPV4_LEN
#define LC_IPV4_LEN                             16
#endif
#ifndef LC_DOMAIN_LEN
#define LC_DOMAIN_LEN                           64
#endif
#ifndef LC_EXCLUDED_SERVER_LIST_LEN
#define LC_EXCLUDED_SERVER_LIST_LEN             128
#endif

#define LC_EXCLUDED_SERVER_LEN                  (LC_DOMAIN_LEN + LC_IPV4_LEN + 2)

#define LC_VGW_PUBINTF_NUM                      3
#define LC_VGW_IP_NUM_PER_INTF                  10
#define LC_VGW_IPS_LEN                          (LC_VGW_IP_NUM_PER_INTF * LC_IPV4_LEN)

/* flag */
#define LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION      0x00000001

/* return value */
#define LC_ALLOC_RET_OK                         0x00000000
#define LC_ALLOC_RET_GATEWAY_FAILED             0x00000001
#define LC_ALLOC_RET_ALL_VM_FAILED              0x00000002
#define LC_ALLOC_RET_SOME_VM_FAILED             0x00000004
#define LC_ALLOC_RET_FAILED                     0x00008000

/* pool ip type */
#define LC_POOL_IP_TYPE_GLOBAL                  1
#define LC_POOL_IP_TYPE_EXCLUSIVE               2

/* storage type */
#define LC_STORAGE_TYPE_LOCAL                   1
#define LC_STORAGE_TYPE_SHARED                  2

/* alloc errno */
#define LC_ALLOC_VM_ERRNO_NO_DISK               0x00000001
#define LC_ALLOC_VM_ERRNO_NO_MEMORY             0x00000002
#define LC_ALLOC_VM_ERRNO_NO_CPU                0x00000004
#define LC_ALLOC_VM_ERRNO_NO_HOST               0x00000008

#define LC_ALLOC_VGW_ERRNO_NO_DISK              0x00000001
#define LC_ALLOC_VGW_ERRNO_NO_MEMORY            0x00000002
#define LC_ALLOC_VGW_ERRNO_NO_CPU               0x00000004
#define LC_ALLOC_VGW_ERRNO_NO_IP                0x00000008
#define LC_ALLOC_VGW_ERRNO_NO_HOST              0x00000010
#define LC_ALLOC_VGW_ERRNO_TOO_MANY_VGWS        0x00000020
#define LC_ALLOC_VGW_ERRNO_VGWS_INFO_MISMATCH   0x00000040

/* slist */
struct lc_node {
    SLIST_ENTRY(lc_node) chain;
    void *element;
};
SLIST_HEAD(lc_head, lc_node);

struct lc_vgw {
    uint32_t        id;
    uint32_t        cpu_num;
    uint32_t        mem_size;
    uint32_t        sys_vdi_size;
    char            excluded_server[LC_EXCLUDED_SERVER_LIST_LEN];
    uint32_t        ip_num[LC_VGW_PUBINTF_NUM]; /* ip number on public interfaces */
    uint32_t        ip_isp[LC_VGW_PUBINTF_NUM];

    char            sys_sr_name[LC_NAMESIZE];
    char            server[LC_IPV4_LEN];
    uint32_t        pool_id;
    char            domain[LC_DOMAIN_LEN];
    /* ip allocated: <ip1>#<ip2>#<ip3>#...#<ipn> */
    char            ip[LC_VGW_PUBINTF_NUM][LC_VGW_IPS_LEN];
    uint32_t        alloc_errno;
    uint32_t        pool_ip_type;
};

struct lc_vm {
    uint32_t        id;
    uint32_t        cpu_num;
    uint32_t        mem_size;
    uint32_t        sys_vdi_size;
    uint32_t        usr_vdi_size;
    char            excluded_server[LC_EXCLUDED_SERVER_LIST_LEN];

    char            sys_sr_name[LC_NAMESIZE];
    char            usr_sr_name[LC_NAMESIZE];
    char            server[LC_IPV4_LEN];
    uint32_t        pool_id;
    char            domain[LC_DOMAIN_LEN];
    uint32_t        alloc_errno;
};

struct lc_storage {
    char            name_label[LC_NAMESIZE];
    uint32_t        storage_type;
    uint32_t        disk_total;
    uint32_t        disk_used;
};

struct lc_host {
    uint32_t        id;
    char            address[LC_IPV4_LEN];
    uint32_t        pool_id;
    char            domain[LC_DOMAIN_LEN];
    uint32_t        rack_id;
    uint32_t        cpu_total;
    uint32_t        cpu_used;
    uint32_t        mem_total;
    uint32_t        mem_used;
    struct lc_head  storage;
    uint32_t        pool_htype;
    uint32_t        pool_ip_type;
};

struct lc_ip {
    char            address[LC_IPV4_LEN];
    uint32_t        pool_id;
    uint32_t        isp;
};

int lc_alloc_vgateway(struct lc_head *vgws, struct lc_head *vgw_host);
int lc_alloc_vm(struct lc_head *vms, struct lc_head *vm_host,
                struct lc_head *vgws, struct lc_head *vgw_host,
                struct lc_head *ips, int flag);

int lc_check_vm(struct lc_head *vms, struct lc_head *vm_host,
                struct lc_head *vgws, struct lc_head *vgw_host, int flag);

#endif
