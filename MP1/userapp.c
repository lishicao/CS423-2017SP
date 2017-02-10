#include "userapp.h"

int main(int argc, char* argv[])
{

	printf("This is the user application\n");
	int fileid = open("/proc/mp1/status",O_RDONLY);
	
	int bytestoread = 3;
	char * buffer = (char*)malloc(bytestoread);
	ssize_t readbytes = read (fileid, (void*)buffer, bytestoread-1);
	buffer[bytestoread]='\0';
	
	printf("The read stuff is %s.\n", buffer); 		

	return 0;
}
