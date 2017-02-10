#define LINUX

#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "mp1_given.h"
#include <linux/list.h>
#include <asm/uaccess.h>

static struct proc_dir_entry *proc_dir;

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_ID");
MODULE_DESCRIPTION("CS-423 MP1");

int flag = 1;
LIST_HEAD(head);

struct mp1_list {
  struct list_head list;
  int start_time;
  char* pid_ptr;
};

#define DEBUG 1
static ssize_t mp1_read (struct file *file, char __user *buffer, size_t count, loff_t *data){
// implementation goes here...
 
  printk(KERN_ALERT "read function is called!!!");
  int copied = 0;
  char * buf;
  buf = (char *) kmalloc(count,GFP_KERNEL); 
  copied = strlen(buf)+1;
  printk(KERN_ALERT "Size %d\n", copied);
  copy_to_user(buffer, buf, copied);
  kfree(buf);
  printk(KERN_ALERT "%d\t%d\n", count, copied);
  return copied;

}
static ssize_t mp1_write (struct file *file, const char __user *buffer, size_t count, loff_t *data){ // implementation goes here...

  printk(KERN_ALERT "write function is called");
  int copied;
  char * buf;
  buf = (char *) kmalloc(count+1,GFP_KERNEL); 
  copied = 0;
  copy_from_user(buffer, buf, count);
  LIST_HEAD(node);
  mp1_list* new_node = kmalloc(sizeof(list_head)+sizeof(int)+sizeof(char*), GFP_KERNEL);
  new_node->pid_ptr = buf;
  new_node->list = node;
  new_node->start_time = 1337;
  list_add(node, head);
  printk(KERN_ALERT "%x\n", *buffer);
  // kfree(buf);
  
  return 1;

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
  remove_proc_entry("status", proc_dir);
  remove_proc_entry("mp1", NULL);
  list_head n = head.next; 
  while (n != &head) {
    my_list* ptr = list_entry(n, mp1_list, list);
    kfree(ptr->pid_ptr);
    n = list_entry(n->next, mp1_list, list);
    kfree(ptr);
  }

  printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp1_init);
module_exit(mp1_exit);
