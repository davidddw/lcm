/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Xiang Yang
 * Finish Date: 2013-12-02
 */

#include "lc_header.h"

int get_message_header_pb_len(void)
{
    const Header__Header hdr = HEADER__HEADER__INIT;
    const int hdr_len = header__header__get_packed_size(&hdr);

    return hdr_len;
}

int fill_message_header(const header_t *hdr, void *buf)
{
    int pb_hdr_len = get_message_header_pb_len();
    Header__Header pb_hdr = HEADER__HEADER__INIT;

    if (!hdr || !buf) {
        return -1;
    }

    pb_hdr.sender = hdr->sender;
    pb_hdr.type = hdr->type;
    pb_hdr.length = hdr->length;
    pb_hdr.seq = hdr->seq;

    header__header__pack(&pb_hdr, buf);
    return pb_hdr_len;
}

int unpack_message_header(const uint8_t *buf, int buf_len, header_t *hdr)
{
    Header__Header *pb_hdr = NULL;

    if (!buf || !hdr || buf_len < get_message_header_pb_len()) {
        return -1;
    }

    pb_hdr = header__header__unpack(NULL, buf_len, buf);
    if (!pb_hdr) {
        return -1;
    }

    hdr->sender = pb_hdr->sender;
    hdr->type = pb_hdr->type;
    hdr->length = pb_hdr->length;
    hdr->seq = pb_hdr->seq;

    header__header__free_unpacked(pb_hdr, NULL);
    return 0;
}

