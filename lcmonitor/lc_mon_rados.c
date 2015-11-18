#include <stdio.h>
#include <stdlib.h>

#include "librados.h"
#include "lc_mon.h"
#include "talker.pb-c.h"

#ifndef SLIST_REMOVE_AFTER
#define SLIST_REMOVE_AFTER(elm, field) \
    do { \
        SLIST_NEXT(elm, field) = \
        SLIST_NEXT(SLIST_NEXT(elm, field), field); \
    } while (0)
#endif

#define IPV4_SIZE 16

struct host_counter {
    char host_address[IPV4_SIZE];
    int count;
    enum { TICK, TOCK } round;

    SLIST_ENTRY(host_counter) chain;
};
SLIST_HEAD(hc_head, host_counter);

struct osd_stat {
    int id;
    char host_address[IPV4_SIZE];
    enum { OSD_UP, OSD_DOWN } stat;
};

int is_osd_stats_ok(rados_t cluster)
{
    const char *cmds[] = { "{\"prefix\": \"osd stat\"}" };
    char *outbuf = 0;
    size_t outbuflen = 0;
    int err;
    int i;
    char *p_osd_count = 0, *p_osd_stats = 0;
    int osd_count, osd_stats;

    err = rados_mon_command(cluster, cmds, 1, 0, 0, &outbuf, &outbuflen, 0, 0);
    if (err < 0) {
        /* TODO: add log */
        return 0;
    }

    for (i = 0; i < outbuflen; ++i) {
        if (outbuf[i] == ':') {
            if (!p_osd_count) {
                p_osd_count = outbuf + i + 1;
                continue;
            } else {
                p_osd_stats = outbuf + i + 1;
                break;
            }
        }
    }

    osd_count = atoi(p_osd_count);
    osd_stats = atoi(p_osd_stats);

    rados_buffer_free(outbuf);

    if (osd_count == osd_stats) {
        return 1;
    } else {
        return 0;
    }
}

int fill_osd_stat_from_string(struct osd_stat *data, char *line)
{
    /* line is like:
     *
     * osd.3 up   in  weight 1 up_from 5671 up_thru 5870 down_at 5662
     * last_clean_interval [5630,5661)
     * 192.168.122.1:6800/6404 192.168.122.1:6801/6404
     * 192.168.122.1:6802/6404 192.168.122.1:6803/6404
     * exists,up a450da4a-8c15-4802-bd4e-6a25f05c76d6
     *
     * update:
     *     This line may have an extra 'lost_at' column, so the host address
     *     is not always at column 13. Will try to find the address around
     *     character ':'.
     */
#define COL_STAT         1
#define COL_HOST_ADDRESS 13
    char *p = line;
    char *q;
    char *saveptr = 0;
    int col;
    int is_host_address_column;

    /* get osd id first */
    while (*p) {
        if (*p == '.') {
            break;
        }
        ++p;
    }
    ++p;
    if (!p) {
        return -1;
    }
    data->id = atoi(p);

    /* get osd stat and host address */
    col = 0;
    p = strtok_r(line, " ", &saveptr);
    while (p) {
        if (col == COL_STAT) {
            if (strncmp(p, "up", strlen("up")) == 0) {
                data->stat = OSD_UP;
            } else if (strncmp(p, "down", strlen("down")) == 0) {
                data->stat = OSD_DOWN;
            } else {
                return -1;
            }
        } else {
            q = p;
            is_host_address_column = 0;
            while (*q) {
                if (*q == ':') {
                    *q = 0;
                    is_host_address_column = 1;
                    break;
                }
                ++q;
            }
            if (is_host_address_column) {
                if (strlen(p) == 0) {
                    return -1;
                } else {
                    strncpy(data->host_address, p, sizeof data->host_address);
                }
                break;
            }
        }
        p = strtok_r(0, " ", &saveptr);
        ++col;
    }
    return 0;
}

int compare_osd_stats(const void *a, const void *b)
{
    struct osd_stat *s1 = (struct osd_stat *) a;
    struct osd_stat *s2 = (struct osd_stat *) b;

    return strcmp(s1->host_address, s2->host_address);
}

int is_ping_check_ok(const char *host_address)
{
    int  cnt = 0;
    char cmd[LC_EXEC_CMD_LEN];
    FILE *stream = 0;

    memset(cmd, 0, LC_EXEC_CMD_LEN);
    sprintf(cmd,
            "ping -w 1 -i 0.001 -c 100 %s -q | grep -Eo \"[0-9]+ received\"",
            host_address);
    stream = popen(cmd, "r");
    if (!stream) {
        LCMON_LOG(LOG_ERR, "stream is NULL.\n");
        return 0;
    }
    fscanf(stream, "%d received", &cnt);
    pclose(stream);

    if (cnt > 0) {
        return 1;
    } else {
        return 0;
    }
}

int get_failed_host_list_from_osd_stats(rados_t cluster, char **host_list,
                                        size_t *host_count)
{
    const char *cmds[] = { "{\"prefix\": \"osd stat\"}",
                           "{\"prefix\": \"osd dump\"}" };
    char *outbuf = 0;
    size_t outbuflen = 0;
    int err, ret = 0;
    char *p = 0;
    int osd_count = 0;
    struct osd_stat *osd_stats = 0;
    int i, j;
    char *saveptr = 0;
    int reach_osd_count = 0;
    char last_host[IPV4_SIZE];
    int last_host_up;

    err = rados_mon_command(cluster, cmds, 1, 0, 0,
            &outbuf, &outbuflen, 0, 0);
    if (err < 0) {
        /* TODO: add log */
        return -1;
    }

    for (i = 0; i < outbuflen; ++i) {
        if (outbuf[i] == ':') {
            osd_count = atoi(outbuf + i + 1);
            break;
        }
    }
    if (osd_count <= 0) {
        /* TODO: add log */
        ret = -1;
        goto finish;
    }
    osd_stats = calloc(osd_count, sizeof(struct osd_stat));
    if (!osd_stats) {
        /* TODO: add log */
        ret = -1;
        goto finish;
    }

    rados_buffer_free(outbuf);
    err = rados_mon_command(cluster, cmds + 1, 1, 0, 0,
            &outbuf, &outbuflen, 0, 0);
    if (err < 0) {
        /* TODO: add log */
        ret = -1;
        goto finish;
    }

    p = strtok_r(outbuf, "\n", &saveptr);
    i = 0;
    while (p && i < osd_count) {
        if (reach_osd_count == 0 &&
                    strncmp(p, "max_osd", strlen("max_osd")) == 0) {

            reach_osd_count = 1;
        } else if (reach_osd_count && osd_count > 0) {
            if (fill_osd_stat_from_string(osd_stats + i, p)) {
                /* TODO: add log */
                ret = -1;
                goto finish;
            }
            ++i;
        }
        p = strtok_r(0, "\n", &saveptr);
    }

    qsort(osd_stats, osd_count, sizeof(struct osd_stat), compare_osd_stats);

    last_host[0] = 0;
    last_host_up = 1;
    *host_count = 0;
    for (i = 0; i < osd_count; ++i) {
        if (strcmp(osd_stats[i].host_address, last_host)) {
            if (!last_host_up && !is_ping_check_ok(last_host)) {
                ++*host_count;
            }
            strncpy(last_host, osd_stats[i].host_address, sizeof last_host);
            if (osd_stats[i].stat == OSD_UP) {
                last_host_up = 1;
            } else {
                last_host_up = 0;
            }
        } else if (osd_stats[i].stat == OSD_UP) {
            last_host_up = 1;
        }
    }
    if (!last_host_up && !is_ping_check_ok(last_host)) {
        ++*host_count;
    }

    *host_list = calloc(*host_count, IPV4_SIZE);
    if (!*host_list) {
        /* TODO: add log */
        ret = -1;
        goto finish;
    }

    last_host[0] = 0;
    last_host_up = 1;
    j = 0;
    for (i = 0; i < osd_count; ++i) {
        if (strcmp(osd_stats[i].host_address, last_host)) {
            if (!last_host_up && !is_ping_check_ok(last_host)) {
                strncpy(*host_list + j * IPV4_SIZE, last_host, IPV4_SIZE - 1);
                ++j;
            }
            strncpy(last_host, osd_stats[i].host_address, sizeof last_host);
            if (osd_stats[i].stat == OSD_UP) {
                last_host_up = 1;
            } else {
                last_host_up = 0;
            }
        } else if (osd_stats[i].stat == OSD_UP) {
            last_host_up = 1;
        }
    }
    if (!last_host_up && !is_ping_check_ok(last_host)) {
        strncpy(*host_list + j * IPV4_SIZE, last_host, IPV4_SIZE - 1);
    }

finish:

    rados_buffer_free(outbuf);
    if (osd_stats) {
        free(osd_stats);
    }
    return ret;
}

int free_host_list(char *host_list)
{
    if (host_list) {
        free(host_list);
    }
    return 0;
}

int send_notify_message_for_host_list(struct hc_head *head)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__NotifyProactive *notify = 0;
    Talker__NotifyBundleProactive notify_bundle = \
            TALKER__NOTIFY_BUNDLE_PROACTIVE__INIT;
    int hdr_len = get_message_header_pb_len(), msg_len = 0;
    char buf[MAX_MSG_BUFFER];
    int ret = 0, i;
    struct host_counter *hc = 0;

    notify_bundle.n_bundle = 1;
    notify_bundle.bundle = calloc(1, sizeof(Talker__NotifyProactive *));
    if (!notify_bundle.bundle) {
        ret = -1;
        goto finish;
    }
    notify = calloc(1, sizeof(Talker__NotifyProactive));
    if (!notify) {
        ret = -1;
        goto finish;
    }
    talker__notify_proactive__init(notify);
    notify_bundle.bundle[0] = notify;

    notify->type = TALKER__RESOURCE_TYPE__HOST_HA_RESOURCE;
    notify->has_type = 1;

    notify->state = TALKER__RESOURCE_STATE__RESOURCE_DOWN;
    notify->has_state = 1;

    notify->n_ips = 0;
    SLIST_FOREACH(hc, head, chain) {
        ++notify->n_ips;
    }
    notify->ips = calloc(notify->n_ips, sizeof(char *));
    if (!notify->ips) {
        ret = -1;
        goto finish;
    }
    i = 0;
    SLIST_FOREACH(hc, head, chain) {
        notify->ips[i] = hc->host_address;
        ++i;
    }

    msg.notify_bundle = &notify_bundle;
    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__LCMOND;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    fill_message_header(&hdr, buf);

    lc_bus_lcmond_publish_unicast(buf, (hdr_len + msg_len), LC_BUS_QUEUE_TALKER);

finish:
    if (notify->ips) {
        free(notify->ips);
    }
    if (notify) {
        free(notify);
    }
    if (notify_bundle.bundle) {
        free(notify_bundle.bundle);
    }

    return ret;
}

void *lc_rados_monitor_thread()
{
    rados_t cluster;
    int err, i;
    char *host_list = 0;
    size_t host_count;
    struct hc_head host_errors;
    struct hc_head failed_hosts;
    struct host_counter *hc, *last_hc, *hc_to_be_free;
    enum { TICK, TOCK } this_round = TICK;

    LCMON_LOG(LOG_INFO, "RADOS monitor enabled");
    LCMON_LOG(LOG_DEBUG, "Cluster name: %s; User name: %s; Config file: %s",
              lcmon_config.ceph_cluster_name, lcmon_config.ceph_user_name,
              lcmon_config.ceph_conf_file)

    SLIST_INIT(&host_errors);
    SLIST_INIT(&failed_hosts);

#define RADOS_MON_INTERVAL 30
    while (1) {
        sleep(RADOS_MON_INTERVAL);
        LCMON_LOG(LOG_DEBUG, "Checking RADOS ...");

        /* check whether host counter being updated on this round or not */
        if (this_round == TICK) {
            this_round = TOCK;
        } else {
            this_round = TICK;
        }

        err = rados_create2(&cluster, lcmon_config.ceph_cluster_name,
                lcmon_config.ceph_user_name, 0);
        if (err < 0) {
            LCMON_LOG(LOG_ERR, "Couldn't create the cluster handle! %s",
                    strerror(-err));
            continue;
        }

        err = rados_conf_read_file(cluster, lcmon_config.ceph_conf_file);
        if (err < 0) {
            LCMON_LOG(LOG_ERR, "Couldn't read config file: [%s] %s",
                    lcmon_config.ceph_conf_file, strerror(-err));
            continue;
        }

        err = rados_connect(cluster);
        if (err < 0) {
            LCMON_LOG(LOG_ERR, "Couldn't connect to cluster! %s",
                    strerror(-err));
            continue;
        }

        if (is_osd_stats_ok(cluster)) {
            LCMON_LOG(LOG_INFO, "RADOS OSD stats OK");
            goto next_loop;
        }

        err = get_failed_host_list_from_osd_stats(
                cluster, &host_list, &host_count);
        if (err < 0) {
            LCMON_LOG(LOG_ERR, "Couldn't get failed host list");
            goto next_loop;
        }

        if (host_count == 0) {
            LCMON_LOG(LOG_INFO, "RADOS OSD down but host stats OK");
            /* move on to clean the list host_errors */
        } else {
            LCMON_LOG(LOG_ERR, "RADOS host found down");
        }

        /* update failed hosts */
        for (i = 0; i < host_count; ++i) {
            SLIST_FOREACH(hc, &host_errors, chain) {
                if (strncmp(hc->host_address, host_list + i * IPV4_SIZE,
                            IPV4_SIZE) == 0) {

                    ++hc->count;
                    hc->round = this_round;
                    break;
                }
            }
            if (hc) {
                /* updated */
                continue;
            } else {
                /* new entry */
                hc = calloc(1, sizeof(struct host_counter));
                if (!hc) {
                    LCMON_LOG(LOG_ERR, "calloc() failed");
                }
                strncpy(hc->host_address, host_list + i * IPV4_SIZE,
                        IPV4_SIZE - 1);
                hc->count = 1;
                hc->round = this_round;
                SLIST_INSERT_HEAD(&host_errors, hc, chain);
            }
        }

        /* find failed hosts and remove not updated hosts */
        last_hc = 0;
        hc_to_be_free = 0;
        SLIST_FOREACH(hc, &host_errors, chain) {
            if (hc_to_be_free) {
                free(hc_to_be_free);
                hc_to_be_free = 0;
            }
            if (hc->count >= lcmon_config.ceph_switch_threshold) {
                /* move into failed host list */
                SLIST_INSERT_HEAD(&failed_hosts, hc, chain);
                if (last_hc) {
                    SLIST_REMOVE_AFTER(last_hc, chain);
                } else {
                    SLIST_REMOVE_HEAD(&host_errors, chain);
                }
            } else if (hc->round != this_round) {
                /* hosts not failed on this round */
                if (last_hc) {
                    SLIST_REMOVE_AFTER(last_hc, chain);
                } else {
                    SLIST_REMOVE_HEAD(&host_errors, chain);
                }
                /*
                 * we can't free hc here since we need the value of
                 * SLIST_NEXT(hc, chain) for next loop
                 */
                hc_to_be_free = hc;
            }
            last_hc = hc;
        }

        LCMON_LOG(LOG_DEBUG, "Error hosts");
        SLIST_FOREACH(hc, &host_errors, chain) {
            LCMON_LOG(LOG_DEBUG, "%s: %d", hc->host_address, hc->count);
        }
        LCMON_LOG(LOG_DEBUG, "Failed hosts");
        SLIST_FOREACH(hc, &failed_hosts, chain) {
            LCMON_LOG(LOG_DEBUG, "%s: %d", hc->host_address, hc->count);
        }

        if (!SLIST_EMPTY(&failed_hosts)) {
            err = send_notify_message_for_host_list(&failed_hosts);
            if (err < 0) {
                LCMON_LOG(LOG_ERR, "Couldn't send notify message!");
            }
        }

        while (!SLIST_EMPTY(&failed_hosts)) {
            hc = SLIST_FIRST(&failed_hosts);
            SLIST_REMOVE_HEAD(&failed_hosts, chain);
            if (hc) {
                free(hc);
            }
        }

next_loop:
        if (host_list) {
            free_host_list(host_list);
            host_list = 0;
        }
        rados_shutdown(cluster);
    }
}

#if 0
int main(int argc, char **argv)
{
        rados_t cluster;
        char cluster_name[] = "ceph";
        char user_name[] = "client.admin";
        uint64_t flags = 0;
        const char **p = (const char **) argv;
        char *host_list;
        size_t host_count;
        int i;

        int err;
        err = rados_create2(&cluster, cluster_name, user_name, flags);

        if (err < 0) {
                fprintf(stderr, "%s: Couldn't create the cluster handle! %s\n",
                        argv[0], strerror(-err));
                exit(EXIT_FAILURE);
        } else {
                printf("\nCreated a cluster handle.\n");
        }


        /* Read a Ceph configuration file to configure the cluster handle. */
        err = rados_conf_read_file(cluster, "/etc/ceph/ceph.conf");
        if (err < 0) {
                fprintf(stderr, "%s: cannot read config file: %s\n", argv[0],
                        strerror(-err));
                exit(EXIT_FAILURE);
        } else {
                printf("\nRead the config file.\n");
        }

        /* Read command line arguments */
        err = rados_conf_parse_argv(cluster, argc, p);
        if (err < 0) {
                fprintf(stderr, "%s: cannot parse command line arguments: %s\n",
                        argv[0], strerror(-err));
                exit(EXIT_FAILURE);
        } else {
                printf("\nRead the command line arguments.\n");
        }

        /* Connect to the cluster */
        err = rados_connect(cluster);
        if (err < 0) {
                fprintf(stderr, "%s: cannot connect to cluster: %s\n", argv[0],
                        strerror(-err));
                exit(EXIT_FAILURE);
        } else {
                printf("\nConnected to the cluster.\n");
        }

        printf("\nis_osd_stats_ok = %d\n", is_osd_stats_ok(cluster));
        get_failed_host_list_from_osd_stats(cluster, &host_list, &host_count);
        for (i = 0; i < host_count; ++i) {
            printf("%d: %s\n", i, host_list + i * IPV4_SIZE);
        }

        rados_shutdown(cluster);

        return 0;
}
#endif
