#include <stdio.h>
#include <stdlib.h>
#include "cloudmessage.pb-c.h"

int main()
{
    Cloudmessage__VMAddResp msg = CLOUDMESSAGE__VMADD_RESP__INIT;
    void *buf;
    char *s;
    unsigned len, i;

    len = cloudmessage__vmadd_resp__get_packed_size(&msg);

    buf = malloc(len);
    cloudmessage__vmadd_resp__pack(&msg, buf);
    s = (char *)buf;

    printf("pack size %d\n", len);
    for (i = 0; i < len; ++i) {
        printf("%x ", s[i]);
    }
    putchar('\n');

    return 0;
}
