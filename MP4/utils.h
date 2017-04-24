#ifndef _UTILSH_
#define _UTILSH_

#include <string.h>
#include "queue.h"
#include <stdio.h>
/**
 * The largest size the message can be that a client
 * sends to the server.
 */
#define MSG_SIZE (256)
#define JOB_NUM 1024      // the whole job can be divided into this many jobs, using the job size below
#define JOB_SIZE 1024 * 4 // each job covers this many entries in the array
#define MESSAGE_SIZE_DIGITS 4
#define ARRAY_SIZE 1024 * 1024 * 4
#define MSG_TYPE_DATA 0
#define MSG_TYPE_STATE -1
#define MSG_TYPE_REBALANCE 2
#define INFO_PERIOD 1
#define DIFF_THRESHOLD 5

extern volatile float throttle;

extern int peer_num_jobs;
extern float peer_throttle_value;
extern double peer_cpu_usage;
extern int server_flag;
extern int transfer_policy;
extern int rebalance_requested;
extern int job_sent;

queue_t *job_todo;
queue_t *job_tosend;
double job_array[ARRAY_SIZE];
double monitor_utilization;


/**
 * Builds a message in the form of
 * <name>: <message>\n
 *
 * Returns a char* to the created message on the heap
 */
double *create_message(int job_id, double *data);

/**
 * Read the first four bytes from socket and transform it into ssize_t
 *
 * Returns the size of the incomming message,
 * 0 if socket is disconnected, -1 on failure
 */
ssize_t get_message_size(int socket);

/**
 * Writes the bytes of size to the socket
 *
 * Returns the number of bytes successfully written,
 * 0 if socket is disconnected, or -1 on failure
 */
ssize_t write_message_size(size_t size, int socket);

/**
 * Attempts to read all count bytes from socket into buffer.
 * Assumes buffer is large enough.
 *
 * Returns the number of bytes read, 0 if socket is disconnected,
 * or -1 on failure.
 */
ssize_t read_all_from_socket(int socket, char *buffer, size_t count);

/**
 * Attempts to write all count bytes from buffer to socket.
 * Assumes buffer contains at least count bytes.
 *
 * Returns the number of bytes written, 0 if socket is disconnected,
 * or -1 on failure.
 */
ssize_t write_all_to_socket(int socket, char *buffer, size_t count);


void compute(double* vec, int i);
void compute_with_throttle(double* vec, int i);
void storeLocal(int jobId, double* data);
int get_jobid(int socket);
int write_jobid(int job_id, int socket);
int send_msg_type(int value, int socket);
int send_state(int num_jobs, float throttle, double cpu_utilization, int socket);
int get_msg_type(int socket);
int state_handle(int socket);
int adaptor(int socket);
int transfer(int jobID, int socket);
void *update_throttle(void *p);
#endif
