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
void* state_manager(void *p);

static volatile int sessionEnd;
static volatile int serverSocket;

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
  //printf("job array address %p\n", job_array);
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

    pthread_t state_thread;
    pthread_create(&state_thread, NULL, state_manager, NULL);

    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, update_throttle, NULL);

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

  while (sessionEnd == 0) {
    int jobID = (int)(long) queue_pull(job_tosend);

    //return result back
    retval = transfer(jobID, clientFd);
    if(retval)
      printf("transfer job error jobID%d\n", jobID);

    //finished job
    if(jobID == -1) {
      sleep(2);
      printf("Job finished. GoodBye!\n");
      kill(getpid(), SIGINT);
      continue;
    }
  }

  return NULL;
}

void transfer_handler() {
		double *buffer = NULL;
    int retval = get_jobid(clientFd);
    printf("receiving job_id: %d, \n", retval);
    if(retval == -1) {
      //signal to finished job
      queue_push(job_tosend, (void*) (long) -1);
      return;
    }

    //printf("Receiving JobID: %d\n", retval);
    int jobID = retval;

    if (retval > -1) {
      buffer = calloc(1, sizeof(double)*JOB_SIZE);
			memset((void*)buffer, 0, sizeof(double)*JOB_SIZE);
      retval = read_all_from_socket(clientFd, (char*)buffer, sizeof(double) * JOB_SIZE);
      //printf("retval is %d\n", retval); 
      printf("Receiving First Element: %f; Length is %lu\n", buffer[0], strlen((char*)buffer));
    }

    if (jobID > -1) {
      if(jobID > JOB_NUM)
        jobID -= JOB_NUM;
      printf("adding jobid %d\n", jobID);
      storeLocal(jobID, buffer);
      queue_push(job_todo, (void*)(long)jobID);
    }

    free(buffer);
    buffer = NULL;  
}

void *process_client(void *p) {
  pthread_detach(pthread_self());
  int retval = 1;

  //while (retval > -1 && sessionEnd == 0) {
  while( sessionEnd == 0 ) {
    int msg_type = get_msg_type(clientFd);
    //printf("receiving - msg_type: %d\n", msg_type);
    if(msg_type==0) {
      transfer_handler();
    } else if(msg_type==-1) {
      state_handle(clientFd);
    } else {
      printf("Unknown Message Type: %d\n", msg_type);
      return NULL;
    }
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
    //printf("Computing for %d\n", jobID);

    compute_with_throttle(job_array, jobID);
    queue_push(job_tosend, (void*)(long)jobID);
  }
  return NULL;
}

void* state_manager(void *p) {
  pthread_detach(pthread_self());
  printf("State Manager Started\n");

  while(1) {
    //printf("sending state\n");
    send_state(job_todo->size, throttle, monitor_utilization, clientFd);
    sleep(INFO_PERIOD);
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

