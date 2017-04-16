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
static volatile int serverSocket;
static pthread_t threads[2];

void *write_to_server(void *arg);
void *read_from_server(void *arg);
void close_program(int signal);
void bootstrap();

static queue_t *job_todo;
static queue_t *job_tosend;
static volatile int job_finished, job_server_done;
static volatile double throttle_value;

pthread_mutex_t mutex;

/**
 * Clean up for client
 * Called by close_program upon SIGINT
 */
void close_client() {
  // Cancel the running threads
  pthread_cancel(threads[0]);
  pthread_cancel(threads[1]);

  // Any other cleanup code goes here!
//  fprintf(stderr, "shuting down the socket\n");
  shutdown(serverSocket, SHUT_RDWR);
  close(serverSocket);
  queue_destroy(job_todo);
  queue_destroy(job_tosend);
}

/**
 * Sets up a connection to a chatroom server and begins
 * reading and writing to the server.
 *
 * host     - Server to connect to.
 * port     - Port to connect to server on.
 * username - Name this user has chosen to connect to the chatroom with.
 */
void run_client(const char *host, const char *port) {
  int s;
  serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  struct addrinfo hints, *result;

  memset(&hints, 0, sizeof(struct addrinfo));

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  s = getaddrinfo(host, port, &hints, &result);
  if (s != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    exit(1);
  }

  int tmp = connect(serverSocket, result->ai_addr, result->ai_addrlen);
  if (tmp != 0) {
    perror(NULL);
    exit(1);
  }

  // initializatin
  int i;
  for (i=0; i<ARRAY_SIZE; i++) {
    job_array[i] = 1.111111;
  }
  job_todo = queue_create(JOB_NUM);
  job_tosend = queue_create(JOB_NUM);
  job_finished = 0;
  job_server_done = 0;
  pthread_mutex_init(&mutex, NULL);
  bootstrap();

  // ---
  pthread_mutex_lock(&mutex);
  pthread_create(threads, 0, write_to_server, NULL);
  pthread_create(threads+1, 0, read_from_server, NULL);
  pthread_mutex_unlock(&mutex);

  // perform computation locally
  pthread_mutex_lock(&mutex);
  while (job_finished < JOB_NUM) {
    pthread_mutex_unlock(&mutex);
    int jobID = (int) ((long) queue_pull(job_todo));
    if (jobID == -1) continue;
    compute_with_throttle(job_array, jobID);
    pthread_mutex_lock(&mutex);

    job_finished += 1;
    printf("Local computation: job_finished = %d\n", job_finished);
  }
  pthread_mutex_unlock(&mutex);
  printf("local computation has finished\n");
  queue_push(job_tosend, (void*) -1);

  // ---
  pthread_join(threads[0], NULL);
  printf("thread[0] is joined\n");
  pthread_join(threads[1], NULL);
  printf("job has finished --> successfully joined 2 threads\n");

  // save job_array to file
  FILE* fp;
  fp = fopen("mp4_output.txt", "w+");
  for(i=0; i<ARRAY_SIZE; i++) {
    fprintf(fp, "%f\n", job_array[i]);
  }

  freeaddrinfo(result);
}

void bootstrap() {
  int i;
  for (i=0; i<JOB_NUM/2; i++) {
    queue_push(job_todo, (void*) ((long) i));
    queue_push(job_tosend, (void*) ((long) (i+JOB_NUM/2)));
  }
}


/**
 * Reads bytes from user and writes them to server
 *
 * arg - void* casting of char* that is the username of client
 */
void *write_to_server(void *arg) {
  char *buffer = NULL;
  double *msg = NULL;
  int retval = 1;

  // Setup thread cancellation handlers
  // Read up on pthread_cancel, thread cancellation states, pthread_cleanup_push
  // for more!

  while (1) {
    printf("writter enters a NEW loop\n");
    pthread_mutex_lock(&mutex);
    if (job_finished == JOB_NUM) {
      printf("job has finished --> exiting writter\n");
      pthread_mutex_unlock(&mutex);
      break;
    }
    pthread_mutex_unlock(&mutex);
    int jobID = (int) ((long) queue_pull(job_tosend));
    if (jobID < 0) {
      write_jobid(-1, serverSocket);
      continue;
    }
    msg = create_message(jobID, job_array);
    printf("Message sent: [%d] (%lu) %f\n", jobID, strlen((char*) msg), msg[0]);
    retval = write_jobid(jobID, serverSocket);
    if (retval > 0)
      retval = write_all_to_socket(serverSocket, msg, JOB_SIZE*sizeof(double));

    msg = NULL;
  }
  printf("exit writter\n");
  return 0;
}



/**
 * Reads bytes from the server and prints them to the user.
 *
 * arg - void* requriment for pthread_create function
 */
void *read_from_server(void *arg) {
  // Silence the unused parameter warning
  (void)arg;
  int retval = 1;
  double *buffer = NULL;
  while (1) {
    pthread_mutex_lock(&mutex);
    if (job_finished == JOB_NUM) {
      printf("job has finished --> exiting reader\n");
      queue_push(job_todo, (void*) -1);
      pthread_mutex_unlock(&mutex);
      break;
    }
    pthread_mutex_unlock(&mutex);

    retval = get_jobid(serverSocket);
    int jobID = retval;
/*
    if (retval > -1) {
      buffer = calloc(1, sizeof(double)*JOB_SIZE);
      retval = read_all_from_socket(serverSocket, buffer, retval);
    }

    if (retval > 0) {
      printf("Message received: %s\n", buffer);
    }
*/
    if (jobID > -1 && jobID < JOB_NUM) {
      buffer = calloc(1, sizeof(double)*JOB_SIZE);
      retval = read_all_from_socket(serverSocket, buffer, sizeof(double) * JOB_SIZE);
      printf("Message received: [%d] (%lu) %f\n", jobID, strlen((char*) buffer), buffer[0]);

      storeLocal(jobID, buffer);
      pthread_mutex_lock(&mutex);
      job_finished += 1;
      job_server_done += 1;
      printf("job_finished: %d, job_server_done: %d\n", job_finished, job_server_done);
      pthread_mutex_unlock(&mutex);
    }
    else if (jobID >= JOB_NUM) {
      queue_push(job_todo, (void*) ((long) (jobID - JOB_NUM)));
    }

    free(buffer);
    buffer = NULL;
  }

  printf("exit reader\n");
  return 0;
}

/**
 * Signal handler used to close this client program.
 */
void close_program(int signal) {
  if (signal == SIGINT) {
    close_client();
  }
}

int main(int argc, char **argv) {

  if (argc < 4 || argc > 5) {
    fprintf(stderr, "Usage: %s <address> <port> <throttle>\n",
            argv[0]);
    exit(1);
  }

  // Setup signal handler
  signal(SIGINT, close_program);
  sscanf(argv[3], "%lf", &throttle_value);
  run_client(argv[1], argv[2]);

  return 0;
}
