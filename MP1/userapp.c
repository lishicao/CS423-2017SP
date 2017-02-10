#include "userapp.h"

int main(int argc, char* argv[])
{
	
	printf("This is the write test starts\n");

	pid_t mypid = getpid();
	int fileid = open ("/proc/mp1/status", O_RDWR);
	ssize_t written_bytes = write(fileid, &mypid, sizeof(pid_t));

	printf("My pid is %i, The written number of bytes is %lu\n", mypid, written_bytes);

	printf("This is the read test starts\n");
	
	int size_buffer = 256;
	char * buffer = (char*)malloc(size_buffer);
	ssize_t read_bytes = read (fileid, (void*)buffer, size_buffer);
	buffer[read_bytes-1]='\0';
	
	printf("The read stuff is %s, the read number of bytes is %lu.\n", buffer,read_bytes); 		
	close(fileid);
	return 0;
}
