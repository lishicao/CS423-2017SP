#include "userapp.h"

int main(int argc, char* argv[])
{
	
	printf("\n=====This is the write test starts\n");

	pid_t mypid = getpid();
	FILE *f = fopen ("/proc/mp1/status", "a");
	int written_bytes = fprintf(f, "%d\n", mypid);
	fclose(f);
	printf("My pid is %i, The written number of bytes is %lu\n", mypid, written_bytes);

	printf("\n=====This is the read test starts\n");
	f = fopen ("/proc/mp1/status", "r");
	int size_buffer = 256;
	char * buffer = (char*)malloc(size_buffer);
	ssize_t read_bytes = read (fileno(f), (void*)buffer, size_buffer);
	buffer[255]='\0';
	
	printf("The read stuff is %s, the read number of bytes is %lu.\n", buffer,read_bytes); 		
	fclose(f);
	return 0;
}
