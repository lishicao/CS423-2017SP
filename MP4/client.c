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
static volatile int serverSocket; // this is a volatile ?
static pthread_t threads[2];

void *write_to_server(void *arg);
void *read_from_server(void *arg);
void close_program(int signal);
void bootstrap();
void* state_manager(void *p);

static volatile int job_finished, job_server_done;
//static volatile double throttle_value;

pthread_mutex_t mutex;
int server_flag=0;
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

				// set up and initialization of socket parameters
				memset(&hints, 0, sizeof(struct addrinfo));

				hints.ai_family = AF_INET;
				hints.ai_socktype = SOCK_STREAM;

				// get the network stuff information 
				s = getaddrinfo(host, port, &hints, &result);
				if (s != 0) {
								fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
								exit(1);
				}

				// connect the stuff
				int tmp = connect(serverSocket, result->ai_addr, result->ai_addrlen);
				if (tmp != 0) {
								perror(NULL);
								exit(1);
				}

				// initializatin of local job array
				int i;
				for (i=0; i<ARRAY_SIZE; i++) {
								job_array[i] = 1.111111;
				}
				// create job_todo queue with size of half the total size, job_tosend queue with size of half the total size
				// the job_todo and job_tosend are two thread safe queue, which holds void pointers to data
				job_todo = queue_create(JOB_NUM);
				job_tosend = queue_create(JOB_NUM);
				// mark the number of finished job locally and in the server to be 0.
				job_finished = 0;
				job_server_done = 0;
				pthread_mutex_init(&mutex, NULL); // why need mutex? nothing is doing anything at the boostrap
				bootstrap();

				// ---
				pthread_mutex_lock(&mutex); // why need mutex? nothing is doing anything when creating the two threads
				// install the two threads with write_to_server and read_from_server
				pthread_create(threads, 0, write_to_server, NULL);
				pthread_create(threads+1, 0, read_from_server, NULL);
				pthread_mutex_unlock(&mutex);

				//init state thread
				pthread_t state_thread;
				pthread_create(&state_thread, NULL, state_manager, NULL);

        pthread_t monitor_thread;
        pthread_create(&monitor_thread, NULL, update_throttle, NULL);

				// perform computation locally, the mutex is to protect job_finished
				pthread_mutex_lock(&mutex);
				while (job_finished < JOB_NUM) {
								pthread_mutex_unlock(&mutex);
								// pull a job id from the job_todo queue and work on it.
								int jobID = (int) ((long) queue_pull(job_todo));
								if (jobID == -1) continue; // jobID -1 indicates the jobs are finished, this is to move to next iteration.
								//printf("COMPUTIN LOCAL %d\n", jobID);
								compute_with_throttle(job_array, jobID);
								pthread_mutex_lock(&mutex);

								job_finished += 1;
								//printf("Local computation: job_finished = %d\n", job_finished);
				}
				pthread_mutex_unlock(&mutex);
				// when the above is finished, isn't it all the jobs are finished, since JOB_NUM is the total number of jobs?
				printf("local computation has finished\n");
				// push a dummy variable to job_tosend to indicate the local computation has finished
				queue_push(job_tosend, (void*) -1);

				// join the read_from_server and write to server
				pthread_join(threads[0], NULL);
				printf("thread[0] is joined\n");
				pthread_join(threads[1], NULL);
				printf("job has finished --> successfully joined 2 threads\n");

				printf("DOUBLE %f\n", job_array[0]);
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
								// push the number(job id) to the two queues as a start, in the form of void*
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
								//    printf("writter enters a NEW loop\n");
								pthread_mutex_lock(&mutex);
								// if all the jobs are finished, then exit the while(1)
								if (job_finished == JOB_NUM) {
												printf("job has finished --> exiting writter\n");
												pthread_mutex_unlock(&mutex);
												break;
								}
								pthread_mutex_unlock(&mutex);
								// pull from the job to send queue to send it
								int jobID = (int) ((long) queue_pull(job_tosend));
								transfer(jobID, serverSocket);	
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
												// if jobs are finished then push a -1 to the job_todo queue
												printf("job has finished --> exiting reader\n");
												queue_push(job_todo, (void*) -1);
												pthread_mutex_unlock(&mutex);
												break;
								}
								pthread_mutex_unlock(&mutex);

								int msg_type = get_msg_type(serverSocket);
								// printf("MESSAGE TYPE %d \n", msg_type);
								if(msg_type == -1) {
												//printf("GOT STATE MSG\n");
												state_handle(serverSocket);
								}
								else if (msg_type == 0) {
												int jobID = get_jobid(serverSocket);
												buffer = calloc(1, sizeof(double)*JOB_SIZE);
												retval = read_all_from_socket(serverSocket, (char*)buffer, sizeof(double) * JOB_SIZE);
												
												printf("GOT SERVER ID %d, %f\n", jobID, buffer[0]);
												if (jobID > -1 && jobID < JOB_NUM) {
																//printf("Message received: [%d] (%lu) %f\n", jobID, strlen((char*) buffer), buffer[0]);

																storeLocal(jobID, buffer);
																pthread_mutex_lock(&mutex);
																job_finished += 1;
																job_server_done += 1;
																//printf("job_finished: %d, job_server_done: %d\n", job_finished, job_server_done);
																pthread_mutex_unlock(&mutex);
												}
												else if (jobID >= JOB_NUM) {

																queue_push(job_todo, (void*) ((long) (jobID - JOB_NUM)));
												}

												free(buffer);
												buffer = NULL;
								}
								else {
												printf("BAD MSG_TYPE %x \n", msg_type);
								}
				}

				printf("exit reader\n");
				return 0;
}

void* state_manager(void *p) {
				pthread_detach(pthread_self());

				while(1) {
								send_state(job_todo->size, throttle, monitor_utilization, serverSocket);
								printf("SENDING STATE\n");
								sleep(INFO_PERIOD);
				}

				return NULL;
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
				sscanf(argv[3], "%f", &throttle);
				run_client(argv[1], argv[2]);

				return 0;
}
