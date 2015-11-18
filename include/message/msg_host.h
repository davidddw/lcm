#ifndef _MESSAGE_MSG_HOST_H
#define _MESSAGE_MSG_HOST_H

/*
 * Msg for host
 */
#define HOST_CHANGE_MASK_USERNAME    1
#define HOST_CHANGE_MASK_PASSWORD    2
#define HOST_CHANGE_MASK_IP          4
#define HOST_CHANGE_MASK_IFTYPE      8
#define HOST_CHANGE_MASK_MAX         0xF
#define IPV4_ADDR_LENGTH            16
#define PORT_LIST_LENGTH            256
#define PORT_MAC_LENGTH             32
struct msg_host_device_opt {
    int    host_id;
    int    host_chgmask;
    int    host_is_ofs;
};

struct msg_nsp_opt {
    char     host_address[MAX_HOST_ADDRESS_LEN];
    uint32_t host_state;
    uint32_t pool_index;
};

struct msg_compute_resource_opt {
    int    compute_id;
    int    network_id;
};

struct msg_host_port_ofs_config_opt {
    int    host_port_ofs_config_id;
    int    ctrl_vlan_vid;
    char   port_type[PORT_MAC_LENGTH];
    char   ofs_ip[IPV4_ADDR_LENGTH];

};

struct msg_ofs_port_config_opt {
    int    uplink_port0;
    int    uplink_port1;
    int    ctrl_vlan_vid;
    char   ofs_ip[IPV4_ADDR_LENGTH];
    char   ofs_crl_port_list[PORT_LIST_LENGTH];
    char   ofs_data_port_list[PORT_LIST_LENGTH];
};


struct msg_storage_resource_opt {
    int    storage_id;
};

struct msg_network_resource_opt {
    int    network_id;
};

struct msg_host_ip_opt {
    char   host_ip[MAX_HOST_ADDRESS_LEN];
};

#endif
