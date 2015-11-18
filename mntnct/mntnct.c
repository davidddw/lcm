#include <curl/curl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mntnct_commands.h"

char* short_options = "";
struct option long_options[] = {
    { "minimal", 0, NULL, 'm' },
    { 0, 0, 0, 0 }
};

int main(int argc, char **argv)
{
    int c, i, j, n, ret;
    char *p;
    uint32_t opts = 0;
    argument *args;

    if (argc == 1) {
        fprintf(stderr, "Usage: %s <command> [command specific arguments]\n\n",
                argv[0]);
        fprintf(stderr, "A full list of commands can be obtained by running\n");
        fprintf(stderr, "\t%s help\n", argv[0]);

        return -1;
    }

    while ((c = getopt_long(argc, argv, short_options, long_options, 0)) != -1) {
        switch (c) {
            case 'm':
                opts |= MNTNCT_OPTION_MINIMAL;
                break;
            default:
                break;
        }
    }

    if (optind >= argc) {
        return -1;
    }

    if (strcmp(argv[optind], "help") == 0) {
        if (argc > optind + 1) {
            return help_func_(argv[optind + 1], opts);
        } else {
            return help_func_(0, opts);
        }
    }

    if (curl_global_init(CURL_GLOBAL_ALL)) {
        fprintf(stderr, "cURL init error\n");
        return -1;
    }

    args = (argument *)calloc(argc, sizeof(argument));
    if (!args) {
        fprintf(stderr, "Call calloc() error\n");
        return -1;
    }
    j = 0;

    for (i = optind + 1; i < argc; ++i) {
        p = strchr(argv[i], '=');
        if (p) {
            n = p - argv[i];
            n = n < (MNTNCT_ARGNAME_LEN - 1) ?
                n : (MNTNCT_ARGNAME_LEN - 1);
            strncpy(args[j].key, argv[i], n);
            strncpy(args[j].value, p + 1, MNTNCT_ARG_LEN - 1);
        } else {
            strncpy(args[j].key, argv[i], MNTNCT_ARGNAME_LEN - 1);
        }
        ++j;
    }

    ret = call_func_(argv[optind], opts, args);

    free(args);
    curl_global_cleanup();

    return ret;
}
