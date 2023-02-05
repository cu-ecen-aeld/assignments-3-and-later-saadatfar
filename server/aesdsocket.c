#include "aesdsocket.h"
#include "../aesd-char-driver/aesd_ioctl.h"

int sockfd;
int sig_close = 0;
#if !USE_AESD_CHAR_DEVICE
pthread_t time_thread_id = 0;
#endif
pthread_mutex_t mutex;
SLIST_HEAD(tlisthead, slist_data_s) head;

// Cleanup and close
void cleanup_close(int fd) {
  remove(DATA_FILE_PATH);
  CHECK(close(sockfd));
  #if !USE_AESD_CHAR_DEVICE
  CHECK(pthread_join(time_thread_id, NULL));
  #endif

  while (!SLIST_EMPTY(&head)) {
    slist_data_t *datap = SLIST_FIRST(&head);
    SLIST_REMOVE_HEAD(&head, entries);
    CHECK(pthread_join(datap->thread_id, NULL));
    CHECK(close(datap->fd));
    free(datap);
  }

  exit(fd);
}

// Signal handler
static void signal_handler() {
  syslog(LOG_INFO, "Caught signal, exiting");
  shutdown(sockfd,SHUT_RDWR);
  sig_close = 1;
}

// Create daemon
void create_daemon() {
  pid_t pid;
  pid = fork();
  if (pid == -1) {
    syslog(LOG_ERR, "Error on creating daemon: %s", strerror(errno));
    exit(EXIT_FAILURE);
  } else if (pid != 0) {
    exit(EXIT_SUCCESS);
  }
  if (setsid () == -1) {
    syslog(LOG_ERR, "Error on creating daemon: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }
  CHECK(chdir ("/"));
  open("/dev/null", O_RDWR);
  dup(0);
  dup(0);
}

// Called after packet completion to write to:
// - Write to the file
// - Send the file
void packet_complete(int sfd,char *buffer) {
  FILE *fp;
  struct aesd_seekto seekto;
  CHECK(pthread_mutex_lock(&mutex));
  fp = fopen(DATA_FILE_PATH, "a+");
  if (fp == NULL){
    syslog(LOG_ERR, "Error on opening file: %s", strerror(errno));
    cleanup_close(EXIT_FAILURE);
  }
  if (sscanf(buffer, "AESDCHAR_IOCSEEKTO:%d,%d", &seekto.write_cmd, &seekto.write_cmd_offset) == 2) {
    syslog(LOG_INFO, "Seeking to: %d,%d", seekto.write_cmd, seekto.write_cmd_offset);
    CHECK(ioctl(fileno(fp), AESDCHAR_IOCSEEKTO, &seekto));
  } else{
    if (fprintf(fp, "%s\n", buffer) < 0) {
      syslog(LOG_ERR, "Error on writing to file: %s", strerror(errno));
      fclose(fp);
      cleanup_close(EXIT_FAILURE);
    }
    rewind(fp);
    //CHECK(fseek(fp,0,SEEK_SET));
  }
  CHECK(pthread_mutex_unlock(&mutex));
  
  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  
  while ((read = getline(&line,&len,fp)) != -1) {
    if (send(sfd,line,read,MSG_NOSIGNAL) == -1) {
      syslog(LOG_ERR, "Error on sending: %s", strerror(errno));
      cleanup_close(EXIT_FAILURE);
    }
  }
  
  free(line);
  CHECK(fclose(fp));
}

// Socket thread
static void *start_socket_thread (void *arg)
{
    slist_data_t *params=(slist_data_t *)arg;
    char *buffer=malloc(sizeof(char));
    *buffer='\0';
    int size=1;
    while (1) {
      char recv_buff[RECV_BUFFER_SIZE + 1];
      int received = recv(params->fd, recv_buff, RECV_BUFFER_SIZE, 0);
      if (received == -1) {
        syslog(LOG_ERR, "Error on receiving: %s", strerror(errno));
        break;
      } else if (received == 0) {
        syslog(LOG_INFO, "Closed connection from %s",params->client_ip);
        break;
      }
      recv_buff[received]='\0';
      char *remain = recv_buff;
      char *token = strsep(&remain, "\n");
         if (remain  == NULL) { //Packet is not completed
        size += received;
        buffer = realloc(buffer, size*sizeof(char));
        //Append received chars to buffer
        strncat(buffer, token, received);
      } else { //Packet in completed
        size += strlen(token);
        buffer = realloc(buffer, size*sizeof(char));
        strcat(buffer, token);

        syslog(LOG_INFO, "Received packet: %s", buffer);

        //Write to file and send
        packet_complete(params->fd, buffer);

        //Store remaining characters in buffer (if any)
        size=strlen(remain)+1;
        buffer = realloc(buffer, size*sizeof(char));
        strcpy(buffer, remain);
      }
    }
    free(buffer);
    params->completed = 1;
    return arg;
}

#if !USE_AESD_CHAR_DEVICE
//Timer thread
static void *start_time_thread (void *arg) {
  FILE *fp;
  while (sig_close == 0) {
    for (int i=0; (sig_close == 0) && (i < 10); i++){
      sleep(1);
    }
    CHECK((pthread_mutex_lock(&mutex)));
    fp = fopen(DATA_FILE_PATH, "a+");
    if (fp == NULL){
      syslog(LOG_ERR, "Error on opening file: %s", strerror(errno));
      cleanup_close(EXIT_FAILURE);
    }

    time_t rawtime;
    struct tm *timeinfo;
    char buffer[80] = "";

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "timestamp:%a, %d %b %Y %T %z", timeinfo);

    if (fprintf(fp, "%s\n", buffer) < 0) {
      syslog(LOG_ERR, "Error on writing to file: %s", strerror(errno));
      fclose(fp);
      cleanup_close(EXIT_FAILURE);
    }

    CHECK(fclose(fp));
    CHECK(pthread_mutex_unlock(&mutex)!=0);
  }
  return NULL;
}
#endif

int main(int argc, char *argv[]) {
  //Open syslog file
  openlog("aesdsocket", LOG_PERROR * DEBUG, LOG_USER);
  
  // Register Signals
  struct sigaction saction;
  memset(&saction,0,sizeof(saction));
  saction.sa_handler=signal_handler;
  CHECK(sigaction(SIGTERM,&saction,NULL));
  CHECK(sigaction(SIGINT,&saction,NULL));

  CHECK(pthread_mutex_init(&mutex, NULL));
  
  //Open socket stream
  sockfd = socket(PF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    syslog(LOG_ERR, "Error on opening socket: %s", strerror(errno));
    exit(EXIT_FAILURE);	
  }

  int optval = 1;
  CHECK(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof(optval)));

  // Bind to port
  struct addrinfo hints;
  struct addrinfo *serverinfo;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  CHECK(getaddrinfo(NULL,"9000",&hints,&serverinfo));
  
  if (bind(sockfd,serverinfo->ai_addr,serverinfo->ai_addrlen) != 0){
    syslog(LOG_ERR, "Error on binding: %s", strerror(errno));
    freeaddrinfo(serverinfo);
    exit(EXIT_FAILURE);
  }

  freeaddrinfo(serverinfo);
  
  //Start listening
  syslog(LOG_INFO, "Bounded to port 9000");

  CHECK(listen(sockfd, 8));
  
  char opt;
  if ((opt = getopt(argc, argv, "d")) != -1) {
    if (opt != 'd') {
      syslog(LOG_ERR, "Invalid option - exiting");
      exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "Option -d detected");
    create_daemon();
  }

  SLIST_INIT(&head);

  #if !USE_AESD_CHAR_DEVICE
  // Create timer thread
  CHECK(pthread_create(&time_thread_id, NULL, start_time_thread, NULL));
  #endif

  // Wait, accpet and handle connections
  while (1) {
    slist_data_t *datap=NULL, *datap2=NULL;
    SLIST_FOREACH_SAFE(datap, &head, entries, datap2) {
        if (datap->completed == 1) {
          CHECK(pthread_join(datap->thread_id, NULL));
          //Cleanup
          SLIST_REMOVE(&head, datap, slist_data_s, entries);
          close(datap->fd);
          free(datap);
        }
    }

    // Accept connection
    struct sockaddr_in addr;
    socklen_t addr_len=sizeof(addr);
    memset(&addr, 0, sizeof(addr));
    int fd = accept(sockfd,(struct sockaddr*)  &addr, &addr_len);
    if (fd == -1) {
      if (errno != EINTR) {
        syslog(LOG_ERR, "Error on accepting: %s", strerror(errno));
        cleanup_close(EXIT_FAILURE);
      } else {
        cleanup_close(EXIT_SUCCESS);
      }
    }

    //Extract human readable IP
    char client_ip[INET_ADDRSTRLEN]="";
    inet_ntop(AF_INET, &(addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    syslog(LOG_INFO, "Accepted connection from %s",client_ip);

    datap = calloc(1,sizeof(slist_data_t));
    datap->fd = fd;
    strcpy(datap->client_ip, client_ip);

    SLIST_INSERT_HEAD(&head, datap, entries);
    CHECK(pthread_create(&(datap->thread_id), NULL, start_socket_thread, datap));
  }
}
