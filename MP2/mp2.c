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
  printk("ENTERED WRITE\n");
  char* buf;
  buf = (char*) kmalloc(count+1, GFP_KERNEL);
  copy_from_user(buf, buffer, count);
  buf[count] = '\0';
  if (buf[0] == 'R') {
    printk("First letter is R.\n");
    registration(buf);
  }
  else if (buf[0] == 'Y') {
    printk("First letter is Y.\n");
    yield_handle(buf);
  }
  else if (buf[0] == 'D') {
    printk("First letter is D.\n");
    de_registration(buf);
  }
  else {
    printk("WTF\n");
    printk("%x\n", buf[0]);
  }
  kfree(buf);
  printk(KERN_ALERT "I'm at mp_write line  %d\n", __LINE__);
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

printk(KERN_ALERT "created mp_task_struct address is %x\n", new_task);
  //init variables
  sscanf(buf+3, "%d, %d, %d\n", &new_task->pid, &new_task->period, &new_task->processing_time);
  new_task->task_state = SLEEPING;
  new_task->next_period = 0;
  new_task->task = find_task_by_pid(new_task->pid);

//  printk("new_task->task: %x\n", new_task->task);

  setup_timer( &new_task->task_timer, _timer_callback, new_task->pid); 
//  printk("mmp timer %x\n", &(new_task->task_timer));
//  printk("registering %d: %d, %d\n", new_task->pid, new_task->period, new_task->processing_time);

  // admission control
  if (!admission_control(new_task)) return;

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

  // critical section begin
  spin_lock(&mylock);
  struct mp_task_struct *task_to_remove = __get_task_by_pid(pid);
  task_to_remove->task_state = SLEEPING;
  //printk("DELETE NODE: %x\n", task_to_remove->task_node); 
  del_timer( &task_to_remove->task_timer );
  list_del(&(task_to_remove->task_node));

  if(running_mptask == task_to_remove)
  {
    running_mptask = NULL;
    wake_up_process(dispatcher);
  }
  //clean up
  kmem_cache_free(mp_task_struct_cache, task_to_remove);
printk("de-reg unlocked\n");
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
  //return;
  unsigned long flags;
  spin_lock_irqsave(&mylock, flags);
  struct mp_task_struct *task_to_wake = __get_task_by_pid(pid);
  printk("Timer Callback, %x\n", task_to_wake);
  task_to_wake->task_state = READY;
  if(task_to_wake!=NULL)
  {
    printk("TASK is NOT NULL\n");
    //mod_timer( &task_to_wake->task_timer, jiffies + msecs_to_jiffies(task_to_wake->period) ); 
  }
  else
  {
    printk("TASK is NULL\n");
  }
  spin_unlock_irqrestore(&mylock, flags);
  wake_up_process(dispatcher);
  // set_task_state(task_to_wake->task, TASK_RUNNING);

  //set_current_state();
  //schedule();
}

void __set_priority(struct mp_task_struct *mytask, int policy, int priority)
{
  struct sched_param sparam;
  sparam.sched_priority = priority;
  printk("%x LINE %d\n", mytask->task, __LINE__);
  sched_setscheduler(mytask->task, policy, &sparam);
}
/**
dispatching thread
wake up in 2 cases: userapp signals YIELD; or timer interrupt
perform the preemption, select and arrange the next thread to run
*/
int thread_fn() {


  printk(KERN_INFO "I'm at dispatcher function line number %d\n",__LINE__);
  struct mp_task_struct *task_to_run;

  // the ktread_should_stop will be changed flag at the module exit, where the kthread_stop() is called. 
  while(!kthread_should_stop()){
  
    set_current_state(TASK_INTERRUPTIBLE);
    schedule();
    printk(KERN_INFO "I'm at dispatcher function line number %d\n",__LINE__);
      
    spin_lock(&mylock);

printk("thread_fn get the lock\n");
    // loop over the task entries and find the ready task with smallest period
    struct mp_task_struct *entry;
    int min_period=INT_MAX;
    printk(KERN_INFO "I'm at dispatcher function line number %d\n",__LINE__);
      
    list_for_each_entry(entry, &head, task_node)
    {
      if(entry->task_state == READY && entry->period<min_period ){
          task_to_run = entry;
          min_period = entry->period;
      }
    }
    printk(KERN_INFO "I'm at dispatcher function line number %d\n",__LINE__);

    if( task_to_run == NULL  ) 
    {
      //no task is ready
      printk(KERN_INFO "task_to_run is NULL\n");
      if(running_mptask != NULL)
      {
        printk("no task is ready\n");
        __set_priority(running_mptask, SCHED_NORMAL, 0);
      }
    }
    else 
    {
printk("task_to_run addr: %x\n", task_to_run);
    printk("TASK TO RUN %d\n", task_to_run->task->pid);
      if(running_mptask != NULL && task_to_run->period < running_mptask->period)
      {
        running_mptask->task_state = READY;
        __set_priority(running_mptask, SCHED_NORMAL, 0);
      }
    printk(KERN_INFO "I'm at dispatcher function line number %d\n",__LINE__);
      

      task_to_run->task_state = RUNNING;
      printk(KERN_INFO "I'm at dispatcher function line number %d\n",__LINE__);
      wake_up_process(task_to_run->task);
      printk(KERN_INFO "I'm at dispatcher function line number %d\n",__LINE__);
      __set_priority(task_to_run, SCHED_FIFO, 99);
      printk(KERN_INFO "I'm at dispatcher function line number %d\n",__LINE__);
      running_mptask = task_to_run;
printk(KERN_INFO "I'm at dispatcher function line number %d\n",__LINE__);
    }
    spin_unlock(&mylock);
  
    printk(KERN_INFO "I'm at dispatcher function line number %d\n",__LINE__);
      

/*
      spin_lock(&mylock);
  printk(KERN_INFO "I'm at dispatcher function line number %d\n",__LINE__);
      // set the previous running task state to ready and put to normal schedule
      struct sched_param sparam;
      if (running_mptask !=NULL){
        sparam.sched_priority=0;
        running_mptask->task_state = READY;
        sched_setscheduler(running_mptask->task, SCHED_NORMAL, &sparam);
        running_mptask = NULL;
  printk(KERN_INFO "I'm at dispatcher function line number %d\n",__LINE__);
      } 
      // loop over the task entries and find the ready task with smallest period
      struct mp_task_struct *entry;
      struct mp_task_struct * return_task=NULL;
  printk(KERN_INFO "I'm at dispatcher function line number %d\n",__LINE__);
      long minp=LONG_MAX;
  printk(KERN_INFO "I'm at dispatcher function line number %d\n",__LINE__);
      list_for_each_entry(entry, &head, task_node)
      { 
        if(entry->task_state == READY && entry->period<minp ){
            return_task = entry->task;
            minp=entry->period;
        }
      }
      // if there is a ready task, set it to running, install the timer
      if (return_task!=NULL){
      	return_task->task_state = RUNNING;
      	running_mptask = return_task;
      	setup_timer( &return_task->task_timer, _timer_callback, return_task->pid); 
      }     
      spin_unlock(&mylock);
     
      // if no ready task, then goto next loop, which sleeps at the beginning 
      if (return_task ==NULL){
        printk(KERN_INFO "no task in ready state\n");
        continue;
      }
      
      // wake up the returned task, set the priority and put it to real time scheduling
      wake_up_process(return_task);
      sparam.sched_priority=99;
      sched_setscheduler(return_task, SCHED_FIFO, &sparam);
*/
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

  struct mp_task_struct *task_to_yield = __get_task_by_pid(pid);
  printk(KERN_ALERT "FOUND TASK TO YIELD  %d pid is %d\n", __LINE__, (task_to_yield->task)->pid);
  

  //if(task_to_yield->next_period==0) should_skip=0;

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
//printk("AFTER MOD mmp timer %lu\n", task_to_yield->task_timer.expires);
  task_to_yield->task_state = SLEEPING;

  //set current running to null
  running_mptask = NULL;
  //printk("Yield - My PID is %d", (int)getpid());
  printk(KERN_ALERT "about to wake up dispacher  %d pid is %d\n", __LINE__, (task_to_yield->task)->pid);

  //wakeup dispatcher and schedule
  wake_up_process(dispatcher);
  set_task_state(task_to_yield->task, TASK_INTERRUPTIBLE);

printk("Yielding----%d[%d]: %d ms, %d ms\n", task_to_yield->pid, task_to_yield->task_state, task_to_yield->period, task_to_yield->processing_time);

//printk("----task_to_yield->task: %x\n", task_to_yield->task);

printk(KERN_ALERT "I'm at yield_handle line  %d\n", __LINE__);
  schedule();
printk(KERN_ALERT "I'm at yield_handle line  %d\n", __LINE__);
  //wake_up_process(dispatcher);
printk(KERN_ALERT "I'm at yield_handle line  %d\n", __LINE__);

  //return 0;
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
  printk("START ALLOCATE CACHE FOR SLAB\n");
  mp_task_struct_cache = KMEM_CACHE(mp_task_struct, SLAB_PANIC);

  // create dispatcher kernel thread
  printk(KERN_INFO "dispatching thread init\n");
  //dispatcher = kthread_create(thread_fn,NULL,"dispatcher");
  dispatcher = kthread_run(thread_fn,NULL,"dispatcher");

printk("head addr: %x\n", head);
  
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
  //struct list_head* n = head.next; 

	struct mp_task_struct *entry;
	struct mp_task_struct *temp_entry;

  // destroy the kernel thread
  int ret;
  ret = kthread_stop(dispatcher);
  if(!ret)
     printk(KERN_INFO "Dispatcher thread stopped\n");

  spin_lock(&mylock);
	list_for_each_entry_safe(entry, temp_entry, &head, task_node)
  {
		list_del(&(entry->task_node));
		del_timer( &entry->task_timer );
    kmem_cache_free(mp_task_struct_cache, entry);	
  }
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
