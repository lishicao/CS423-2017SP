#define LINUX

#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "mp2_given.h"
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>

#include <linux/delay.h>

/**
static variables and struct
*/
static struct proc_dir_entry *proc_dir;
static struct kmem_cache *mp_task_struct_cache;
static struct task_struct *dispatcher;
static struct mp_task_struct *running_mptask;

static spinlock_t mylock;
void _timer_callback(int);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_10");
MODULE_DESCRIPTION("CS-423 MP1");

LIST_HEAD(head);

#define SLEEPING 0
#define READY 1
#define RUNNING 2

/**
Augmented Task Struct
*/
struct mp_task_struct
{
  struct task_struct *task;
  struct list_head task_node;
  struct timer_list task_timer;

  int task_state;
  unsigned long next_period;
  int pid;
  int period;
  int processing_time;
};

int flag = 1;
#define DEBUG 1

/*
 * Find Mp Task Struct by PID
 * this iterate through the list
 */
struct mp_task_struct *__get_task_by_pid(int pid)
{
    struct mp_task_struct *tmp;
    list_for_each_entry(tmp, &head, task_node) {
        if (tmp->pid == pid) {
            return tmp;
        }
    }
    // if no task with such pid, then return NULL
    return NULL;
}

/**
read function:
called when userapp read from proc file system
goes through the list of all registered threads, return info for each of them
*/
static ssize_t mp_read (struct file *file, char __user *buffer, size_t count, loff_t *data)
{
  printk(KERN_ALERT "read function is called!!! %d\n", *data);
  if(*data>0)
    return 0;
  
  int copied = 0;
  char * buf;
  struct mp_task_struct* entry;
  int offset = 0;

  buf = (char *) kmalloc(2048,GFP_KERNEL); 
  // critical section begin
  spin_lock(&mylock);
  list_for_each_entry(entry, &head, task_node) {
    char temp[256];
    sprintf(temp, "%d[%d]: %d ms, %d ms\n", entry->pid, entry->task_state, entry->period, entry->processing_time);
    strcpy(buf + offset, temp);
    offset = strlen(buf);
  }
  spin_unlock(&mylock);
  // critical section end
  
  buf[strlen(buf)] = '\0'; 
  copied = strlen(buf)+1;
  copy_to_user(buffer, buf, copied);
  
  kfree(buf);
  *data += copied;
  return copied;
}

/**
write function:
called when userapp write to prof file system
handle 3 cases based on the information passed in by userapp
*/
static ssize_t mp_write (struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
  printk("ENTERED WRITE FUNCTION\n");
  char* buf;
  buf = (char*) kmalloc(count+1, GFP_KERNEL);
  copy_from_user(buf, buffer, count);
  buf[count] = '\0';
  if (buf[0] == 'R') {
    registration(buf);
  }
  else if (buf[0] == 'Y') {
    yield_handle(buf);
  }
  else if (buf[0] == 'D') {
    de_registration(buf);
  }
  else {
    printk("WTF\n");
    printk("%x\n", buf[0]);
  }
  kfree(buf);
  return count;
}

/**
[Important] only use integer arithmatic
go through the list of registered threads, calculate the accumulative utilization for all threads
return 1 (true) if the new task can be accepted, 0 (false) otherwise
*/
int admission_control(struct mp_task_struct* new_task) {
  unsigned long total_util = new_task->processing_time * 1000000 / new_task->period;

  struct mp_task_struct* entry;
  // critical section begin
  spin_lock(&mylock);
  list_for_each_entry(entry, &head, task_node) {
    total_util += (entry->processing_time * 1000000) / entry->period;
  }
  spin_unlock(&mylock);
  // critical section end
  if (total_util <= 693000) return 1;
  else return 0;
}

/**
register the new task into task list
create a corresponding struct from the slab allocator
if new task passes admission control, add it to the list
*/
void registration(char* buf) {
  struct mp_task_struct* new_task;
  new_task = (struct mp_task_struct*) kmem_cache_alloc(mp_task_struct_cache, GFP_KERNEL);
  INIT_LIST_HEAD(&new_task->task_node);

  //init variables
  sscanf(buf+3, "%d, %d, %d\n", &new_task->pid, &new_task->period, &new_task->processing_time);
  new_task->task_state = SLEEPING;
  new_task->next_period = 0;
  new_task->task = find_task_by_pid(new_task->pid);
  setup_timer( &new_task->task_timer, _timer_callback, new_task->pid); 

  printk("REGISTER PID %d created mp_task_struct address is %x\n", new_task->pid, new_task);
  // admission control
  if (!admission_control(new_task)) 
  {
    kmem_cache_free(mp_task_struct_cache, new_task);
    return;
  }

  // critical section begin
  spin_lock(&mylock);
  list_add(&new_task->task_node, &head);
  spin_unlock(&mylock);
  // critical section end
  printk(KERN_ALERT "registratin succesfully finished\n");
}


/**
de-register a task
if this task is running, preempt it
go through the task list, find the task by pid and remove it from list
*/
void de_registration(char* buf)
{
  // read in pid from buffer
  int pid;
  sscanf(buf+3, "%d\n", &pid);
  printk(KERN_ALERT "DE-REGISTER PID %d\n", pid);
  // critical section begin
  spin_lock(&mylock);
  struct mp_task_struct *task_to_remove = __get_task_by_pid(pid);
  task_to_remove->task_state = SLEEPING;
  del_timer( &task_to_remove->task_timer );
  list_del(&(task_to_remove->task_node));
  kmem_cache_free(mp_task_struct_cache, task_to_remove);
  
  if(running_mptask == task_to_remove)
  {
    running_mptask = NULL;
    wake_up_process(dispatcher);
  }
  //clean up
  spin_unlock(&mylock);
  // critical section end
}

/**
Top half interrupt:
wake up the dispatching thread
*/
void _timer_callback( int pid )
{
  printk("entered timer callback, pid is %d\n", pid);
  unsigned long flags;
  
  spin_lock_irqsave(&mylock, flags);
  // find mp task struct by pid
  struct mp_task_struct *task_to_wake = __get_task_by_pid(pid);
  //set its status to ready and wake up dispatcher
  task_to_wake->task_state = READY;
  spin_unlock_irqrestore(&mylock, flags);
  wake_up_process(dispatcher);
}

/*
 * set the task priority helper
 */
void __set_priority(struct mp_task_struct *mytask, int policy, int priority)
{
  struct sched_param sparam;
  sparam.sched_priority = priority;
  sched_setscheduler(mytask->task, policy, &sparam);
}

/**
dispatching thread
wake up in 2 cases: userapp signals YIELD; or timer interrupt
perform the preemption, select and arrange the next thread to run
*/
int thread_fn() {
  struct mp_task_struct *task_to_run;

  // the ktread_should_stop will be changed flag at the module exit, where the kthread_stop() is called. 
  while(1){
    //go to sleep when initialized
    set_current_state(TASK_INTERRUPTIBLE);
    schedule();
    //exit if needed
    if(kthread_should_stop()) return 0;
    
    spin_lock(&mylock);

    // loop over the task entries and find the ready task with smallest period
    struct mp_task_struct *entry;
    task_to_run = NULL;
    int min_period=INT_MAX;

    list_for_each_entry(entry, &head, task_node)
    {
      if(entry->task_state == READY && entry->period < min_period ){
          task_to_run = entry;
          min_period = entry->period;
      }
    }
    
    // if we didn't find a task we need to schedule
    if( task_to_run == NULL  ) 
    {
      //no task is ready just preempt current running task
      if(running_mptask != NULL)
      {
        printk("no task is ready\n");
        __set_priority(running_mptask, SCHED_NORMAL, 0);
        running_mptask = NULL; //---------------------------------
      }
    }
    else 
    {
      //found a higher priority job is READY; preempt current one
      if(running_mptask != NULL && task_to_run->period < running_mptask->period)
      {
        printk(KERN_ALERT "PREEMPTING PID: %d\n", running_mptask->pid);
        running_mptask->task_state = READY;
        __set_priority(running_mptask, SCHED_NORMAL, 0);
      }

      //set task_to_run to run
      task_to_run->task_state = RUNNING;
      wake_up_process(task_to_run->task);
      __set_priority(task_to_run, SCHED_FIFO, 99);
      running_mptask = task_to_run;
    }
    spin_unlock(&mylock);
  }

  return 0;
}


/**
yield handler
find and change the task state to READY
*/
void yield_handle(char* buf) 
{
  // read in pid
  int pid;
  sscanf(buf+3, "%d\n", &pid);
printk("YIELD PID %d\n", pid);
  struct mp_task_struct *task_to_yield = __get_task_by_pid(pid);

  task_to_yield->next_period += task_to_yield->next_period==0 ? 
    jiffies + msecs_to_jiffies(task_to_yield->period) :
    msecs_to_jiffies(task_to_yield->period);

  //if only the next period has not start yet
  if(task_to_yield->next_period< jiffies){
    printk(KERN_ALERT "skip current!! LINE  %d\n", __LINE__);
    return;
  }

  //setup timer
  mod_timer(&(task_to_yield->task_timer), task_to_yield->next_period);
  task_to_yield->task_state = SLEEPING;

  //set current running to null
  /*
  if(running_mptask == task_to_yield)
  {
    running_mptask = NULL;
  }
  */

  //wakeup dispatcher and schedule
  wake_up_process(dispatcher);
  set_task_state(task_to_yield->task, TASK_INTERRUPTIBLE);

  schedule();
}

static const struct file_operations mp_file = {
  .owner = THIS_MODULE, 
  .read = mp_read,
  .write = mp_write,
};

/**
mp_init - Called when module is loaded
create proc directory and file
*/
int __init mp_init(void)
{
  #ifdef DEBUG
  printk(KERN_ALERT "MP MODULE LOADING\n");
  #endif
  proc_dir =  proc_mkdir("mp",NULL);
  proc_create("status",0666, proc_dir, &mp_file);  
  spin_lock_init(&mylock);

  // intialize slab allocator
  mp_task_struct_cache = KMEM_CACHE(mp_task_struct, SLAB_PANIC);

  // create dispatcher kernel thread
  dispatcher = kthread_run(thread_fn,NULL,"dispatcher");
  
  printk(KERN_ALERT "MP MODULE LOADED\n");
  return 0;   
}

/**
mp_exit - Called when module is unloaded
remove proc directory and file
clean the linked list and free all memory used
*/
void __exit mp_exit(void)
{
  #ifdef DEBUG
  printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
  #endif

	struct mp_task_struct *entry;
	struct mp_task_struct *temp_entry;

  // destroy the kernel thread
  int ret;
  ret = kthread_stop(dispatcher);
  if(!ret)
     printk(KERN_INFO "Dispatcher thread stopped\n");

  spin_lock(&mylock);
  //go through the list and detroy the list entry and timer inside of mp task struct
  list_for_each_entry_safe(entry, temp_entry, &head, task_node)
  {
    list_del(&(entry->task_node));
    del_timer( &entry->task_timer );
    kmem_cache_free(mp_task_struct_cache, entry);	
  }
  //destroy allocated memory
  kmem_cache_destroy(mp_task_struct_cache);
  
  spin_unlock(&mylock);
  printk(KERN_ALERT "MP1 MODULE UNLOADED\n");

  //remove proc entry
  remove_proc_entry("status", proc_dir);
  remove_proc_entry("mp", NULL);
}

// Register init and exit funtions
module_init(mp_init);
module_exit(mp_exit);
