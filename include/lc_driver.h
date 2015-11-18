#include "lc_global.h"


lc_drv_sock_t lc_sock;

#define MAX_FILE_NAME_LEN            200

struct sockaddr_un sa_drv_rcv;
struct sockaddr_un sa_drv_snd_ker;

typedef struct lc_opt_ {
    u_int32_t lc_flag;
    char      config_file[MAX_FILE_NAME_LEN];
} lc_opt_t;

lc_opt_t lc_opt;
