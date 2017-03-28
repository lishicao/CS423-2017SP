#define LINUX

#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "mp3_given.h"
//#include <errno.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/mm.h>

#define DEBUG 1
#define PAGE_NUM 128

/**
static variables and struct
*/
struct cdev chrdev;
static struct proc_dir_entry *proc_dir;
static spinlock_t mylock;
static struct workqueue_struct *wq;
unsigned long * mem_buf;
unsigned long delay;
int mem_buf_ptr = 0;
int majorNumber = 0;

static void mp_work_func(struct work_struct *work);
static int dev_mmap(struct file *filp, struct vm_area_struct *vma);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_10");
MODULE_DESCRIPTION("CS-423 MP3");

LIST_HEAD(head);
DECLARE_DELAYED_WORK(mp_delayed_work, mp_work_func);
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
  return 0;
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



static ssize_t dev_open (struct inode *inode, struct file *filp)
{}
static ssize_t dev_close (struct inode *inode, struct file *filp)
{}

static int dev_mmap(struct file *filp, struct vm_area_struct *vma){
        printk(KERN_INFO "dev_mmap called\n");

        unsigned long pfn;
	char *pos;
        unsigned long start = vma->vm_start;
        unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);

        /* if userspace tries to mmap beyond end of our buffer, fail */
        if (size > PAGE_NUM * PAGE_SIZE)
                return -EINVAL;

        /* start off at the start of the buffer */
        pos = mem_buf;
        printk(KERN_INFO "Before While %d\n", __LINE__);

        /* loop through all the physical pages in the buffer */
        /* Remember this won't work for vmalloc()d memory ! */
        while (size > 0) {
                printk(KERN_INFO "Inside While %d\n", size);

                /* remap a single physical page to the process's vma */
                //TODO!!!!
                //pfn = vmalloc_to_pfn((void *)pos);
                struct page *page;
                page = vmalloc_to_page( pos );

                /* fourth argument is the protection of the map. you might
                 * want to use vma->vm_page_prot instead.
                 */
                //TODO
                int ret = 0;
                if( ( ret = vm_insert_page(vma, start, page) ) < 0 ) 
                {
                  return ret;
                }
                //if (remap_pfn_range(vma, start, pfn, PAGE_SIZE, PAGE_SHARED))
                //        return -EAGAIN;
                start+=PAGE_SIZE;
                pos+=PAGE_SIZE;
                size-=PAGE_SIZE;
        }
	printk(KERN_INFO "dev_mmap finished\n");
        return 0;
}


static const struct file_operations dev_file = {
        .open = NULL,
        .release = NULL,
        .mmap = dev_mmap,
        .owner = THIS_MODULE,
};


/**
register the new task into task list
create a corresponding struct from the slab allocator
if new task passes admission control, add it to the list
*/
void registration(char* buf) {
  printk("enter registration()\n");
  //return 0;
  
  // TODO: potential bug here
  int empty_flag = list_empty(&head);
  printk("EMPTY FLAG: %d\n", empty_flag);

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
  
  if (empty_flag) { // current PCB list is empty
    queue_delayed_work(wq, &mp_delayed_work, delay);
    printk("--> a NEW workqueue JOB is created\n");
  }
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
    cancel_delayed_work(&mp_delayed_work);
    flush_workqueue(wq);
    printk("--> work queue is EMPTY and flushed\n");
  }
  kfree(task_to_remove);
}

/**

*/
static void mp_work_func(struct work_struct *work) {
  unsigned long min_flt, maj_flt, utime, stime;
  unsigned long min_flt_sum, maj_flt_sum, cpu_time_sum;
  return 0;
  // critical section begin  
  spin_lock(&mylock);
  struct mp_task_struct *entry;
  list_for_each_entry(entry, &head, task_node) {
    if (get_cpu_use(entry->pid, &min_flt, &maj_flt, &utime, &stime) == -1) {
      continue;
    }
    min_flt_sum += min_flt;
    maj_flt_sum += maj_flt;
    cpu_time_sum += (utime + stime);
  }
  spin_unlock(&mylock);
  // critical section end

  mem_buf[mem_buf_ptr ++] = jiffies;
  mem_buf[mem_buf_ptr ++] = min_flt_sum;
  mem_buf[mem_buf_ptr ++] = maj_flt_sum;
  mem_buf[mem_buf_ptr ++] = jiffies_to_msecs(cputime_to_jiffies(cpu_time_sum));

  if (mem_buf_ptr > PAGE_NUM * PAGE_SIZE / sizeof(unsigned long)) {
    printk(KERN_ALERT "memory buffer is full!\n");
  }

  queue_delayed_work(wq, &mp_delayed_work, delay);
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

  // TODO potential bug here
  mem_buf = (unsigned long*) vmalloc(PAGE_NUM * PAGE_SIZE);

  if (mem_buf == NULL) {
    printk("WTF: mem_buff is NULL\n");
  }

  wq = create_workqueue("mp_queue");
  delay = msecs_to_jiffies(50);

  cdev_init(&chrdev, &dev_file);
  majorNumber = register_chrdev(0, "MP3", &dev_file);
  printk(KERN_ALERT "MAJOR NUMBER: %d\n", majorNumber);

  //create work_queue
  wq = create_workqueue("mp_queue");

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

  unregister_chrdev(majorNumber, "MP3");

  struct mp_task_struct *entry;
  struct mp_task_struct *temp_entry;

  // handle work queue
  cancel_delayed_work(&mp_delayed_work);
  flush_workqueue(wq);
  destroy_workqueue(wq);

  spin_lock(&mylock);
  //go through the list and detroy the list entry and timer inside of mp task struct
  list_for_each_entry_safe(entry, temp_entry, &head, task_node)
  {
    list_del(&(entry->task_node));
  }
  spin_unlock(&mylock);
  printk(KERN_ALERT "MP3 MODULE UNLOADED\n");

  vfree(mem_buf);
  //remove proc entry
  remove_proc_entry("status", proc_dir);
  remove_proc_entry("mp", NULL);
}

// Register init and exit funtions
module_init(mp_init);
module_exit(mp_exit);

