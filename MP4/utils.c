#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include "utils.h"
float THROTTLE = 1;
float CPU_USE = 0;

typedef struct monitorInfo {
        double cpu_utilization;
} monitorInfo;

double *create_message(int job_id, double *data) {
  
  //double *msg = (double*) calloc(1, 12);
  //sprintf(msg, "%s", "Hello World!"); // TODO: do I need \n?
  double *ret = data + JOB_SIZE * job_id;

  return ret;
}

int get_jobid(int socket) {
  int32_t job_id;
  ssize_t read_bytes =
      read_all_from_socket(socket, (double *)&job_id, MESSAGE_SIZE_DIGITS);
  if (read_bytes == 0 || read_bytes == -1)
    return -2;
  return (int)ntohl(job_id);
}

int write_jobid(int job_id, int socket) {
  uint32_t hostlong = htonl((uint32_t) job_id);
  ssize_t write_bytes = write_all_to_socket(socket,(double*) &hostlong, MESSAGE_SIZE_DIGITS);
  return write_bytes;
}

ssize_t read_all_from_socket(int socket, double *buffer, size_t count) {
  ssize_t read_bytes = 0;
  ssize_t total_read = 0;
  do {
    read_bytes = read(socket, (char*)buffer + total_read, count - (size_t) total_read);
    if (read_bytes == 0) return 0;
    if (read_bytes == -1 && errno == EINTR) continue;
    if (read_bytes == -1 && errno != EINTR) return -1;
    total_read += read_bytes;
  } while ((size_t) total_read < count);
  
  return total_read;
}

ssize_t write_all_to_socket(int socket, double *buffer, size_t count) {
  ssize_t total_write = 0;
  ssize_t write_bytes = 0;
  do {
    write_bytes = write(socket, (char*)buffer + total_write, count - (size_t) total_write);
    if (write_bytes == 0) return 0;
    if (write_bytes == -1 && errno == EINTR) continue;
    if (write_bytes == -1 && errno != EINTR) return -1;
    total_write += write_bytes;
  } while ((size_t) total_write < count);
  return total_write;
}


void compute(double* vec, int jobID) {
  for(int i=jobID * JOB_SIZE; i<(jobID+1) * JOB_SIZE; i++) {
    for(int j=0; j<6000; j++) {
      vec[i] += 1.111111;
    }
  }
}
void compute_with_throttle(double* vec, int i) {
    float throttle_value=1;
		struct timeval start;
		gettimeofday(&start, NULL);
		compute(vec, i);
		struct timeval end;
		gettimeofday(&end, NULL);
		int diffMsec = end.tv_usec - start.tv_usec;
		struct timespec ts;
		time_t seconds = end.tv_sec - start.tv_sec;
		time_t usec = end.tv_usec - start.tv_usec;
		time_t sleepTime = (seconds*1000000+usec) * (1 - throttle_value);
		seconds = sleepTime/1000000;
		usec = sleepTime - seconds*1000000;
		ts.tv_sec = seconds;
		ts.tv_nsec = usec*1000;
		nanosleep(&ts, NULL);
}

void getInfo_fromMonitor(monitorInfo* holder){

        // get the cpu utilization ratio
        double a[4];
        FILE* fp;
        // the first line of /proc/stat is the total cpu usage : user, nice, system, and idle times
        fp = fopen("/proc/stat","r");
        fscanf(fp,"%*s %lf %lf %lf %lf",&a[0],&a[1],&a[2],&a[3]);
        holder->cpu_utilization  = (a[0]+a[1]+a[2]) / (a[0]+a[1]+a[2]+a[3]);

        fclose(fp);

}

void storeLocal(int jobId, double* data) {
  int i;
  for (i = 0; i < JOB_SIZE; i++)
    job_array[i] = data[i];
  return;
}
