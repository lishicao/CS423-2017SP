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

void *process_client(void *p);
void write_to_clients(const char *message, size_t size);

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
      printf("New client is connected but ignored\n");
      pthread_mutex_unlock(&mutex);
      continue;
    }
    // Launching a new thread to serve the client
    pthread_t thread;
    int retval = pthread_create(&thread, NULL, process_client, NULL);
    if (retval != 0) {
      perror("pthread_create():");
      exit(1);
    }
  }
  // Be sure to free the address info here
  freeaddrinfo(result);
}

void write_to_clients(const char *message, size_t size) {
  printf("Message to send: %s\n", message);
  pthread_mutex_lock(&mutex);
  if (clientFd != -1) {
    ssize_t retval = write_message_size(size, clientFd);
    if (retval > 0)
      retval = write_all_to_socket(clientFd, message, size);
    if (retval == -1)
      perror("write():");
    printf("sent %zu bytes\n", retval);
  }
  pthread_mutex_unlock(&mutex);
}

void *process_client(void *p) {
  pthread_detach(pthread_self());
  ssize_t retval = 1;
  char *buffer = NULL;

  while (retval > 0 && sessionEnd == 0) {
    retval = get_message_size(clientFd);
    if (retval > 0) {
      buffer = calloc(1, retval);
      retval = read_all_from_socket(clientFd, buffer, retval);
      printf("Message received: %s\n", buffer);
    }
    if (retval > 0) {
      sleep(3);
      write_to_clients(buffer, retval);
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


int main(int argc, char **argv) {

  if (argc != 2) {
    fprintf(stderr, "./server <port>\n");
    return -1;
  }

  signal(SIGINT, close_server);
  run_server(argv[1]);

  return 0;
}

