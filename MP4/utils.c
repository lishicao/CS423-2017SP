#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include "utils.h"
float CPU_USE = 0;

int peer_num_jobs = -1;
float peer_throttle_value = 0;
double peer_cpu_usage = 0;

volatile float throttle = 1;
pthread_mutex_t transfer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;

int transfer_policy = 0;

double *create_message(int job_id, double *data) {
  //double *msg = (double*) calloc(1, 12);
  //sprintf(msg, "%s", "Hello World!"); // TODO: do I need \n?
  if(job_id > JOB_NUM)
    job_id -= JOB_NUM;
  double *ret = data + JOB_SIZE * job_id;

  //printf("create msg: %f\n", ret[0]); 
  return ret;
}

int send_state(int num_jobs, float _throttle, double cpu_utilization, int socket) {
	int state[4];
	state[0] = num_jobs;
	*(float*)(&state[1]) = _throttle;
	*(double*)(&state[2]) = cpu_utilization;
  pthread_mutex_lock(&transfer_mutex);
  send_msg_type(MSG_TYPE_STATE, socket);
	int ret = write_all_to_socket(socket, (char*)state, 16);
  pthread_mutex_unlock(&transfer_mutex);
	if (ret != 16)
		return -1;
	return 0;
}

int get_single_value(int socket) {
  int job_id;
  ssize_t read_bytes =
      read_all_from_socket(socket, (char *)(&job_id), MESSAGE_SIZE_DIGITS);
	//printf("READ BYTES %d \n", read_bytes);
  if (read_bytes == 0 || read_bytes == -1)
    return -2;
  return (int)ntohl(job_id);
}

int get_msg_type(int socket) {
  return get_single_value(socket);
}

int get_jobid(int socket) {
  return get_single_value(socket);
}

int send_single_value(int value, int socket) {
  uint32_t hostlong = htonl((uint32_t) value);
  //the message_size_digits is the size of jobID, which is to be written ot the server.
  ssize_t write_bytes = write_all_to_socket(socket,(char*) &hostlong, MESSAGE_SIZE_DIGITS);
  return write_bytes;
}

// write the job id to the server 
int write_jobid(int job_id, int socket) {
  return send_single_value(job_id, socket);
}

int send_msg_type(int value, int socket) {
  //printf("send msg_type %d \n", value);
  return send_single_value(value, socket);
}

ssize_t read_all_from_socket(int socket, char *buffer, size_t count) {
  ssize_t read_bytes = 0;
  ssize_t total_read = 0;
	//pthread_mutex_lock(&socket_mutex);
  do {
    read_bytes = read(socket, (char*)buffer + total_read, count - (size_t) total_read);
    if (read_bytes == 0) return 0;
    if (read_bytes == -1 && errno == EINTR) continue;
    if (read_bytes == -1 && errno != EINTR) return -1;
    total_read += read_bytes;
  } while ((size_t) total_read < count);
	//pthread_mutex_unlock(&socket_mutex);
  return total_read;
}

ssize_t write_all_to_socket(int socket, char *buffer, size_t count) {
  ssize_t total_write = 0;
  ssize_t write_bytes = 0;
  // write everything to socket and indicate if there is an error
//	pthread_mutex_lock(&socket_mutex);
  do {
    write_bytes = write(socket, (char*)buffer + total_write, count - (size_t) total_write);
    if (write_bytes == 0) return 0;
    if (write_bytes == -1 && errno == EINTR) continue;
    if (write_bytes == -1 && errno != EINTR) return -1;
    total_write += write_bytes;
  } while ((size_t) total_write < count);
//	pthread_mutex_unlock(&socket_mutex);
  return total_write;
}

// do the dummy computation with the given jobID, which covers one batch of array
void compute(double* vec, int jobID) {
  for(int i=jobID * JOB_SIZE; i<(jobID+1) * JOB_SIZE; i++) {
    for(int j=0; j<6000; j++) {
      vec[i] += 1.111111;
    }
  }
}

// do the dummy computation for this jobID, and nanosleep with the throttle value
void compute_with_throttle(double* vec, int i) {
		struct timeval start;
		gettimeofday(&start, NULL);
		compute(vec, i);
		struct timeval end;
		gettimeofday(&end, NULL);
		int diffMsec = end.tv_usec - start.tv_usec;
		struct timespec ts;
		time_t seconds = end.tv_sec - start.tv_sec;
		time_t usec = end.tv_usec - start.tv_usec;
		time_t sleepTime = (seconds*1000000+usec) * (1 - throttle);
		seconds = sleepTime/1000000;
		usec = sleepTime - seconds*1000000;
		ts.tv_sec = seconds;
		ts.tv_nsec = usec*1000;
		nanosleep(&ts, NULL);
    //printf("waked up\n");
}

void getInfo_fromMonitor(){

        // get the cpu utilization ratio
        double a[4];
        FILE* fp;
        // the first line of /proc/stat is the total cpu usage : user, nice, system, and idle times
        fp = fopen("/proc/stat","r");
        fscanf(fp,"%*s %lf %lf %lf %lf",&a[0],&a[1],&a[2],&a[3]);
        monitor_utilization  = (a[0]+a[1]+a[2]) / (a[0]+a[1]+a[2]+a[3]);

        fclose(fp);

}

void storeLocal(int jobId, double* data) {
  int i;
  for (i = 0; i < JOB_SIZE; i++ )
    job_array[i+4096*jobId] = data[i];
  return;
}

int state_handle(int socket) {
  //printf("entered state handler\n");
	int buffer[4];
	int retval = read_all_from_socket(socket, (char *)&buffer, 16);
	if(retval != 16)
    return -1;
  if(peer_num_jobs==-1 && buffer[0]==0)
    return 0;
  peer_num_jobs = buffer[0];
  peer_throttle_value = (float) buffer[1];
  peer_cpu_usage = *(double*)(&buffer[2]);

  //transfer policy
  //0: receiver-initiated
  //1: sender-initiated
  //2: symmetric initiated
  if(transfer_policy==2 || (transfer_policy ^ server_flag))
    adaptor(socket);
  return 0;
}

int adaptor(int socket) {
  //return 0;
  printf("Entered Adaptor! Local Job Size: %d, Peer Job Size: %d, current throttle value %f \n", job_todo->size, peer_num_jobs, throttle);
  int diff = job_todo->size - peer_num_jobs;
  int to_transfer = diff/2;
  
  //return 0; 
  if(diff > DIFF_THRESHOLD && peer_num_jobs != -1) {
    //moving job
    int jobID = 0;
    while( to_transfer>0 && (jobID = (int)(long)queue_pull(job_todo)) > -1) {
      printf("[LAOD BALANCE] sending jobID: %d\n", jobID);
      //adding JOB_NUM to indicate not yet computed jobs
      transfer(JOB_NUM+jobID, socket);
      to_transfer--;
    }
  }
  return 0;
}

int transfer(int jobID, int socket) {
  /*
   * jobID: 
   * -1 all all job done
   * 0-1023 job done
   * >1024 load balance job to transfer
   */
  pthread_mutex_lock(&transfer_mutex);
  char *buffer = NULL;
  send_msg_type(MSG_TYPE_DATA, socket);
  //printf("job array is at %p\n", job_array);
  double* msg = create_message(jobID, job_array);
  int retval = write_jobid(jobID, socket);
  if(jobID == -1) {
    pthread_mutex_unlock(&transfer_mutex);
    return 0;
  }

  if (retval > 0)
    retval = write_all_to_socket(socket, (char*)msg, JOB_SIZE*sizeof(double));
  if(retval<=0)
		printf("Sending job: %d - Bytes Sent: %d\n", jobID, retval);

  pthread_mutex_unlock(&transfer_mutex);
  //printf("Message sent: [%d] %f\n", jobID, msg[0]);
  return 0;
}

void *update_throttle(void *p) {
  while(1) {
	  char digits[10];
    fgets(digits, 10, stdin);
	  throttle = atof(digits);
		printf("Throttle has been updated to %f", throttle);
  }
  return NULL;
}
