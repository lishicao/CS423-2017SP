// This is only draft of mmap

struct vm_operations_struct mmap_vm_ops =
{
    .open =     mmap_open,
    .close =    mmap_close,
    .fault =    mmap_fault,    
};

static int dev_mmap(struct file *filp, struct vm_area_struct *vma){

	vma->vm_ops = &mmap_vm_ops;
    	vma->vm_flags |= VM_RESERVED;    
    	vma->vm_private_data = filp->private_data;
    	mmap_open(vma);

	return 0;
}
