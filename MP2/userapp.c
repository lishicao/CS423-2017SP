#include "userapp.h"

void factorial (int);
void call_kernel();


int main(int argc, char* argv[])
{
  /*
  printf("%d\n", (int) sizeof(argv));
  if((int) sizeof(argv)<3){
    printf("Usage: ./userapp repeat_time number\n"); 
    exit(1);
  }
  */
    
	int time_factorial = atoi(argv[1]);
	int value_factorial = atoi(argv[2]);

	printf("\n=====I'm registering my process.\n");

	pid_t mypid = getpid();
	FILE *f = fopen ("/proc/mp1/status", "a");
	int written_bytes = fprintf(f, "%d\0", mypid);
	fclose(f);
	printf("My pid is %i, pid has been registered\n", mypid);

	printf("Factorial and call kernel starts, this test has %d rounds.\n", time_factorial);
	while(time_factorial--!=0){
		factorial(value_factorial);
//		call_kernel();
	}
	call_kernel();
	return 0;
}

void factorial (int fact){

	int val =1;
	while(fact--!=0){
		val *= fact ;
	}
}


void call_kernel (){

	printf("\n=====This is the calling kernel starts and read, update the time.\n");
	FILE *f = fopen ("/proc/mp1/status", "r");
	int size_buffer = 2048;
	char * buffer = (char*)malloc(size_buffer);
	ssize_t read_bytes = read (fileno(f), (void*)buffer, size_buffer);
	buffer[read_bytes-1]='\0';
	
	printf("The read stuff is:\n%s", buffer); 		
	fclose(f);
}



