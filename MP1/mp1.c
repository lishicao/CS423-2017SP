#define LINUX

#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "mp1_given.h"
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>


/**
static variables and struct
*/
static struct proc_dir_entry *proc_dir;
static struct timer_list timer;
static struct work_struct bottom_work;

struct workqueue_struct *mp1_q;
static spinlock_t mylock;

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_10");
MODULE_DESCRIPTION("CS-423 MP1");

LIST_HEAD(head);

/**
Linked list struct to store registered pid and corresponding cpt time
*/
struct mp1_list {
  struct list_head list;
  unsigned long cpu_time;
  long pid;
};

int flag = 1;
#define DEBUG 1

/**
Called when user request cpu time about registed pid
Go through the linked list, read cpt time of each registered pid, save in the buffer for caller
Return: number of bytes read
*/
static ssize_t mp1_read (struct file *file, char __user *buffer, size_t count, loff_t *data)
{
  printk(KERN_ALERT "read function is called!!! %d", *data);
  if(*data>0)
    return 0;

  int copied = 0;
  char * buf;
  struct mp1_list *entry;
  int offset = 0;

  buf = (char *) kmalloc(2048,GFP_KERNEL); 
  // critical section begin
  spin_lock(&mylock);
  list_for_each_entry(entry, &head, list) {
    char temp[256];
    sprintf(temp, "%lu: %lu ms\n", entry->pid, jiffies_to_msecs(entry->cpu_time));
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
static ssize_t mp1_write (struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
  int copied;
  struct mp1_list* new_node;
  char * buf;
  buf = (char *) kmalloc(count+1,GFP_KERNEL); 
  copied = 0;
  copy_from_user(buf, buffer, count);
  buf[count] = '\0';
  new_node  = kmalloc(sizeof(struct mp1_list), GFP_KERNEL);

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
  printk("%d \n", ((struct mp1_list*)head.next)->cpu_time);

  kfree(buf);
  return count;
}

/**
Top half interrupt:
setup up a timer for 5 seconds and put the bottom half on the workqueue
*/
void _timer_callback( unsigned long data )
{
    setup_timer( &timer, _timer_callback, 0 );
    queue_work(mp1_q, &bottom_work);
    mod_timer( &timer, jiffies + msecs_to_jiffies(5000) );
}

/**
Bottom half interrupt:
go through the linked list of registered pid and update cpu time for each
remove the registerd pid if the job is completed
*/
static void bottom_fn(void *ptr)
{
  struct mp1_list *entry;
  struct mp1_list *temp_entry;
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
      list_del(&(entry->list));
      printk("ERROR: CAN'T GET CPU USE FOR PID: %d, So this process is now being deleted\n", entry->pid);
    }
  }
  spin_unlock(&mylock);
  // critical section end
}

static const struct file_operations mp1_file = {
  .owner = THIS_MODULE, 
  .read = mp1_read,
  .write = mp1_write,
};

/**
mp1_init - Called when module is loaded
create proc directory and file
set up timer
create workqueue and work struct
*/
int __init mp1_init(void)
{
  #ifdef DEBUG
  printk(KERN_ALERT "MP1 MODULE LOADING\n");
  #endif
  proc_dir =  proc_mkdir("mp1",NULL);
  proc_create("status",0666, proc_dir, &mp1_file);  
  spin_lock_init(&mylock);
  setup_timer( &timer, _timer_callback, 0 ); 
  mod_timer( &timer, jiffies + msecs_to_jiffies(5000) ); 
  
  mp1_q = create_workqueue("mp_queue");
  INIT_WORK(&bottom_work, &bottom_fn);
  printk(KERN_ALERT "MP1 MODULE LOADED\n");

  return 0;   
}

/**
mp1_exit - Called when module is unloaded
remove proc directory and file
clean the linked list and free all memory used
destroy workqueue and timer
*/
void __exit mp1_exit(void)
{
  #ifdef DEBUG
  printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
  #endif
  struct list_head* n = head.next; 
  struct mp1_list* ptr;
  remove_proc_entry("status", proc_dir);
  remove_proc_entry("mp1", NULL);

  while (n != &head) {
    ptr = (struct mp1_list*) n;
    n = n->next;
    kfree(ptr);
  }
  del_timer( &timer );
  destroy_workqueue(mp1_q);
  printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp1_init);
module_exit(mp1_exit);
