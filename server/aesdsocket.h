#include <stdlib.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "queue.h"


#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"
#define RECV_BUFFER_SIZE 128
#define DEBUG 0


#define CHECK(x) do { \
  int retval = (x); \
  if (retval != 0) { \
    syslog(LOG_ERR, "Runtime error: %s returned %d at %s:%d", #x, retval, __FILE__, __LINE__); \
    exit(EXIT_FAILURE); \
  } \
} while (0)


typedef struct slist_data_s slist_data_t;
struct slist_data_s {
    pthread_t thread_id;
    int completed;
    int fd;
    char client_ip[INET_ADDRSTRLEN];
    SLIST_ENTRY(slist_data_s) entries;
};

