#include <stdio.h>
#include <stdlib.h>

#include "bism_ofdp.h"

#define RES_MAX 4096

int main(int argc, char *argv[])
{
    char res[RES_MAX];

    printf("bism_ofdp_flow_add\n");
    printf("%d\n", bism_ofdp_flow_add("tcp:127.0.0.1:10000", "priority=2,in_port=1,actions=drop"));
    getchar();

    printf("bism_ofdp_flow_dump\n");
    printf("%d\n", bism_ofdp_flow_dump(res, RES_MAX, "tcp:127.0.0.1:10000", "in_port=1"));
    printf("%s", res);
    getchar();

    printf("bism_ofdp_flow_del\n");
    printf("%d\n", bism_ofdp_flow_del("tcp:127.0.0.1:10000", "in_port=1"));
    getchar();

    printf("bism_ofdp_flow_dump\n");
    printf("%d\n", bism_ofdp_flow_dump(res, RES_MAX, "tcp:127.0.0.1:10000", NULL));
    printf("%s", res);
    getchar();

    return 0;
}

