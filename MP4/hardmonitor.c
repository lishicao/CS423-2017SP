#include "hardmonitor.h"


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
