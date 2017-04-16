#include <utils.h>

void storeLocal(int jobId, double* data) {
	int i;
	for (i = 0; i < JOB_SIZE; i++)
		job_array[i] = data[i];
	return;
}
