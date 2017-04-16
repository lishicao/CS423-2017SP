#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"
#include "queue.h"

void *process_client(void *p);
void *write_to_clients(void *p);
void *compute_worker(void *p);

static volatile int sessionEnd;
static volatile int serverSocket;

static queue_t *job_todo;
static queue_t *job_tosend;

static volatile int clientFd = -1; // only accept 1 client

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void close_server() {
  // Use a signal handler to call this function and close the server
  // How to you let clients stop waiting for the server?
  sessionEnd = 1;
  if (clientFd != -1) {
    shutdown(clientFd, SHUT_RDWR);
    close(clientFd);
  }
  shutdown(serverSocket, SHUT_RDWR);
  close(serverSocket);
  queue_destroy(job_todo);
  queue_destroy(job_tosend);

}

void run_server(char *port) {
  serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  struct addrinfo hints, *result;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int s = getaddrinfo(NULL, port, &hints, &result);
  if (s != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    exit(1);
  }

  if (bind(serverSocket, result->ai_addr, result->ai_addrlen) != 0) {
    perror("bind():");
    exit(1);
  }

  if (listen(serverSocket, 8) != 0) {
    perror("listen():");
    exit(1);
  }
  
  // initializatin
  int i;
  for (i=0; i<ARRAY_SIZE; i++) {
    job_array[i] = 1.111111;
  }
  job_todo = queue_create(JOB_NUM);
  job_tosend = queue_create(JOB_NUM);


  while (sessionEnd == 0) {
    // Can now start accepting and processing client connections
    printf("Waiting for connection...\n");

    // I wonder what are the structs for :)
    struct sockaddr clientAddr;
    socklen_t clientAddrlen = sizeof(struct sockaddr);
    memset(&clientAddr, 0, sizeof(struct sockaddr));

    int tmp_clientFd = accept(serverSocket, &clientAddr, &clientAddrlen);
    pthread_mutex_lock(&mutex);
    if (clientFd == -1) {
      // Printing out IP address of newly joined clients
      char clientIp[INET_ADDRSTRLEN];
      if (inet_ntop(AF_INET, &clientAddr, clientIp, clientAddrlen) != 0) {
        printf("Client joined on %s\n", clientIp);
      }
      
      clientFd = tmp_clientFd;
      pthread_mutex_unlock(&mutex);
    }
    else {
      //printf("New client is connected but ignored\n");
      pthread_mutex_unlock(&mutex);
      continue;
    }
    // Launching a new thread to serve the client
    pthread_t thread;
    int retval = pthread_create(&thread, NULL, process_client, NULL);

    pthread_t compute_thread;
    pthread_create(&compute_thread, NULL, compute_worker, NULL);

    pthread_t send_thread;
    pthread_create(&send_thread, NULL, write_to_clients, NULL);

    if (retval != 0) {
      perror("pthread_create():");
      exit(1);
    }
  }
  // Be sure to free the address info here
  freeaddrinfo(result);
}

void *write_to_clients(void *p) {
  (void) p;
  int retval = 1;
  double *msg = NULL;

  while (sessionEnd == 0) {
    int jobID = (int)(long) queue_pull(job_tosend);
    if(jobID == -1) {
      write_jobid(jobID, clientFd);
      // make it ready for new jobs
      //clientFd = -1;
      printf("Job finished. GoodBye!\n");
      sleep(2);
      //raise(SIGINT);
      kill(getpid(), SIGINT); 
      continue;
    }
    msg = create_message(jobID, job_array);
    printf("Message sent: [%d] %f\n", jobID, msg[0]);
    retval = write_jobid(jobID, clientFd);
    if (retval > 0)
      retval = write_all_to_socket(clientFd, msg, JOB_SIZE*sizeof(double));

    msg = NULL;
  }

  return NULL;
}

void *process_client(void *p) {
  pthread_detach(pthread_self());
  int retval = 1;
  double *buffer = NULL;

  //while (retval > -1 && sessionEnd == 0) {
  while( sessionEnd == 0 ) {
    retval = get_jobid(clientFd);
    if(retval == -1) {
      //signal to finished job
      queue_push(job_tosend, (void*) (long) -1);
      continue;
    }

    //printf("Receiving JobID: %d\n", retval);
    int jobID = retval;

    if (retval > -1) {
      buffer = calloc(1, sizeof(double)*JOB_SIZE);
      retval = read_all_from_socket(clientFd, buffer, sizeof(double) * JOB_SIZE);
      printf("Receiving First Element: %f; Length is %lu\n", buffer[0], strlen((char*)buffer));
    }

    if (jobID > -1) {
      storeLocal(jobID, buffer);
      queue_push(job_todo, (void*)(long)jobID);
    }

    free(buffer);
    buffer = NULL;
  }

  close(clientFd);

  pthread_mutex_lock(&mutex);
  clientFd = -1;
  pthread_mutex_unlock(&mutex);

  return NULL;
}

void *compute_worker(void *p) {
  (void) p;
  pthread_detach(pthread_self());
  int jobID;
  while( (jobID = (int)(long)queue_pull(job_todo)) > -1) {
    printf("Computing for %d\n", jobID);

    compute_with_throttle(job_array, jobID);
    queue_push(job_tosend, (void*)(long)jobID);
  }
  return NULL;
}

int main(int argc, char **argv) {

  if (argc != 2) {
    fprintf(stderr, "./server <port>\n");
    return -1;
  }

  signal(SIGINT, close_server);
  run_server(argv[1]);

  return 0;
}

