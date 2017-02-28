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

/**
static variables and struct
*/
static struct proc_dir_entry *proc_dir;
static struct timer_list timer;
static struct work_struct bottom_work;
static struct kmem_cache *mp_task_struct_cache;
static struct task_struct *dispatcher;
static struct mp_task_struct *running_mptask;

struct workqueue_struct *mp_q;
static spinlock_t mylock;


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_10");
MODULE_DESCRIPTION("CS-423 MP1");

LIST_HEAD(head);

#define SLEEPING 0
#define READY 1
#define RUNNING 2

/**
Linked list struct to store registered pid and corresponding cpt time
*/
/*
struct mp_list {
  struct list_head list;
  unsigned long cpu_time;
  long pid;
};
*/
/**
 * Augmented Task Struct
*/
struct mp_task_struct
{
  struct task_struct *task;
  struct list_head task_node;
  struct timer_list task_timer;

  unsigned int task_state;
  uint64_t next_period;
  unsigned int pid;
  unsigned long period;
  unsigned long processing_time;
  unsigned long slice;
};

int flag = 1;
#define DEBUG 1

/**
Called when user request cpu time about registed pid
Go through the linked list, read cpt time of each registered pid, save in the buffer for caller
Return: number of bytes read
*/
static ssize_t mp_read (struct file *file, char __user *buffer, size_t count, loff_t *data)
{
  printk(KERN_ALERT "read function is called!!! %d", *data);
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
    sprintf(temp, "%lu[%lu]: %lu ms, %lu ms\n", entry->pid, entry->task_state, entry->period, entry->processing_time);
    strcpy(buf + offset, temp);
    offset = strlen(buf);
  }
  spin_unlock(&mylock);
  // critical section end
  copied = strlen(buf)+1;
  copy_to_user(buffer, buf, copied);
  kfree(buf);
  printk(KERN_ALERT "READ COUNT COPIED %d\t%d\n", count, copied);
  *data += copied;
  return copied;
}

/**
Called when user register pid to the linked list
Initiate a new block and add it to the linked list
*/

static ssize_t mp_write (struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
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
  }
  else if (buf[0] == 'D') {
    printk("First letter is D.\n");
  }
  else {
    printk("WTF\n");
  }
/*
  int copied;
  struct mp_list* new_node;
  char * buf;
  buf = (char *) kmalloc(count+1,GFP_KERNEL); 
  copied = 0;
  copy_from_user(buf, buffer, count);
  buf[count] = '\0';
  new_node  = kmalloc(sizeof(struct mp_list), GFP_KERNEL);

  if(kstrtol(buf, 10, &(new_node->pid)))
  {
    int i = 0;
    while( buf[i] != '\0' )
    {
      printk("ERROR STR TO LONG: %x\n", *(buf+i));
      i++;
    }
  }
  printk("PID AT BEGINNING %d \n", new_node->pid);
  printk("PID: %d\n", new_node->pid);
  new_node->cpu_time = 1337;
  INIT_LIST_HEAD(&new_node->list);
  // critical section begin
  spin_lock(&mylock);
  list_add(&new_node->list, &head);
  spin_unlock(&mylock);
  // critical section end
  printk("%d \n", ((struct mp_list*)head.next)->cpu_time);
*/
  kfree(buf);
  return count;
}


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


void registration(char* buf) {
  struct mp_task_struct* new_task;
  new_task = (struct mp_task_struct*) kmem_cache_alloc(mp_task_struct_cache, GFP_KERNEL);
  INIT_LIST_HEAD(&new_task->task_node);

  sscanf(buf+3, "%u, %u, %u\n", &new_task->pid, &new_task->period, &new_task->processing_time);
/*
  sscanf(strsep(&tmp, ","), "%u", &new_task->pid);
  sscanf(strsep(&tmp, ","), "%u", &new_task->period);
  sscanf(strsep(&tmp, "\n"), "%u", &new_task->processing_time);
*/
  new_task->task_state = SLEEPING;

  // admission control
  if (!admission_control(new_task)) return;

  // critical section begin
  spin_lock(&mylock);
  list_add(&new_task->task_node, &head);
  spin_unlock(&mylock);
  // critical section end
}

int de_registration(int pid)
{
  // critical section begin
  spin_lock(&mylock);
  struct mp_task_struct *task_to_remove = (struct mp_task_struct *)find_task_by_pid(pid);
  list_del(&(task_to_remove->task_node));
  
  if(running_mptask == task_to_remove)
  {
    running_mptask = NULL;
    wake_up_process(dispatcher);
  }

  kmem_cache_free(mp_task_struct_cache, task_to_remove);
  spin_unlock(&mylock);
  // critical section end
}

/**
Top half interrupt:
setup up a timer for 5 seconds and put the bottom half on the workqueue
*/
void _timer_callback( unsigned long data )
{
   wake_up_process(dispatcher);
   // setup_timer( &timer, _timer_callback, 0 );
   // queue_work(mp_q, &bottom_work);
   // mod_timer( &timer, jiffies + msecs_to_jiffies(5000) );
}

/**
Bottom half interrupt:
go through the linked list of registered pid and update cpu time for each
remove the registerd pid if the job is completed
*/
/*
static void bottom_fn(void *ptr)
{
  struct mp_list *entry;
  struct mp_list *temp_entry;
  // critical section begin
  spin_lock(&mylock);
  list_for_each_entry_safe(entry,temp_entry, &head, list) 
  {
    unsigned long cpu_value;
    if(!get_cpu_use(entry->pid, &cpu_value))
    {
      //success
      entry->cpu_time = cpu_value;
      printk("RECORDED CPU TIME FOR PID %d: %d\n", entry->pid, jiffies_to_msecs(entry->cpu_time));
    }
    else  // job finished, remove from linked list 
    {
      //list_del(&(entry->list));
      printk("ERROR: CAN'T GET CPU USE FOR PID: %d, So this process is now being deleted\n", entry->pid);
    }
  }
  spin_unlock(&mylock);
  // critical section end
}
*/
////////////////////////////
///////////////////////////
int thread_fn() {

  printk(KERN_INFO "In dispatcher thread function");

  // the ktread_should_stop will be changed flag at the module exit, where the kthread_stop() is called. 
  while(!kthread_should_stop()){
      set_current_state(TASK_INTERRUPTIBLE);
      schedule();

      spin_lock(&mylock);
      // set the previous running task state to ready and put to normal schedule
      struct sched_param sparam;
      if (running_mptask !=NULL){
        sparam.sched_priority=0;
        running_mptask->task_state = READY;
        sched_setscheduler(running_mptask->task, SCHED_NORMAL, &sparam);
        running_mptask = NULL;
      } 
      // loop over the task entries and find the ready task with smallest period
      struct mp_task_struct *entry;
      struct mp_task_struct * return_task=NULL;
      long minp=LONG_MAX;
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
      	setup_timer( &return_task->task_timer, _timer_callback, 0 ); 
      	mod_timer( &return_task->task_timer, jiffies + msecs_to_jiffies(return_task->period) ); 
      }     
      spin_unlock(&mylock);
     
      // if no ready task, then goto next loop, which sleeps at the beginning 
      if (return_task ==NULL){
        printk(KERN_INFO "no task in ready state");
        continue;
      }
      
      // wake up the returned task, set the priority and put it to real time scheduling
      wake_up_process(return_task);
      sparam.sched_priority=99;
      sched_setscheduler(return_task, SCHED_FIFO, &sparam);
  }

  return 0;
}

int yield_handle(int pid) 
{
  struct mp_task_struct *task_to_yield = (struct mp_task_struct *)find_task_by_pid(pid);

  task_to_yield->next_period += task_to_yield->next_period==0 ? 
    jiffies + msecs_to_jiffies(task_to_yield->period) :
    msecs_to_jiffies(task_to_yield->period);

  //if only the next period has not start yet
  if(task_to_yield->next_period < jiffies)  return;

  //setup timer
  mod_timer(&(task_to_yield->task_timer), task_to_yield->next_period);
  task_to_yield->task_state = SLEEPING;

  //set current running to null
  //
  
  //wakeup dispatcher and schedule
  wake_up_process(dispatcher);
  set_task_state(task_to_yield->task, TASK_UNINTERRUPTIBLE);
  schedule();

  return 0;
}

static const struct file_operations mp_file = {
  .owner = THIS_MODULE, 
  .read = mp_read,
  .write = mp_write,
};

/**
mp_init - Called when module is loaded
create proc directory and file
set up timer
create workqueue and work struct
*/
int __init mp_init(void)
{
  #ifdef DEBUG
  printk(KERN_ALERT "MP1 MODULE LOADING\n");
  #endif
  proc_dir =  proc_mkdir("mp",NULL);
  proc_create("status",0666, proc_dir, &mp_file);  
  spin_lock_init(&mylock);
  setup_timer( &timer, _timer_callback, 0 ); 
  mod_timer( &timer, jiffies + msecs_to_jiffies(5000) ); 

/*
  mp_q = create_workqueue("mp_queue");
  INIT_WORK(&bottom_work, &bottom_fn);
*/

  mp_task_struct_cache = KMEM_CACHE(mp_task_struct, SLAB_PANIC);

  // create dispatcher kernel thread
  char dispatcher_name[11]="dispatcher";
  printk(KERN_INFO "dispatching thread init");
  dispatcher = kthread_create(thread_fn,NULL,dispatcher_name);

  printk(KERN_ALERT "MP1 MODULE LOADED\n");

  return 0;   
}

/**
mp_exit - Called when module is unloaded
remove proc directory and file
clean the linked list and free all memory used
destroy workqueue and timer
*/
void __exit mp_exit(void)
{
  #ifdef DEBUG
  printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
  #endif
  struct list_head* n = head.next; 
  /*
  struct mp_list* ptr;
  remove_proc_entry("status", proc_dir);
  remove_proc_entry("mp", NULL);

  while (n != &head) {
    ptr = (struct mp_list*) n;
    n = n->next;
    kfree(ptr);
  }
  del_timer( &timer );
  destroy_workqueue(mp_q);
  */
  kmem_cache_destroy(mp_task_struct_cache);

  printk(KERN_ALERT "MP1 MODULE UNLOADED\n");

  // destroy the kernel thread
  int ret;
  ret = kthread_stop(dispatcher);
  if(!ret)
     printk(KERN_INFO "Dispatcher thread stopped");
}

// Register init and exit funtions
module_init(mp_init);
module_exit(mp_exit);
