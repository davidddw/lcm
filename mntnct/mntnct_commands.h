#ifndef _MNTNCT_COMMANDS_H
#define _MNTNCT_COMMANDS_H

#define MNTNCT_OPTION_MINIMAL       0x1

#define MNTNCT_ARGNAME_LEN          64
#define MNTNCT_ARG_LEN              128
#define LCMD_SHM_ID                 "/usr/local/livecloud/conf/lcmconf"
#define LCMD_LOG_LEVEL_ID           "/usr/local/livecloud/conf/sql_init_cmd"
#define LC_VM_SYS_LEVEL_SHM_ID      "/tmp/.vm_shm_sys_level_key" /*added by kzs for system log level global variable*/
#define LC_LCPD_SYS_LEVEL_SHM_ID    "/usr/local/livecloud/conf/sql_init_cmd" /*added by kzs for system log level global variable*/
#define MNTNCT_VIF_DEVICE_TYPE_THIRDHW      3

typedef struct {
    char key[MNTNCT_ARGNAME_LEN];
    char value[MNTNCT_ARG_LEN];
} argument;

int help_func_(char *func, int opts);
int call_func_(char *func, int opts, argument *args);

#endif
