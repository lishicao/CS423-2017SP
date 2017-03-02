#include "userapp.h"

void factorial (){
	int fact = 1000;
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
	printf("%d --- %s", pid,  target);	
	while(fgets(buffer, 255, f)) {
		printf("%s", buffer);	
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
	fprintf(f, "U, %d", pid);
	fclose(f);
}

// argv[1] : peroid
// argv[2] : process_time
// argv[3] : num_of_jobs
int main(int argc, char* argv[]) {
	if (argc != 4)
		return 0;
	printf("\nScheduler registration request.\n");\
	pid_t pid = getpid();
	reg(pid, argv[2], argv[2]);
	if (read_status(pid, argv[1], argv[2])) {
		printf("Registeration failed.\n");
		exit(1);
	}	
	/*
	printf("Registration succeeded.\n");
	yield(pid);
	int num_jobs = atoi(argv[3]);
	while (num_jobs > 0) {
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
	}
	unreg(pid);
*/
	return 0;
}

