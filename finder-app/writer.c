#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[]) {

    openlog("writer", LOG_CONS | LOG_PERROR, LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, "Incorrect number of arguments. Usage: writer  <file> <string>");
        return 1;
    }

    char *string = argv[2];
    char *file = argv[1];

    FILE *fp;
    fp = fopen(file, "w");
    if (fp == NULL) {
        syslog(LOG_ERR, "Error opening file %s: %s", file, strerror(errno));
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", string, file);
    fprintf(fp, "%s", string);
    fclose(fp);

    return 0;
}
