/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Xiang Yang
 * Finish Date: 2013-12-02
 */

#ifndef _LC_HEADER_H
#define _LC_HEADER_H


#include "header.pb-c.h"

typedef struct header_s {
    int sender; /* HEADER__MODULE__xxx in include/message.pb-c.h */
    int type;   /* HEADER__TYPE__xxx in include/message.pb-c.h */
    int length; /* #bytes of payload */
    int seq;    /* optional */
} header_t;

int get_message_header_pb_len(void);
int fill_message_header(const header_t *hdr, void *buf);
int unpack_message_header(const uint8_t *buf, int buf_len, header_t *hdr);


#endif /* _LC_HEADER_H */
