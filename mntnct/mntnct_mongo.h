#ifndef _MNTNCT_MONGO_H
#define _MNTNCT_MONGO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MONGO_BUFSIZE     512

#define PORT              "20011"
#define HOST              "localhost"

#define MONGOCMDPRE       "mongo -port " PORT \
                               " -host " HOST
#define EXEC              " --quiet --eval "
#define DB_CHARGE         " charge "
#define MONGOCMD          MONGOCMDPRE DB_CHARGE EXEC
#define MONOG_INSERT      "db.%s.insert({%s})"
#define MONOG_DELETE      "db.%s.remove({%s})"
#define MONOG_DELSEL      "db.%s.remove(db.%s.find({%s}).sort({%s}).toArray()[0])"

#define mongo_insert(table, value) \
    do { \
        char __cmd[MONGO_BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, MONGOCMD, MONGO_BUFSIZE - 1); \
        strncat(__cmd, "\"", MONGO_BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, MONGO_BUFSIZE - strlen(__cmd), MONOG_INSERT, table, value); \
        strncat(__cmd, "\"", MONGO_BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#define mongo_delete(table, cond) \
    do { \
        char __cmd[MONGO_BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, MONGOCMD, MONGO_BUFSIZE - 1); \
        strncat(__cmd, "\"", MONGO_BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, MONGO_BUFSIZE - strlen(__cmd), MONOG_DELETE, table, cond); \
        strncat(__cmd, "\"", MONGO_BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#define mongo_delete_select(table, cond, sort) \
    do { \
        char __cmd[MONGO_BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, MONGOCMD, MONGO_BUFSIZE - 1); \
        strncat(__cmd, "\"", MONGO_BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, MONGO_BUFSIZE - strlen(__cmd), MONOG_DELSEL, table, table, cond, sort); \
        strncat(__cmd, "\"", MONGO_BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#endif
