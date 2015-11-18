/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Xiang Yang
 * Finish Date: 2013-08-09
 *
 */


#ifndef __LCS_SOCKET_H__
#define __LCS_SOCKET_H__


int lcsnf_sock_init(void);
int lcsnf_sock_exit(void);
int hb_msg_process(int fd);
int hb_message_send(void);


#endif /* __LCS_SOCKET_H__ */
