#include "message/msg.h"

int path[][MAX_PATH_LEN] = {
                              { MSG_MG_START, MSG_MG_END },
                              { MSG_MG_START, MSG_MG_UI2DRV, MSG_MG_END },
                              { MSG_MG_START, MSG_MG_UI2DRV, MSG_MG_DRV2KER, MSG_MG_END },
                              { MSG_MG_START, MSG_MG_UI2APP, MSG_MG_END },
                              { MSG_MG_START, MSG_MG_UI2KER, MSG_MG_END },
                              { MSG_MG_START, MSG_MG_UI2KER, MSG_MG_KER2DRV, MSG_MG_END },
                              { MSG_MG_START, MSG_MG_UI2MON, MSG_MG_MON2DRV, MSG_MG_DRV2KER, MSG_MG_END },
                              { MSG_MG_START, MSG_MG_UI2KER, MSG_MG_KER2DRV, MSG_MG_DRV2KER, MSG_MG_END },
                              { MSG_MG_START, MSG_MG_UI2VWA, MSG_MG_VWA2KER, MSG_MG_END },
                              { MSG_MG_START, MSG_MG_END }
                           };

static int get_type_id(int type)
{
    switch (type) {
    case LC_MSG_HOST_ADD:
    case LC_MSG_HOST_DEL:
    case LC_MSG_HOST_CHG:
    case LC_MSG_VM_SNAPSHOT_CREATE:
    case LC_MSG_VM_SNAPSHOT_DELETE:
    case LC_MSG_VM_SNAPSHOT_RECOVERY:
    case LC_MSG_VM_BACKUP_CREATE:
    case LC_MSG_VM_BACKUP_DELETE:
    case LC_MSG_VM_RECOVERY:
    case LC_MSG_VM_VNC_CONNECT:
    case LC_MSG_VEDGE_VNC_CONNECT:
    case LC_MSG_VM_PLUG_DISK:
    case LC_MSG_VM_UNPLUG_DISK:
        return 1;
    case LC_MSG_VM_ADD:
    case LC_MSG_VM_CHG:
    case LC_MSG_VM_START:
    case LC_MSG_VM_RESUME:
    case LC_MSG_VCD_REPLY_VMD_VM_ADD:
    case LC_MSG_VEDGE_START:
    case LC_MSG_VEDGE_CHG:
    case LC_MSG_SR_JOIN:
    case LC_MSG_SR_EJECT:
    case LC_MSG_HOST_EJECT:
    case LC_MSG_VM_MIGRATE:
    case LC_MSG_VEDGE_MIGRATE:
    case LC_MSG_VINTERFACE_ATTACH:
    case LC_MSG_VINTERFACE_DETACH:
        return 2;
    case LC_MSG_VL2_ADD:
    case LC_MSG_VL2_DEL:
    case LC_MSG_VL2_CHANGE:
        return 3;
    case LC_MSG_VM_ISOLATE:
    case LC_MSG_VM_RELEASE:
    case LC_MSG_VEDGE_SWITCH:
    case LC_MSG_HOST_BOOT:
    case LC_MSG_VDEVICE_BOOT:
    case LC_MSG_VDEVICE_DOWN:
        return 4;
    case LC_MSG_HOST_JOIN:
    case LC_MSG_VM_DEL:
    case LC_MSG_VEDGE_DEL:
    case LC_MSG_VM_STOP:
    case LC_MSG_VM_SUSPEND:
    case LC_MSG_VEDGE_STOP:
        return 5;
    case LC_MSG_VEDGE_ADD:
        return 6;
    case LC_MSG_VM_REPLACE:
        return 7;
    case LC_MSG_VEDGE_ADD_SECU:
    case LC_MSG_VEDGE_DEL_SECU:
        return 8;
    default:
        return 0;
    }
}

int lc_msg_get_path(int type, int now)
{
    int *p = path[get_type_id(type)];
    int i = 0;

    for (i = 0; p[i] != MSG_MG_END; ++i) {
        if (p[i] == now) {
            return p[i + 1];
        }
    }
    return -1;
}

int lc_msg_get_rollback_type(int type)
{
    return type + 1;
}

int lc_msg_get_rollback_destination(int *dest, int type, int now)
{
    int *p = path[get_type_id(type)];
    int base = 0x4C430000 | ((now & 0xFF) << 8);
    int i;

    for (i = 1; p[i] != MSG_MG_END; ++i) {
        dest[i - 1] = base | ((p[i] & 0xFF00) >> 8);
        if (p[i] == now) {
            break;
        }
    }
    if (p[i] == MSG_MG_END) {
        return 0;
    } else {
        return i;
    }
}
