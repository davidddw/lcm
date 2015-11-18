#ifndef _LC_DB_LOG_H
#define _LC_DB_LOG_H

int lc_vmdriver_db_log(char *action, char *loginfo, int level);
int lc_monitor_db_log(char *action, char *loginfo, int level);
int lc_rm_db_syslog(char *action, char* otype, int oid, char* oname,
                    char *loginfo, int level);
int lc_vmdriver_db_syslog(char *action, char* otype, int oid, char* oname,
                          char *loginfo, int level);
int lc_monitor_db_syslog(char *action, char* otype, int oid, char* oname,
                         char *loginfo, int level);
int lc_lcpd_db_syslog(char *action, char* otype, int oid, char* oname,
                      char *loginfo, int level);
#endif
