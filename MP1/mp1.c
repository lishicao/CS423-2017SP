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

static struct proc_dir_entry *proc_dir;
static struct timer_list timer;
static struct work_struct bottom_work;

struct workqueue_struct *mp1_q;
static spinlock_t mylock;

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_10");
MODULE_DESCRIPTION("CS-423 MP1");

LIST_HEAD(head);

struct mp1_list {
  struct list_head list;
  unsigned long cpu_time;
  long pid;
};

int flag = 1;
#define DEBUG 1
static ssize_t mp1_read (struct file *file, char __user *buffer, size_t count, loff_t *data){
// implementation goes here...
//  printk(KERN_ALERT "read function is called!!! %d", count);
  int copied = 0;
  char * buf;
  struct mp1_list *entry;
  int offset = 0;
  if (flag == 0) {
    flag = 1;
    return 0;
  } 
  buf = (char *) kmalloc(2048,GFP_KERNEL); 
  spin_lock(&mylock);
  list_for_each_entry(entry, &head, list) {
    char temp[256];
    sprintf(temp, "%lu: %lu ms\n", entry->pid, jiffies_to_msecs(entry->cpu_time));
    strcpy(buf + offset, temp);
    offset = strlen(buf);
  }
  spin_unlock(&mylock);

  copied = strlen(buf)+1;
//  printk(KERN_ALERT "Size %d\n", copied);
  copy_to_user(buffer, buf, copied);
  kfree(buf);
  printk(KERN_ALERT "%d\t%d\n", count, copied);
  flag = 0;
  return copied;

}
static ssize_t mp1_write (struct file *file, const char __user *buffer, size_t count, loff_t *data){ // implementation goes here...

  int copied;
  struct mp1_list* new_node;
  char * buf;
//  printk(KERN_ALERT "write function is called");
//  printk(KERN_ALERT "input count = %d\n", count);
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
  spin_lock(&mylock);
  list_add(&new_node->list, &head);
  spin_unlock(&mylock);
  printk("%d \n", ((struct mp1_list*)head.next)->cpu_time);
  //struct mp1_list* test;
  //test = (struct mp1_list*) (head.next);

  kfree(buf);
  
  return count;

}

void timer_callback( unsigned long data )
{
//    printk( "timer_callback called (%ld).\n", jiffies );
    setup_timer( &timer, timer_callback, 0 );

    queue_work(mp1_q, &bottom_work);
    mod_timer( &timer, jiffies + msecs_to_jiffies(5000) );
}

static void bottom_fn(void *ptr)
{
//  printk("bottom half func called!!");
  struct mp1_list *entry;
  struct mp1_list *temp_entry;
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
    else 
    {
      list_del(&(entry->list));
      printk("ERROR: CAN'T GET CPU USE FOR PID: %d, So this process is now being deleted\n", entry->pid);
    }
  }
  spin_unlock(&mylock);
}

static const struct file_operations mp1_file = {
  .owner = THIS_MODULE, 
  .read = mp1_read,
  .write = mp1_write,
};

// mp1_init - Called when module is loaded
int __init mp1_init(void)
{
   #ifdef DEBUG
//   printk(KERN_ALERT "MP1 MODULE LOADING\n");
   #endif
   // Insert your code here ...
  proc_dir =  proc_mkdir("mp1",NULL);
  proc_create("status",0666, proc_dir, &mp1_file);  
  spin_lock_init(&mylock);
  setup_timer( &timer, timer_callback, 0 ); 
  mod_timer( &timer, jiffies + msecs_to_jiffies(5000) ); 
  
  mp1_q = create_singlethread_workqueue("mp_queue");
  INIT_WORK(&bottom_work, &bottom_fn);
  printk(KERN_ALERT "MP1 MODULE LOADED\n");

  return 0;   
}

// mp1_exit - Called when module is unloaded
void __exit mp1_exit(void)
{
  #ifdef DEBUG
//  printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
  #endif
  // Insert your code here ...
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
