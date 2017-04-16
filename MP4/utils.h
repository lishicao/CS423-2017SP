#include <string.h>
/**
 * The largest size the message can be that a client
 * sends to the server.
 */
#define MSG_SIZE (256)
#define JOB_NUM 1024
#define JOB_SIZE 1024 * 4
#define MESSAGE_SIZE_DIGITS 4
#define ARRAY_SIZE 1024 * 1024 * 4
static double job_array[ARRAY_SIZE];

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
ssize_t read_all_from_socket(int socket, double *buffer, size_t count);

/**
 * Attempts to write all count bytes from buffer to socket.
 * Assumes buffer contains at least count bytes.
 *
 * Returns the number of bytes written, 0 if socket is disconnected,
 * or -1 on failure.
 */
ssize_t write_all_to_socket(int socket, double *buffer, size_t count);


void compute(double* vec, int i);
void compute_with_throttle(double* vec, int i);
void storeLocal(int jobId, double* data);
int get_jobid(int socket);
int write_jobid(int job_id, int socket);
