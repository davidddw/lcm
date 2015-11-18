#ifndef _MNTNCT_SOCKET_H
#define _MNTNCT_SOCKET_H

#include "message/msg.h"

int send_message(lc_mbuf_t* msg);
int recv_message(uint8_t **body, size_t *len);
void free_message(void *body);

#endif
