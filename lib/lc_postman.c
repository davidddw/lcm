/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Xiang Yang
 * Finish Date: 2013-11-26
 */

#include "lc_postman.h"

int construct_send_request(
        send_request_t *send_request, void *buf, int buf_len)
{
    Postman__Message msg = POSTMAN__MESSAGE__INIT;
    Postman__SendRequest req = POSTMAN__SEND_REQUEST__INIT;

    Postman__Policy policy = POSTMAN__POLICY__INIT;
    Postman__MailIdentity id = POSTMAN__MAIL_IDENTITY__INIT;
    Postman__Mail mail = POSTMAN__MAIL__INIT;

    Postman__MailContent content = POSTMAN__MAIL_CONTENT__INIT;
    Postman__AlarmEvent alarm = POSTMAN__ALARM_EVENT__INIT;

    Postman__AlarmEvent *alarms[1] = {NULL};
    char *attachments[1] = {NULL};

    msg.send_request = &req;

    req.policy = &policy;
    req.id = &id;
    req.mail = &mail;

    if (send_request->policy.priority) {
        policy.has_priority = 1;
        policy.priority = send_request->policy.priority;
    }
    if (send_request->policy.aggregate_id) {
        policy.has_aggregate_id = 1;
        policy.aggregate_id = send_request->policy.aggregate_id;
    }
    if (send_request->policy.aggregate_window) {
        policy.has_aggregate_window = 1;
        policy.aggregate_window = send_request->policy.aggregate_window;
    }
    if (send_request->policy.min_event_interval) {
        policy.has_min_event_interval = 1;
        policy.min_event_interval = send_request->policy.min_event_interval;
    }
    if (send_request->policy.min_dest_interval) {
        policy.has_min_dest_interval = 1;
        policy.min_dest_interval = send_request->policy.min_dest_interval;
    }
    if (send_request->policy.issue_timestamp) {
        policy.has_issue_timestamp = 1;
        policy.issue_timestamp = send_request->policy.issue_timestamp;
    }

    if (send_request->id.event_type) {
        id.has_event_type = 1;
        id.event_type = send_request->id.event_type;
    }
    if (send_request->id.resource_type) {
        id.has_resource_type = 1;
        id.resource_type = send_request->id.resource_type;
    }
    if (send_request->id.resource_id) {
        id.has_resource_id = 1;
        id.resource_id = send_request->id.resource_id;
    }

    if (send_request->mail.from[0]) {
        mail.from = send_request->mail.from;
    }
    if (send_request->mail.to[0]) {
        mail.to = send_request->mail.to;
    }
    if (send_request->mail.cc[0]) {
        mail.cc = send_request->mail.cc;
    }
    if (send_request->mail.bcc[0]) {
        mail.bcc = send_request->mail.bcc;
    }
    if (send_request->mail.subject[0]) {
        mail.subject = send_request->mail.subject;
    }
    if (send_request->mail.customer_name[0]) {
        mail.customer_name = send_request->mail.customer_name;
    }

    mail.content = &content;
    if (send_request->mail.mail_type) {
        content.has_mail_type = 1;
        content.mail_type = send_request->mail.mail_type;
    }

    if (send_request->mail.resource_type[0]) {
        content.n_alarms = 1;
        alarms[0] = &alarm;
        content.alarms = alarms;
    }
    if (send_request->mail.resource_type[0]) {
        alarm.resource_type = send_request->mail.resource_type;
    }
    if (send_request->mail.resource_name[0]) {
        alarm.resource_name = send_request->mail.resource_name;
    }
    if (send_request->mail.alarm_begin) {
        alarm.has_alarm_begin = 1;
        alarm.alarm_begin = send_request->mail.alarm_begin;
    }
    if (send_request->mail.alarm_last) {
        alarm.has_alarm_last = 1;
        alarm.alarm_last = send_request->mail.alarm_last;
    }
    if (send_request->mail.alarm_message[0]) {
        alarm.alarm_message = send_request->mail.alarm_message;
    }
    if (send_request->mail.head_template[0]) {
        content.head_template = send_request->mail.head_template;
    }
    if (send_request->mail.tail_template[0]) {
        content.tail_template = send_request->mail.tail_template;
    }

    if (send_request->mail.attachment[0]) {
        content.n_attachments = 1;
        attachments[0] = send_request->mail.attachment;
        content.attachments = attachments;
    }

    postman__message__pack(&msg, buf);
    return postman__message__get_packed_size(&msg);
}

