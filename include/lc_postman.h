/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Xiang Yang
 * Finish Date: 2013-11-26
 */

#ifndef _LC_POSTMAN_H
#define _LC_POSTMAN_H


#include "postman.pb-c.h"

#define LC_EMAIL_ADDRESS_LEN           64
#define LC_EMAIL_SUBJECT_LEN           128
#define LC_EMAIL_CUSTOMER_NAME_LEN     64
#define LC_EMAIL_RESOURCE_TYPE_LEN     32
#define LC_EMAIL_RESOURCE_NAME_LEN     64
#define LC_EMAIL_ALARM_MESSAGE_LEN     256
#define LC_EMAIL_FILE_PATH_LEN         128
#define LC_EMAIL_TEMPLATE_NAME_LEN     128

typedef struct send_request_s {
    struct {
        Postman__Priority priority;
        Postman__AggregateId aggregate_id;
        int aggregate_window;
        int min_event_interval;
        int min_dest_interval;
        int issue_timestamp;
    } policy;
    struct {
        Postman__Event event_type;
        Postman__Resource resource_type;
        int resource_id;
    } id;
    struct {
        char from[LC_EMAIL_ADDRESS_LEN];
        char to[LC_EMAIL_ADDRESS_LEN];
        char cc[LC_EMAIL_ADDRESS_LEN];
        char bcc[LC_EMAIL_ADDRESS_LEN];
        char subject[LC_EMAIL_SUBJECT_LEN];
        char customer_name[LC_EMAIL_CUSTOMER_NAME_LEN];

        Postman__MailType mail_type;
        char resource_type[LC_EMAIL_RESOURCE_TYPE_LEN];
        char resource_name[LC_EMAIL_RESOURCE_NAME_LEN];
        int alarm_begin;
        int alarm_last;
        char alarm_message[LC_EMAIL_ALARM_MESSAGE_LEN];
        char head_template[LC_EMAIL_TEMPLATE_NAME_LEN];
        char tail_template[LC_EMAIL_TEMPLATE_NAME_LEN];
        char attachment[LC_EMAIL_FILE_PATH_LEN];
    } mail;
} send_request_t;

int construct_send_request(send_request_t *send_request,
        void *buf, int buf_len);


#endif /* _LC_POSTMAN_H */
