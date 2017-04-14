#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>


#include "utils.h"
static const size_t JOB_NUM = 1024;
static const size_t ARRAY_SIZE = 1024 * 1024 * 4;
static const size_t MESSAGE_SIZE_DIGITS = 4;

float THROTTLE = 1;
float CPU_USE = 0;

char *create_message(int jobID) {
  
  char *msg = (char*) calloc(1, 12);
  sprintf(msg, "%s", "Hello World!"); // TODO: do I need \n?

  return msg;
}

ssize_t get_message_size(int socket) {
  int32_t size;
  ssize_t read_bytes =
      read_all_from_socket(socket, (char *)&size, MESSAGE_SIZE_DIGITS);
  if (read_bytes == 0 || read_bytes == -1)
    return read_bytes;
  return (ssize_t)ntohl(size);
}

ssize_t write_message_size(size_t size, int socket) {
  uint32_t hostlong = htonl((uint32_t) size);
  ssize_t write_bytes = write_all_to_socket(socket,(char*) &hostlong,MESSAGE_SIZE_DIGITS);
  return write_bytes;
}

ssize_t read_all_from_socket(int socket, char *buffer, size_t count) {
  ssize_t read_bytes = 0;
  ssize_t total_read = 0;
  do {
    read_bytes = read(socket, buffer + total_read, count - (size_t) total_read);
    if (read_bytes == 0) return 0;
    if (read_bytes == -1 && errno == EINTR) continue;
    if (read_bytes == -1 && errno != EINTR) return -1;
    total_read += read_bytes;
  } while ((size_t) total_read < count);
  
  return total_read;
}

ssize_t write_all_to_socket(int socket, const char *buffer, size_t count) {
  ssize_t total_write = 0;
  ssize_t write_bytes = 0;
  do {
    write_bytes = write(socket, buffer + total_write, count - (size_t) total_write);
    if (write_bytes == 0) return 0;
    if (write_bytes == -1 && errno == EINTR) continue;
    if (write_bytes == -1 && errno != EINTR) return -1;
    total_write += write_bytes;
  } while ((size_t) total_write < count);
  return total_write;
}


void compute(int* vec, int i) {
  for(int j=0; j<6000; j++) {
    vec[i] += 1.111111;
  }
}

void compute_with_throttle(int* vec, int i, float throttle_value) {
  time_t start_time = time(NULL);
  compute(vec, i);
  time_t end_time = time(NULL);
  time_t sleeping_time = (end_time - start_time) * (1-throttle_value);
  printf("THROTTLE - SLEEP FOR %f \n", sleeping_time);
  sleep(sleeping_time);
}


