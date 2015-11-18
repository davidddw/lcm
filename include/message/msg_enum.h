#ifndef _MESSAGE_MSG_ENUM_H
#define _MESSAGE_MSG_ENUM_H

enum {
    MSG_MG_START   = 0,
    MSG_MG_END     = 1,
    MSG_MG_DRV2KER = 0x4C43444B, /* ascii "LCDK" */
    MSG_MG_KER2DRV = 0x4C434B44, /* ascii "LCKD" */
    MSG_MG_KER2APP = 0x4C434B41, /* ascii "LCKA" */
    MSG_MG_APP2KER = 0x4C43414B, /* ascii "LCAK" */
    MSG_MG_APP2UI  = 0x4C434155, /* ascii "LCAU" */
    MSG_MG_UI2APP  = 0x4C435541, /* ascii "LCUA" */
    MSG_MG_DRI2UI  = 0x4C434455, /* ascii "LCDU" */
    MSG_MG_UI2DRV  = 0x4C435544, /* ascii "LCUD" */
    MSG_MG_KER2UI  = 0x4C434B55, /* ascii "LCKU" */
    MSG_MG_UI2KER  = 0x4C43554B, /* ascii "LCUK" */
    MSG_MG_CHK2UI  = 0x4C434355, /* ascii "LCCU" */
    MSG_MG_CHK2KER = 0x4C43434B, /* ascii "LCCK" */
    MSG_MG_CHK2DRV = 0X4C434344, /* ascii "LCCD" */
    MSG_MG_CHK2MON = 0X4C43434D, /* ascii "LCCM" */
    MSG_MG_MON2CHK = 0X4C434D43, /* ascii "LCMC" */
    MSG_MG_CHK2SNF = 0X4C434353, /* ascii "LCCS" */
    MSG_MG_SNF2CHK = 0X4C435343, /* ascii "LCSC" */
    MSG_MG_UI2CHK  = 0x4C435543, /* ascii "LCUC" */
    MSG_MG_KER2CHK = 0x4C434B43, /* ascii "LCKC" */
    MSG_MG_DRV2CHK = 0X4C434443, /* ascii "LCDC" */
    MSG_MG_DRV2VCD = 0X4C434456, /* ascii "LCDV" */
    MSG_MG_VCD2DRV = 0X4C435644, /* ascii "LCVD" */
    MSG_MG_APP2VCD = 0X4C434156, /* ascii "LCAV" */
    MSG_MG_VCD2APP = 0X4C435641, /* ascii "LCVA" */
    MSG_MG_KER2VCD = 0X4C434B56, /* ascii "LCKV" */
    MSG_MG_VCD2KER = 0X4C43564B, /* ascii "LCVK" */
    MSG_MG_UI2MON  = 0X4C43554D, /* ascii "LCUM" */
    MSG_MG_MON2DRV = 0X4C434D44, /* ascii "LCMD" */
    MSG_MG_VCD2MON = 0X4C43564D, /* ascii "LCVM" */
    MSG_MG_UI2VWA  = 0x4C435557, /* ascii "LCUW" */
    MSG_MG_VWA2KER = 0X4C43574B, /* ascii "LCWK" */
    MSG_MG_UI2RM   = 0x4C435552, /* ascii "LCUR" */
    MSG_MG_RM2DRV  = 0X4C435244, /* ascii "LCRD" */
    MSG_MG_DRV2RM  = 0X4C434452, /* ascii "LCDR" */
    MSG_MG_MON2KER = 0X4C434D4B, /* ascii "LCMK" */
};

enum {
    MSG_DIR_DRV2KER = (1UL << 0),
    MSG_DIR_KER2DRV = (1UL << 1),
    MSG_DIR_KER2APP = (1UL << 2),
    MSG_DIR_APP2KER = (1UL << 3),
    MSG_DIR_APP2UI  = (1UL << 4),
    MSG_DIR_UI2APP  = (1UL << 5),
    MSG_DIR_DRI2UI  = (1UL << 6),
    MSG_DIR_UI2DRI  = (1UL << 7),
    MSG_DIR_KER2VMDRV  = (1UL << 8),
    MSG_DIR_CHK2UI  = (1UL << 9),
    MSG_DIR_CHK2KER = (1UL << 10),
    MSG_DIR_CHK2DRV = (1UL << 11),
    MSG_DIR_CHK2MON = (1UL << 12),
    MSG_DIR_UI2CHK  = (1UL << 13),
    MSG_DIR_KER2CHK = (1UL << 14),
    MSG_DIR_DRV2CHK = (1UL << 15),
    MSG_DIR_MON2CHK = (1UL << 16),
    MSG_DIR_CHK2SNF = (1UL << 17),
    MSG_DIR_SNF2CHK = (1UL << 18),
};

enum {
    LC_MSG_NOTHING = 0,
    /* vcd reply message */
    LC_MSG_VMD_REQUEST_VCD_VM_ADD = 110,
    LC_MSG_VCD_REPLY_VMD_VM_ADD = 111,
    LC_MSG_VMD_REQUEST_VCD_VM_IMPORT = 112,
    LC_MSG_VCD_REPLY_VMD_VM_IMPORT = 113,
    LC_MSG_VMD_REQUEST_VCD_VM_UPDATE = 121,
    LC_MSG_VCD_REPLY_VMD_VM_UPDATE = 122,
    LC_MSG_VMD_REQUEST_VCD_VM_STATS = 123,
    LC_MSG_VCD_REPLY_VMD_VM_STATS = 124,
    LC_MSG_VMD_REQUEST_VCD_VM_OPS = 125,
    LC_MSG_VCD_REPLY_VMD_VM_OPS = 126,
    LC_MSG_VMD_REQUEST_VCD_VM_DEL = 127,
    LC_MSG_VCD_REPLY_VMD_VM_DEL = 128,
    LC_MSG_NWD_REQUEST_VCD_PG_CREATE = 140,
    LC_MSG_VCD_REPLY_NWD_PG_CREATE = 141,
    LC_MSG_NWD_REQUEST_VCD_PG_DEL = 142,
    LC_MSG_VCD_REPLY_NWD_PG_DEL = 143,
    LC_MSG_NWD_REQUEST_VCD_PG_UPDATE = 146,
    LC_MSG_VCD_REPLY_NWD_PG_UPDATE = 147,
    LC_MSG_NWD_REQUEST_VCD_DVPORT_BIND = 150,
    LC_MSG_VCD_REPLY_NWD_DVPORT_BIND = 151,
    LC_MSG_NWD_REQUEST_VCD_DVPORT_UNBIND = 152,
    LC_MSG_VCD_REPLY_NWD_DVPORT_UNBIND = 153,
    LC_MSG_NWD_REQUEST_VCD_DVPORT_UPDATE = 154,
    LC_MSG_VCD_REPLY_NWD_DVPORT_UPDATE = 155,
    LC_MSG_VMD_REQUEST_VCD_HOST_STATS = 172,
    LC_MSG_VCD_REPLY_VMD_HOST_STATS = 173,
    LC_MSG_VCD_VM_SNAPSHOT_ADD = 181,
    LC_MSG_VCD_VM_SNAPSHOT_DEL = 182,
    LC_MSG_VCD_VM_SNAPSHOT_REVERT = 183,
    LC_MSG_VCD_VM_CONSOLE = 184,
    LC_MSG_VCD_VM_MIGRATE = 185,
    LC_MSG_VMD_REQUEST_VCD_VM_VIF_USAGE = 186,
    LC_MSG_VCD_REPLY_VMD_VM_VIF_USAGE = 187,
    LC_MSG_VCD_RESP_VMD_VM_VCENTER_INFO = 188,
    LC_MSG_VMD_REQUEST_VCD_VM_SET_DISK = 189,
    LC_MSG_VCD_REPLY_VMD_VM_SET_DISK = 190,

    /* host ops */
    LC_MSG_HOST_ADD = 1000,
    LC_MSG_HOST_ADD_ROLLBACK,
    LC_MSG_HOST_DEL,
    LC_MSG_HOST_DEL_ROLLBACK,
    LC_MSG_HOST_CHG,
    LC_MSG_HOST_CHG_ROLLBACK,
    LC_MSG_HOST_JOIN,
    LC_MSG_HOST_JOIN_ROLLBACK,
    LC_MSG_HOST_EJECT,
    LC_MSG_HOST_EJECT_ROLLBACK,
    LC_MSG_HOST_PIF_CONFIG,
    LC_MSG_HOST_PIF_CONFIG_ROLLBACK,
    LC_MSG_OFS_PORT_CONFIG,
    LC_MSG_OFS_PORT_CONFIG_ROLLBACK,

    LC_MSG_HOST_BOOT,
    LC_MSG_HOST_BOOT_ROLLBACK,

    /* nsp ops */
    LC_MSG_NSP_DOWN,
    LC_MSG_NSP_UP,

    /* sr ops */
    LC_MSG_SR_JOIN,
    LC_MSG_SR_JOIN_ROLLBACK,
    LC_MSG_SR_EJECT,
    LC_MSG_SR_EJECT_ROLLBACK,

    /* vm ops */
    LC_MSG_VM_ADD,
    LC_MSG_VM_ADD_ROLLBACK,
    LC_MSG_VM_ADD_RESUME,
    LC_MSG_VM_ADD_RESUME_ROLLBACK,
    LC_MSG_VM_DEL,
    LC_MSG_VM_DEL_ROLLBACK,
    LC_MSG_VM_DEL_RESUME,
    LC_MSG_VM_DEL_RESUME_ROLLBACK,
    LC_MSG_VM_START,
    LC_MSG_VM_START_ROLLBACK,
    LC_MSG_VM_START_RESUME,
    LC_MSG_VM_START_RESUME_ROLLBACK,
    LC_MSG_VM_STOP,
    LC_MSG_VM_STOP_ROLLBACK,
    LC_MSG_VM_STOP_RESUME,
    LC_MSG_VM_STOP_RESUME_ROLLBACK,
    LC_MSG_VM_SUSPEND,
    LC_MSG_VM_SUSPEND_ROLLBACK,
    LC_MSG_VM_RESUME,
    LC_MSG_VM_RESUME_ROLLBACK,
    LC_MSG_VM_CHG,
    LC_MSG_VM_CHG_ROLLBACK,
    LC_MSG_VM_ISOLATE,
    LC_MSG_VM_ISOLATE_ROLLBACK,
    LC_MSG_VM_RELEASE,
    LC_MSG_VM_RELEASE_ROLLBACK,
    LC_MSG_VM_SNAPSHOT_CREATE,
    LC_MSG_VM_SNAPSHOT_CREATE_ROLLBACK,
    LC_MSG_VM_SNAPSHOT_DELETE,
    LC_MSG_VM_SNAPSHOT_DELETE_ROLLBACK,
    LC_MSG_VM_SNAPSHOT_RECOVERY,
    LC_MSG_VM_SNAPSHOT_RECOVERY_ROLLBACK,
    LC_MSG_VM_BACKUP_CREATE,
    LC_MSG_VM_BACKUP_CREATE_ROLLBACK,
    LC_MSG_VM_BACKUP_DELETE,
    LC_MSG_VM_BACKUP_DELETE_ROLLBACK,
    LC_MSG_VM_RECOVERY,
    LC_MSG_VM_RECOVERY_ROLLBACK,
    LC_MSG_VM_MIGRATE,
    LC_MSG_VM_MIGRATE_ROLLBACK,
    LC_MSG_VM_VNC_CONNECT,
    LC_MSG_VM_VNC_CONNECT_ROLLBACK,
    LC_MSG_VM_REPLACE,
    LC_MSG_VM_REPLACE_ROLLBACK,
    LC_MSG_VM_PLUG_DISK,
    LC_MSG_VM_PLUG_DISK_ROLLBACK,
    LC_MSG_VM_UNPLUG_DISK,
    LC_MSG_VM_UNPLUG_DISK_ROLLBACK,
    LC_MSG_VM_SYS_DISK_MIGRATE,
    LC_MSG_VM_SYS_DISK_MIGRATE_ROLLBACK,

    LC_MSG_VEDGE_ADD,
    LC_MSG_VEDGE_ADD_ROLLBACK,
    LC_MSG_VEDGE_DEL,
    LC_MSG_VEDGE_DEL_ROLLBACK,
    LC_MSG_VEDGE_START,
    LC_MSG_VEDGE_START_ROLLBACK,
    LC_MSG_VEDGE_START_RESUME,
    LC_MSG_VEDGE_START_RESUME_ROLLBACK,
    LC_MSG_VEDGE_STOP,
    LC_MSG_VEDGE_STOP_ROLLBACK,
    LC_MSG_VEDGE_CHG,
    LC_MSG_VEDGE_CHG_ROLLBACK,
    LC_MSG_VEDGE_MIGRATE,
    LC_MSG_VEDGE_MIGRATE_ROLLBACK,
    LC_MSG_VEDGE_SWITCH,
    LC_MSG_VEDGE_SWITCH_ROLLBACK,
    LC_MSG_VEDGE_VNC_CONNECT,
    LC_MSG_VEDGE_VNC_CONNECT_ROLLBACK,
    LC_MSG_VEDGE_ADD_SECU,
    LC_MSG_VEDGE_ADD_SECU_ROLLBACK,
    LC_MSG_VEDGE_DEL_SECU,
    LC_MSG_VEDGE_DEL_SECU_ROLLBACK,

    LC_MSG_VDEVICE_BOOT,
    LC_MSG_VDEVICE_BOOT_ROLLBACK,
    LC_MSG_VDEVICE_DOWN,
    LC_MSG_VDEVICE_DOWN_ROLLBACK,

    /* vl2 ops */
    LC_MSG_VL2_ADD,
    LC_MSG_VL2_ADD_ROLLBACK,
    LC_MSG_VL2_DEL,
    LC_MSG_VL2_DEL_ROLLBACK,
    LC_MSG_VL2_CHANGE,
    LC_MSG_VL2_CHANGE_ROLLBACK,

    /* vif attach/detach */
    LC_MSG_VINTERFACE_ATTACH,
    LC_MSG_VINTERFACE_ATTACH_ROLLBACK,
    LC_MSG_VINTERFACE_DETACH,
    LC_MSG_VINTERFACE_DETACH_ROLLBACK,
    LC_MSG_VINTERFACES_CONFIG,

    LC_MSG_TORSWITCH_MODIFY,

    /* hardware device operations */
    LC_MSG_HWDEV_IF_ATTACH,
    LC_MSG_HWDEV_IF_DETACH,
    /*vgateway operation*/
    LC_MSG_VGATEWAY_ADD,
    LC_MSG_VGATEWAY_ADD_ROLLBACK,
    LC_MSG_VGATEWAY_DEL,
    LC_MSG_VGATEWAY_DEL_ROLLBACK,
    LC_MSG_VGATEWAY_CHG,
    LC_MSG_VGATEWAY_CHG_ROLLBACK,
    LC_MSG_VGATEWAY_MIGRATE,
    LC_MSG_VGATEWAY_MIGREATE_ROLLBACK,
    /*valve operation*/
    LC_MSG_VALVE_ADD,
    LC_MSG_VALVE_ADD_ROLLBACK,
    LC_MSG_VALVE_DEL,
    LC_MSG_VALVE_DEL_ROLLBACK,
    LC_MSG_VALVE_CHG,
    LC_MSG_VALVE_CHG_ROLLBACK,
    LC_MSG_VALVE_MIGRATE,
    LC_MSG_VALVE_MIGREATE_ROLLBACK,

    LC_MSG_DUMMY

};

enum {
    /* message from network to LCM */
    LC_MSG_REG_APP = 200,

    /*
     * BACKUP messages
     * Backup recovery is handled by VM_ADD message
     */
    LC_MSG_LCM_REQUEST_DRV_VM_BACKUP,
    LC_MSG_LCM_REQUEST_DRV_VM_SNAPSHOT_CREATE,
    LC_MSG_LCM_REQUEST_DRV_VM_SNAPSHOT_RECOVERY,

    /* message from dirver to LCM */
    LC_MSG_REG_DRV = 700,

    /* heartbeat messages */
    LC_MSG_CHK_REQUEST_LCM_HEARTBEAT = 800,
    LC_MSG_CHK_REQUEST_KER_HEARTBEAT,
    LC_MSG_CHK_REQUEST_DRV_HEARTBEAT,
    LC_MSG_CHK_REQUEST_MON_HEARTBEAT,
    LC_MSG_CHK_REQUEST_SNF_HEARTBEAT,
    LC_MSG_LCM_REPLY_CHK_HEARTBEAT,
    LC_MSG_KER_REPLY_CHK_HEARTBEAT,
    LC_MSG_DRV_REPLY_CHK_HEARTBEAT,
    LC_MSG_MON_REPLY_CHK_HEARTBEAT,
    LC_MSG_SNF_REPLY_CHK_HEARTBEAT,

    LC_MSG_MAX_NUM

};

#endif /* _MESSAGE_MSG_ENUM_H */
