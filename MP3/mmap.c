// This is only draft of mmap
#include "errno.h"


static int dev_mmap(struct file *filp, struct vm_area_struct *vma){

	printk(KERN_INFO "dev_mmap called\n");
	
	unsigned long pfn,pos;
	unsigned long start = (unsigned long)vma->vm_start; 
	unsigned long size = (unsigned long)(vma->vm_end-vma->vm_start); 

	/* if userspace tries to mmap beyond end of our buffer, fail */ 
        if (size>MMT_BUF_SIZE)
                return -EINVAL;
 
	/* start off at the start of the buffer */ 
        pos=(unsigned long) buffer;
 
	/* loop through all the physical pages in the buffer */ 
	/* Remember this won't work for vmalloc()d memory ! */
        while (size > 0) {
		/* remap a single physical page to the process's vma */ 
		pfn = vmalloc_to_pfn((void *)pos);
		/* fourth argument is the protection of the map. you might
		 * want to use vma->vm_page_prot instead.
		 */
                if (remap_page_range(vma, start, pfn, PAGE_SIZE, PAGE_SHARED))
                        return -EAGAIN;
                start+=PAGE_SIZE;
                pos+=PAGE_SIZE;
                size-=PAGE_SIZE;
        }

        return 0;	
}

