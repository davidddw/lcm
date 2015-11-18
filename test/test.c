#include <stdio.h>
#include <stdlib.h>
#include <string.h>



int lc_get_vnet_id_by_name (char *name)
{
    char id[6];
    char *p = name;

    strncpy(id, p + 4, 6);
    return atoi(id);
}

int lc_get_vl2_id_by_name (char *name)
{
    char id[6];
    char *p = name;

    strncpy(id, p + 15, 6);
    return atoi(id);
}

int lc_get_vm_id_by_name (char *name)
{
    char id[6];
    char *p = name;

    strncpy(id, p + 24, 6);
    return atoi(id);
}

int get_buffer(char *buf)
{
    char buffer[20];

    strcpy(buffer, "test");
    memcpy(buf, buffer, 20);
}
int main()
{
    int i = 0;
    char buf[20];

    get_buffer(buf);
    printf("xxx %s \n", buf);
    printf("xxx %d \n", lc_get_vnet_id_by_name("vnet000001_vl2000100_vm000010"));
    printf("xxx %d \n", lc_get_vl2_id_by_name("vnet000001_vl2000100_vm000010"));
    printf("xxx %d \n", lc_get_vm_id_by_name("vnet000001_vl2000100_vm000010"));
    return 0;
}
