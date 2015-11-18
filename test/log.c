#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/klog.h>

int main()
{

    openlog("mydaemon", LOG_NDELAY, LOG_DAEMON);
    syslog(LOG_INFO, "Connection from host %d", "xxxxxxxx");

    syslog(LOG_ALERT, "Database Error !");
    closelog();
}
