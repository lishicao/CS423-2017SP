#define LINUX

#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "mp1_given.h"
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/timer.h>

static struct proc_dir_entry *proc_dir;
static struct timer_list timer;

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_ID");
MODULE_DESCRIPTION("CS-423 MP1");

LIST_HEAD(head);

struct mp1_list {
  struct list_head list;
  unsigned long start_time;
  char* pid_ptr;
};

int flag = 1;
#define DEBUG 1
static ssize_t mp1_read (struct file *file, char __user *buffer, size_t count, loff_t *data){
// implementation goes here...
  printk(KERN_ALERT "read function is called!!! %d", count);
  int copied = 0;
  char * buf;
  struct mp1_list *entry;
  int offset = 0;
  if (flag == 0) {
    flag = 1;
    return 0;
  } 
  buf = (char *) kmalloc(1000,GFP_KERNEL); 

  list_for_each_entry(entry, &head, list) {
    strcpy(buf+offset, entry->pid_ptr);
    offset = strlen(buf);
  }

  copied = strlen(buf)+1;
  printk(KERN_ALERT "Size %d\n", copied);
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
  printk(KERN_ALERT "write function is called");
  printk(KERN_ALERT "input count = %d\n", count);
  buf = (char *) kmalloc(count+1,GFP_KERNEL); 
  copied = 0;
  copy_from_user(buf, buffer, count);
  buf[count] = '\0';
  new_node  = kmalloc(sizeof(struct mp1_list), GFP_KERNEL);
  new_node->pid_ptr = buf;
  new_node->start_time = 1337;
  INIT_LIST_HEAD(&new_node->list);
  list_add(&new_node->list, &head);
  printk("%d \n", ((struct mp1_list*)head.next)->start_time);
  struct mp1_list* test;
  test = (struct mp1_list*) (head.next);

  // kfree(buf);
  
  return count;

}

void timer_callback( unsigned long data )
{
    printk( "timer_callback called (%ld).\n", jiffies );
    setup_timer( &timer, timer_callback, 0 );
    mod_timer( &timer, jiffies + msecs_to_jiffies(5000) );
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
   printk(KERN_ALERT "MP1 MODULE LOADING\n");
   #endif
   // Insert your code here ...
  proc_dir =  proc_mkdir("mp1",NULL);
  proc_create("status",0666, proc_dir, &mp1_file);  
  
  setup_timer( &timer, timer_callback, 0 ); 
  mod_timer( &timer, jiffies + msecs_to_jiffies(5000) ); 
   printk(KERN_ALERT "MP1 MODULE LOADED\n");
   return 0;   
}

// mp1_exit - Called when module is unloaded
void __exit mp1_exit(void)
{
  #ifdef DEBUG
  printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
  #endif
  // Insert your code here ...
  struct list_head* n = head.next; 
  struct mp1_list* ptr;
  remove_proc_entry("status", proc_dir);
  remove_proc_entry("mp1", NULL);

  while (n != &head) {
    ptr = (struct mp1_list*) n;
    kfree(ptr->pid_ptr);
    n = n->next;
    kfree(ptr);
  }
  del_timer( &timer );
  printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp1_init);
module_exit(mp1_exit);
