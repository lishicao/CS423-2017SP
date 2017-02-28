#ifndef __MP2_GIVEN_INCLUDE__
#define __MP2_GIVEN_INCLUDE__

#include <linux/pid.h>
#include <linux/sched.h>

struct task_struct* find_task_by_pid(unsigned int nr)
{
    struct task_struct* task;
    rcu_read_lock();
    task=pid_task(find_vpid(nr), PIDTYPE_PID);
    rcu_read_unlock();
    return task;
}

//#define find_task_by_pid(nr) pid_task(find_vpid(nr), PIDTYPE_PID)

//THIS FUNCTION RETURNS 0 IF THE PID IS VALID AND THE CPU TIME IS SUCCESFULLY RETURNED BY THE PARAMETER CPU_USE. OTHERWISE IT RETURNS -1
int get_cpu_use(int pid, unsigned long *cpu_use)
{
   struct task_struct* task;
   rcu_read_lock();
   task=find_task_by_pid(pid);
   if (task!=NULL)
   {
  *cpu_use=task->utime;
        rcu_read_unlock();
        return 0;
   }
   else
   {
     rcu_read_unlock();
     return -1;
   }
}

#endif
