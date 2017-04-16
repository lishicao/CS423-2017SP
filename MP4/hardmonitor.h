#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

typedef struct monitorInfo {
	double cpu_utilization;
} monitorInfo;

