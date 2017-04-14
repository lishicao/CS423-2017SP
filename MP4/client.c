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

static volatile int serverSocket;
static pthread_t threads[2];

void *write_to_server(void *arg);
void *read_from_server(void *arg);
void close_program(int signal);

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

  // ---
  pthread_create(threads, 0, write_to_server, NULL);
  pthread_create(threads+1, 0, read_from_server, NULL);

  // ---
  pthread_join(threads[0], NULL);
  pthread_join(threads[1], NULL);

  freeaddrinfo(result);
}

typedef struct _thread_cancel_args {
  char **buffer;
  char **msg;
} thread_cancel_args;

/**
 * Cleanup routine in case the thread gets cancelled
 * Ensure buffers are freed if they point to valid memory
 */
void thread_cancellation_handler(void *arg) {
  printf("Cancellation handler\n");
  thread_cancel_args *a = (thread_cancel_args *)arg;
  char **msg = a->msg;
  char **buffer = a->buffer;
  if (*buffer) {
    free(*buffer);
    *buffer = NULL;
  }
  if (msg && *msg) {
    free(*msg);
    *msg = NULL;
  }
}

/**
 * Reads bytes from user and writes them to server
 *
 * arg - void* casting of char* that is the username of client
 */
void *write_to_server(void *arg) {
  char *buffer = NULL;
  char *msg = NULL;
  ssize_t retval = 1;

  thread_cancel_args cancel_args;
  cancel_args.buffer = &buffer;
  cancel_args.msg = &msg;
  // Setup thread cancellation handlers
  // Read up on pthread_cancel, thread cancellation states, pthread_cleanup_push
  // for more!
  pthread_cleanup_push(thread_cancellation_handler, &cancel_args);

  while (retval > 0) {
    msg = create_message(1);
    printf("Message sent: %s\n", msg);
    size_t len = strlen(msg) + 1;
    retval = write_message_size(len, serverSocket);
    if (retval > 0)
      retval = write_all_to_socket(serverSocket, msg, len);

    free(msg);
    msg = NULL;

    retval = 0; // TODO
  }

  pthread_cleanup_pop(0);
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
  ssize_t retval = 1;
  char *buffer = NULL;
  thread_cancel_args cancellation_args;
  cancellation_args.buffer = &buffer;
  cancellation_args.msg = NULL;
  pthread_cleanup_push(thread_cancellation_handler, &cancellation_args);
  while (retval > 0) {
    retval = get_message_size(serverSocket);
    if (retval > 0) {
      buffer = calloc(1, retval);
      retval = read_all_from_socket(serverSocket, buffer, retval);
    }
    if (retval > 0) {
      printf("Message received: %s\n", buffer);
    }
    free(buffer);
    buffer = NULL;
  }

  pthread_cleanup_pop(0);
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

  if (argc < 3 || argc > 4) {
    fprintf(stderr, "Usage: %s <address> <port>\n",
            argv[0]);
    exit(1);
  }

  char *output_filename;
  if (argc == 5) {
    output_filename = argv[4];
  } else {
    output_filename = NULL;
  }

  // Setup signal handler
  signal(SIGINT, close_program);

  run_client(argv[1], argv[2]);

  return 0;
}
