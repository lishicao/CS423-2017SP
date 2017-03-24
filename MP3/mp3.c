#define LINUX

#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "mp3_given.h"
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>



#define DEBUG 1
/**
static variables and struct
*/
static struct proc_dir_entry *proc_dir;
static spinlock_t mylock;
static struct workqueue_struct *wq;

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_10");
MODULE_DESCRIPTION("CS-423 MP3");

LIST_HEAD(head);

/**
Augmented Task Struct
*/
struct mp_task_struct
{ 
  struct task_struct *task;
  struct list_head task_node;
  
  unsigned long utilization;
  unsigned long major_fault;
  unsigned long minor_fault;
  int pid;
};



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
//    sprintf(temp, "%d[%d]: %d ms, %d ms\n", entry->pid, entry->task_state, entry->period, entry->processing_time);
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
    printk("--Registration branch--\n");
  }
  else if (buf[0] == 'U') {
    unregistration(buf);
    printk("--Un-registration branch--\n");
  }
  else {
    printk("WTF\n");
    printk("%x\n", buf[0]);
  }
  kfree(buf);
  return count;
}

/**
register the new task into task list
create a corresponding struct from the slab allocator
if new task passes admission control, add it to the list
*/
void registration(char* buf) {
  printk("enter registration()\n");
  // TODO: potential bug here
  if (list_empty(&head)) { // current PCB list is empty
    wq = create_workqueue("mp_queue");
    printk("--> a NEW work queue is created\n");
  }

  // alloc memory for the new struct
  struct mp_task_struct* new_task;
  new_task = (struct mp_task_struct*) kmalloc(sizeof(struct mp_task_struct), GFP_KERNEL);
  INIT_LIST_HEAD(&new_task->task_node);
  
  //init variables
  sscanf(buf+3, "%d\n", &new_task->pid);
  new_task->utilization = 0;
  new_task->major_fault = 0;
  new_task->minor_fault = 0;
  new_task->task = find_task_by_pid(new_task->pid);
  
  printk("REGISTER PID %d created mp_task_struct address is %x\n", new_task->pid, new_task);
  
  // critical section begin
  spin_lock(&mylock);
  list_add(&new_task->task_node, &head);
  spin_unlock(&mylock);
  // critical section end
  printk(KERN_ALERT "registratin succesfully finished\n");
} 

/**
unregister a task
if this task is running, preempt it
go through the task list, find the task by pid and remove it from list
*/
void unregistration(char* buf)
{
  printk("enter unregistration()\n");
  // read in pid from buffer
  int pid;
  sscanf(buf+3, "%d\n", &pid);
  printk(KERN_ALERT "DE-REGISTER PID %d\n", pid);
  // critical section begin
  spin_lock(&mylock);
  struct mp_task_struct *task_to_remove = __get_task_by_pid(pid);
  list_del(&(task_to_remove->task_node));
  spin_unlock(&mylock);
  // critical section end

  // TODO potential bug here
  if (list_empty(&head)) {  // PCB list is empty after deletion
    destroy_workqueue(wq);
    printk("--> work queue is EMPTY and destroyed\n");
  }

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
  printk(KERN_ALERT "MP3 MODULE LOADING\n");
  #endif
  proc_dir =  proc_mkdir("mp",NULL);
  proc_create("status",0666, proc_dir, &mp_file);
  spin_lock_init(&mylock);
  
  printk(KERN_ALERT "MP3 MODULE LOADED\n");
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
  printk(KERN_ALERT "MP3 MODULE UNLOADING\n");
  #endif

  struct mp_task_struct *entry;
  struct mp_task_struct *temp_entry;

  spin_lock(&mylock);
  //go through the list and detroy the list entry and timer inside of mp task struct
  list_for_each_entry_safe(entry, temp_entry, &head, task_node)
  {
    list_del(&(entry->task_node));
  }
  spin_unlock(&mylock);
  printk(KERN_ALERT "MP3 MODULE UNLOADED\n");

  //remove proc entry
  remove_proc_entry("status", proc_dir);
  remove_proc_entry("mp", NULL);
}

// Register init and exit funtions
module_init(mp_init);
module_exit(mp_exit);

