#include "userapp.h"
#include <stdlib.h>
#include <stdio.h>

void factorial (int factval){
	int fact = factval;
	int val =1;
	while(fact--!=0){
		val *= fact ;
	}
}

void reg(pid_t pid, char* period, char* process_time) {
	FILE *f = fopen("/proc/mp/status", "w");
	fprintf(f, "R, %d, %s, %s\n", pid, period, process_time);
	fclose(f);
}

int read_status(pid_t pid, char* period, char* process_time) {
	FILE *f = fopen ("/proc/mp/status", "r");
	char buffer[255];
	char target[255];
	sprintf(target, "%d[0]: %s ms, %s ms\n", pid, period, process_time);

	while(fgets(buffer, 255, f)) {
		printf("The returned buffer is %s",buffer);	
		if (!strcmp(buffer, target))
			return 0;
	}
	
	fclose(f);
	return -1;
}

void yield(pid_t pid) {
	FILE *f = fopen ("/proc/mp/status", "w");
	fprintf(f, "Y, %d", pid);
	fclose(f);
}

void unreg(pid_t pid) {
	FILE *f = fopen ("/proc/mp/status", "w");
	fprintf(f, "D, %d", pid);
	fclose(f);
}

// argv[1] : peroid
// argv[2] : process_time
// argv[3] : num_of_jobs
int main(int argc, char* argv[]) {
	
	if (argc != 4){
		printf("Reminder: put in three integer value in the following order: 1.peroid 2.numb_of_jobs.\n");		   return 1;
	}
	int factval = atoi(argv[3]);

	struct timeval start;
	struct timeval end;
	gettimeofday(&start, NULL);
	factorial(factval);
	factorial(factval);
	factorial(factval);
	factorial(factval);
	factorial(factval);
	gettimeofday(&end, NULL);
	int safe_processtime = (int)(end.tv_usec-start.tv_usec)/5*(1.2);
	char proctime[32];
	sprintf(proctime,"%d",safe_processtime);
	printf("The average time with 1.2 safe parameter for factorial is : %dms.\n",safe_processtime);
	
	printf("\nScheduler registration request.\n");
	pid_t pid = getpid();
	reg(pid, argv[1], proctime);
	if (read_status(pid, argv[1], proctime)) {
		printf("Registeration failed.\n");
		exit(1);
	}	
	printf("Registration succeeded.\n");
	yield(pid);
	
	int num_jobs = atoi(argv[2]);
	while (num_jobs-- > 0) {
	/*
		struct timeval tv;
		gettimeofday(&tv, NULL);
		suseconds_t t = tv.tv_usec;
		num_jobs--;
		factorial();
		yield(pid);
		gettimeofday(&tv, NULL);
		struct timespec ts;
		ts.tv_nsec = (atoi(argv[1])*1000000-(tv.tv_usec-t))*1000;
		nanosleep(&ts, NULL);
	*/
	/*	
		num_jobs--;
		printf("userapp iteration %d\n", num_jobs);
		sleep(1);
		yield(pid);
	*/
		gettimeofday(&start, NULL);
		factorial(factval);
		gettimeofday(&end, NULL);
		yield(pid);	
		int actual_processingtime = (int)(end.tv_usec-start.tv_usec);
		printf("The actual time it takes for factorial is : %dms.\n",actual_processingtime);
	}
	// sleep(1);
	unreg(pid);
	return 0;
}

