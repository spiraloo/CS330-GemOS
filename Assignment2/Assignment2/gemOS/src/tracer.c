#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
#include<tracer.h>


///////////////////////////////////////////////////////////////////////////
//// 		Start of Trace buffer functionality 		      /////
///////////////////////////////////////////////////////////////////////////

int is_valid_mem_range(unsigned long buff, u32 count, int access_bit) {
    struct exec_context *current = get_current_ctx();
    int read_b = access_bit & 0x1;
    int write_b = (access_bit>>1) & 0x1;
    int exec_b = (access_bit>>2) & 0x1;
    printk("%d %d %d \n", read_b, write_b,exec_b);

    if((buff >= current->mms[MM_SEG_CODE].start) && (buff+count <=current->mms[MM_SEG_CODE].next_free-1)){
        if(read_b==1 && write_b==0){

            printk("inside seg_code\n");
            return 0;
        }
        else{
            return -1;
        }
    }

    if((buff >= current->mms[MM_SEG_RODATA].start) && (buff+count <= current->mms[MM_SEG_RODATA].next_free-1)){
        if(read_b==1 && write_b==0){
            printk("inside seg_rodata\n");
            return 0;
        }
        else{
            return -1;
        }
    }

    if((buff >= current->mms[MM_SEG_DATA].start) && (buff+count <=current->mms[MM_SEG_DATA].next_free-1)){
        if((read_b==1 && write_b==1)){
            printk("inside seg_data\n");
            return 0;
        }
        else{
            return -1;
        }
    }

    if((buff >= current->mms[MM_SEG_STACK].start) && (buff+count <=current->mms[MM_SEG_STACK].next_free-1)){
        if((read_b==1 && write_b==1 )){
            printk("inside seg_stack\n");
            return 0;
        }
        else{
            return -1;
        }
    }

    struct vm_area *vma = current->vm_area;
    while (vma) {
        if (buff >= vma->vm_start && buff + count <= vma->vm_end) {
            if ( vma->access_flags != access_bit) {
                return -1; // Access permissions do not match
            }
            printk("inside vma\n");
            return 0; // Valid memory range
        }
        vma = vma->vm_next;
    }
   return 0;
}

long trace_buffer_close(struct file *filep)
{
	if(filep != NULL)
	{
		if(filep->trace_buffer != NULL)
		{
			if(filep->trace_buffer->buffer != NULL)
			{
				os_page_free(USER_REG, filep->trace_buffer->buffer);
				filep->trace_buffer->buffer = NULL;	
			}
			os_page_free(USER_REG, filep->trace_buffer);
			filep->trace_buffer = NULL;
		}
		if(filep->fops != NULL)
		{
			os_page_free(USER_REG, filep->fops);
			filep->fops = NULL;
		}

		os_page_free(USER_REG, filep);
		filep = NULL;

		return 0;
	}
	else{
		return -EINVAL;
	}	
}


int trace_buffer_read(struct file *filep, char *buff, u32 count) {
    if(is_valid_mem_range((unsigned long)buff, count, filep->mode)){
        return -EBADMEM;
    }
    struct trace_buffer_info *trace_buffer = filep->trace_buffer;
	if(filep==NULL || buff==NULL){
		return -EINVAL;
	}
    // Calculate the available data in the trace buffer

    //int available_data = (trace_buffer->write_offset >= trace_buffer->read_offset) ?(trace_buffer->write_offset - trace_buffer->read_offset) :(4096 - (trace_buffer->read_offset) + (trace_buffer->write_offset));
    printk("size filled before read is  : %d\n", trace_buffer->size_filled);
    int bytes_to_read=0;
    bytes_to_read = (count < trace_buffer->size_filled) ? count : trace_buffer->size_filled;
    trace_buffer->size_filled-=bytes_to_read;

    printk("size filled after read is  : %d\n", trace_buffer->size_filled);


    // Copy data from the trace buffer to buff
    for (int i = 0; i < bytes_to_read; i++) {
        *(buff+i) = trace_buffer->buffer[trace_buffer->read_offset];
        trace_buffer->read_offset = ((trace_buffer->read_offset) + 1) % 4096;
    }

    return bytes_to_read;
}


int trace_buffer_write(struct file *filep, char *buff, u32 count) {
    
    if(is_valid_mem_range((unsigned long)buff, count, filep->mode)){
        return -EBADMEM;
    }
	if(filep==NULL || buff==NULL){
		return -EINVAL;
	}
    struct trace_buffer_info *trace_buffer = filep->trace_buffer;


	
    // Calculate the available space in the trace buffer
    int available_space = (trace_buffer->read_offset <= trace_buffer->write_offset) ? (4096 - (trace_buffer->write_offset) + (trace_buffer->read_offset)) :((trace_buffer->read_offset) - (trace_buffer->write_offset));

   if(available_space == TRACE_BUFFER_MAX_SIZE)
   {
    if(trace_buffer->size_filled !=0)
        available_space = 0;
   }

	if (available_space <= 0) {
        return 0; // Trace buffer is full, nothing to write
    }

    // Calculate how many bytes can be written
    int bytes_to_write = (count < available_space) ? count : available_space;
	
    printk("byes to write is : %d\n", bytes_to_write);
    // Copy data from buff to the trace buffer
    for (int i = 0; i < bytes_to_write; i++) {
        trace_buffer->buffer[trace_buffer->write_offset] = *(buff+i);
        trace_buffer->write_offset = ((trace_buffer->write_offset) + 1) % 4096;
    }
    printk("size filled is in write  : %d\n", trace_buffer->size_filled);
    trace_buffer->size_filled+=bytes_to_write;
    printk("size filled is in write  : %d\n", trace_buffer->size_filled);
    return bytes_to_write;
}


int sys_create_trace_buffer(struct exec_context *current, int mode) {
    // Find a free file descriptor in the files array
	int fd=0;
	
	if(mode!=O_READ && mode!=O_WRITE && mode!=O_RDWR){
		return -EINVAL;
	}
	for (fd = 0; fd < MAX_OPEN_FILES; fd++) {
        if (current->files[fd] == NULL) {
            break;
        }
    }
    
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return -EINVAL; // No free file descriptor available
    }

	// Create a file object and set its fields
	struct file *filep = (struct file *)os_page_alloc(USER_REG);
    if (filep == NULL) {
        return -ENOMEM; // Memory allocation failed
    }
	filep->mode=mode;
	filep->ref_count=1;
	filep->type = TRACE_BUFFER;
    filep->offp=0;
    // Allocate memory for the trace buffer
    struct trace_buffer_info *trace_buffer = (struct trace_buffer_info *)os_page_alloc(USER_REG);
    if (trace_buffer == NULL) {
		os_free(filep, sizeof(struct file));
        return -ENOMEM; // Memory allocation failed
    }

    // Initialize trace_buffer fields as needed
    trace_buffer->buffer = (char*)os_page_alloc(USER_REG);
    trace_buffer->write_offset = 0;
    trace_buffer->read_offset = 0;
    trace_buffer->size_filled=0;
    
    // printk("%d\n", trace_buffer->write_offset);
    // printk("%d\n", trace_buffer->read_offset);
    // Allocate memory for the fileops object and set function pointers
    // struct fileops *fops = (struct fileops *)os_page_alloc(USER_REG);
    // if (fops == NULL) {
    //     os_free(trace_buffer, sizeof(struct trace_buffer_info));
	// 	os_free(filep, sizeof(struct file));
    //     return -ENOMEM; // Memory allocation failed
    // }
    struct fileops *fops = (struct fileops*)os_page_alloc(USER_REG);
    fops->read = trace_buffer_read;
    fops->write = trace_buffer_write;
    fops->close = trace_buffer_close;
    filep->trace_buffer = trace_buffer;
    filep->fops = fops;

    // Set the file descriptor entry in the files array
    current->files[fd] = filep;

    // Return the allocated file descriptor
    return fd;
}

///////////////////////////////////////////////////////////////////////////
//// 		Start of strace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////


int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4)
{
    printk("inside perform tracing\n");
    struct exec_context *current = get_current_ctx();
	// printk("THE CURRENT SD MD BASE COUNT:%d\n",current->st_md_base->count);
	// printk("THE CURRENT SD MD BASE STRACE_FD:%d\n",current->st_md_base->strace_fd);
	// printk("THE CURRENT SD MD TRACING MODE:%d\n",current->st_md_base->tracing_mode);

	if (!current->st_md_base) {
		return 0; // Tracing not started
	}
	if(syscall_num==SYSCALL_END_STRACE){
		printk("end_called\n");
		return 0;
	}
	// Check if the current process is being traced
	if (!current->st_md_base->is_traced) {
		return 0; // Tracing not started
	}

	// Check if the provided syscall number is valid (you may need to define a list of valid syscall numbers)
	
	if(current->st_md_base->tracing_mode == FILTERED_TRACING){
		struct strace_info* current_trace_info = current->st_md_base->next;
		int k=0;
		//printk("THE VALUE OF SYSCALL IS : %d \n", current_trace_info->syscall_num);
		while (current_trace_info) {
			if (current_trace_info->syscall_num == syscall_num) {
				//printk("CURRENT TRACE SYSCALL NUMBER : %d \n", current_trace_info->syscall_num);
				k++;
				break;
			}
			current_trace_info = current_trace_info->next;
		}

		if (k==0) {
			printk("SYSCALL NUMBER NOT FOUND\n");
			return 0; // Syscall not traced
		}

	}

	// Check if the trace buffer is full
	struct file *filep = current->files[current->st_md_base->strace_fd];
	struct trace_buffer_info *trace_buffer = filep->trace_buffer;
	if (trace_buffer->size_filled == TRACE_BUFFER_MAX_SIZE) {
		return 0; // Trace buffer is full
	}

	// check number of parameters given 
	int num_of_params = 0;

	switch ((int)syscall_num)
	{
		case SYSCALL_EXIT:
			num_of_params = 1;
			break;
		case SYSCALL_GETPID:
			num_of_params = 0;
			break;
		case SYSCALL_GETPPID:
			num_of_params = 0;
			break;
		case SYSCALL_EXPAND:
			num_of_params = 2;
			break;
		case SYSCALL_SHRINK:
			num_of_params = 2;
			break;
		case SYSCALL_ALARM:
			num_of_params = 1;
			break;
		case SYSCALL_SLEEP:
			num_of_params = 1;
			break;
		case SYSCALL_SIGNAL:
			num_of_params = 2;
			break;
		case SYSCALL_CLONE:
			num_of_params = 2;
			break;
		case SYSCALL_FORK:
			num_of_params = 0;
			break;
		case SYSCALL_STATS:
			num_of_params = 0;
			break;
		case SYSCALL_CONFIGURE:
			num_of_params = 1;
			break;
		case SYSCALL_PHYS_INFO:
			num_of_params = 0;
			break;
		case SYSCALL_DUMP_PTT:
			num_of_params = 1;
			break;
		case SYSCALL_CFORK:
			num_of_params = 0;
			break;
		case SYSCALL_MMAP:
			num_of_params = 4;
			break;
		case SYSCALL_MUNMAP:
			num_of_params = 2;
			break;
		case SYSCALL_MPROTECT:
			num_of_params = 3;
			break;
		case SYSCALL_PMAP:
			num_of_params = 1;
			break;
		case SYSCALL_VFORK:
			num_of_params = 0;
			break;
		case SYSCALL_GET_USER_P:
			num_of_params = 0;
			break;
		case SYSCALL_GET_COW_F:
			num_of_params = 0;
			break;
		case SYSCALL_OPEN:
			num_of_params = 2;
			break;
		case SYSCALL_READ:
			num_of_params = 3;
			break;
		case SYSCALL_WRITE:
			num_of_params = 3;
			break;
		case SYSCALL_DUP:
			num_of_params = 1;
			break;
		case SYSCALL_DUP2:
			num_of_params = 2;
			break;
		case SYSCALL_CLOSE:
			num_of_params = 1;
			break;
		case SYSCALL_LSEEK:
			num_of_params = 3;
			break;
		case SYSCALL_FTRACE:
			num_of_params = 4;
			break;
		case SYSCALL_TRACE_BUFFER:
			num_of_params = 1;
			break;
		case SYSCALL_START_STRACE:
			num_of_params = 2;
			break;
		case SYSCALL_END_STRACE:
			num_of_params = 0;
			break;
		case SYSCALL_READ_STRACE:
			num_of_params = 3;
			break;
		case SYSCALL_STRACE:
			num_of_params = 2;
			break;
		case SYSCALL_READ_FTRACE:
			num_of_params = 3;
			break;
		default:
			return 0;
			break;
	}


	//we have to fill in 8 byte chunks

	*(u64*)(trace_buffer->buffer + trace_buffer->write_offset) = syscall_num;
	trace_buffer->write_offset = ((trace_buffer->write_offset) + 8) % 4096;
	trace_buffer->size_filled+=8;

	if(num_of_params == 0){
		
		printk("trace buffer size in perform : %d\n",trace_buffer->size_filled);
		return 0;
	}
	else if( num_of_params ==1){
		*(u64*)(trace_buffer->buffer + trace_buffer->write_offset) = param1;
		trace_buffer->write_offset = ((trace_buffer->write_offset) + 8) % 4096;
		trace_buffer->size_filled+=8;
        printk("trace buffer size in perform : %d\n",trace_buffer->size_filled);
		return 0;
	}
	else if(num_of_params ==2 ){
		*(u64*)(trace_buffer->buffer + trace_buffer->write_offset) = param1;
		trace_buffer->write_offset = ((trace_buffer->write_offset) + 8) % 4096;
		trace_buffer->size_filled+=8;
		*(u64*)(trace_buffer->buffer + trace_buffer->write_offset) = param2;
		trace_buffer->write_offset = ((trace_buffer->write_offset) + 8) % 4096;
		trace_buffer->size_filled+=8;
        printk("trace buffer size in perform  : %d\n",trace_buffer->size_filled);
		return 0;
	}
	else if(num_of_params == 3){
		*(u64*)(trace_buffer->buffer + trace_buffer->write_offset) = param1;
		trace_buffer->write_offset = ((trace_buffer->write_offset) + 8) % 4096;
		trace_buffer->size_filled+=8;
		*(u64*)(trace_buffer->buffer + trace_buffer->write_offset) = param2;
		trace_buffer->write_offset = ((trace_buffer->write_offset) + 8) % 4096;
		trace_buffer->size_filled+=8;
		*(u64*)(trace_buffer->buffer + trace_buffer->write_offset) = param3;
		trace_buffer->write_offset = ((trace_buffer->write_offset) + 8) % 4096;
		trace_buffer->size_filled+=8;
        printk("trace buffer size in perform  : %d\n",trace_buffer->size_filled);
		return 0;
	}
	else if(num_of_params == 4){
		*(u64*)(trace_buffer->buffer + trace_buffer->write_offset) = param1;
		trace_buffer->write_offset = ((trace_buffer->write_offset) + 8) % 4096;
		trace_buffer->size_filled+=8;
		*(u64*)(trace_buffer->buffer + trace_buffer->write_offset) = param2;
		trace_buffer->write_offset = ((trace_buffer->write_offset) + 8) % 4096;
		trace_buffer->size_filled+=8;
		*(u64*)(trace_buffer->buffer + trace_buffer->write_offset) = param3;
		trace_buffer->write_offset = ((trace_buffer->write_offset) + 8) % 4096;
		trace_buffer->size_filled+=8;
		*(u64*)(trace_buffer->buffer + trace_buffer->write_offset) = param4;
		trace_buffer->write_offset = ((trace_buffer->write_offset) + 8) % 4096;
		trace_buffer->size_filled+=8;

        printk("trace buffer size in perform : %d\n",trace_buffer->size_filled);
		return 0;
	}
    printk("---return kaam nhi kar rha-----\n");
}

int sys_strace(struct exec_context *current, int syscall_num, int action) {
	
	// Check if the provided syscall number is valid (you may need to define a list of valid syscall numbers
	printk("inside strace\n");
    if (action != ADD_STRACE && action != REMOVE_STRACE) {
		printk("invalid action\n");
        return -EINVAL; // Invalid action
    }


	if(!current->st_md_base){
		// Initialize system call tracing for the current process
		current->st_md_base = (struct strace_head*)os_page_alloc(USER_REG);
		if (current->st_md_base == NULL) {
			return -ENOMEM; // Memory allocation failed
		}
		// Enable system call tracing
		current->st_md_base->strace_fd=0;
		current->st_md_base->count=0;
		current->st_md_base->tracing_mode=0;
		current->st_md_base->next=NULL;
		current->st_md_base->last=NULL;
		current->st_md_base->is_traced = 0;

		// return 0; 
	}
	
    if (action == ADD_STRACE) {

		printk("inside add strace \n");
		struct strace_info* current_trace_info = current->st_md_base->next;

		if (syscall_num < 0) {
            return -EINVAL; // Invalid syscall number
        }

		while (current_trace_info) {
			if (current_trace_info->syscall_num == syscall_num) {
				return -EINVAL; // Syscall already traced
			}
			current_trace_info = current_trace_info->next;
		}
		
		if(current->st_md_base->count==STRACE_MAX){
			return -EINVAL;
		}
        // Check if the provided syscall number is valid (you may need to define a list of valid syscall numbers)
        
		printk("syscall num is %d\n", syscall_num);
        // Add syscall_num to the list of traced system calls
        struct strace_info* new_trace_info = (struct strace_info*)os_page_alloc(USER_REG);
        if (new_trace_info == NULL) {
			printk("memory allocation failed\n");
            return -ENOMEM;
        }
        new_trace_info->syscall_num = syscall_num;
        new_trace_info->next = NULL;
		printk("syscall num is %d\n", new_trace_info->syscall_num);

        if (!current->st_md_base->next) {
			current->st_md_base->next = new_trace_info;
			current->st_md_base->last = new_trace_info;
		} else {
			current->st_md_base->last->next = new_trace_info;
			current->st_md_base->last = new_trace_info;
		}
		printk("VALUE OF NEXT SYSCALLS : %d \n", current->st_md_base->next->syscall_num);
        printk("VALUE OF LAST SYSCALLS : %d \n", current->st_md_base->last->syscall_num);
		current->st_md_base->count++;
		printk("count is %d\n", current->st_md_base->count);
    } else if (action == REMOVE_STRACE) {
		printk("inside remove strace\n");
        // Remove syscall_num from the list of traced system calls

        struct strace_info* current_trace_info = current->st_md_base->next;
        struct strace_info* previous_trace_info = NULL;

        while (current_trace_info) {
            if (current_trace_info->syscall_num == syscall_num) {
                // Found the syscall to remove
                if (previous_trace_info) {
                    previous_trace_info->next = current_trace_info->next;
                } else {
                    current->st_md_base->next = current_trace_info->next;
                }

                os_page_free(USER_REG, current_trace_info);
                current->st_md_base->count--;
                break;
            }

            previous_trace_info = current_trace_info;
            current_trace_info = current_trace_info->next;
        }
    }
	
    return 0; // Success
}


int sys_read_strace(struct file *filep, char *buff, u64 count)
{	
	
	struct trace_buffer_info* trace_buffer = filep->trace_buffer;
	printk("inside read strace\n");
	// check the size of data in trace buffer
	// int available_data = trace_buffer->size_filled;
	// printk("available data is: %d\n", available_data);
    int to_print=0;
	// if(available_data == 0){
	// 	return 0;
	// }
	// here count is number of syscalls to be read and not the number of bytes to be read
    
    printk("trace buffer in read strace : %d\n",trace_buffer->size_filled);
	for(int i=0;i<count;i++){
		// read the syscall number
		*(u64*)(buff) = *(u64*)(trace_buffer->buffer + trace_buffer->read_offset);

        // printk("read offset before is : %d\n ", trace_buffer->read_offset);
        // printk("trace buffer size before : %d\n",trace_buffer->size_filled);
        // printk("buffer filling starts\n");
        // printk("1st element in trace buffer : %x\n ", *(u64*)(buff+i*8));
		trace_buffer->read_offset = ((trace_buffer->read_offset) + 8) % 4096;
		trace_buffer->size_filled-=8;
        // printk("trace buffer size after : %d\n",trace_buffer->size_filled);
        // printk("read offset after is : %d\n ", trace_buffer->read_offset);
		//available_data-=8;
        
		// read the parameters
		int num_of_params = 0;
		switch ((int)*(u64*)(buff))
		{
			case SYSCALL_EXIT:
				num_of_params = 1;
				break;
			case SYSCALL_GETPID:
				num_of_params = 0;
				break;
			case SYSCALL_GETPPID:
				num_of_params = 0;
				break;
			case SYSCALL_EXPAND:
				num_of_params = 2;
				break;
			case SYSCALL_SHRINK:
				num_of_params = 2;
				break;
			case SYSCALL_ALARM:
				num_of_params = 1;
				break;
			case SYSCALL_SLEEP:
				num_of_params = 1;
				break;
			case SYSCALL_SIGNAL:
				num_of_params = 2;
				break;
			case SYSCALL_CLONE:
				num_of_params = 2;
				break;
			case SYSCALL_FORK:
				num_of_params = 0;
				break;
			case SYSCALL_STATS:
				num_of_params = 0;
				break;
			case SYSCALL_CONFIGURE:
				num_of_params = 1;
				break;
			case SYSCALL_PHYS_INFO:
				num_of_params = 0;
				break;
			case SYSCALL_DUMP_PTT:
				num_of_params = 1;
				break;
			case SYSCALL_CFORK:
				num_of_params = 0;
				break;
			case SYSCALL_MMAP:
				num_of_params = 4;
				break;
			case SYSCALL_MUNMAP:
				num_of_params = 2;
				break;
			case SYSCALL_MPROTECT:
				num_of_params = 3;
				break;
			case SYSCALL_PMAP:
				num_of_params = 1;
				break;
			case SYSCALL_VFORK:
				num_of_params = 0;
				break;
			case SYSCALL_GET_USER_P:
				num_of_params = 0;
				break;
		}
	    to_print+=(num_of_params+1)*8;
		for(int j=0;j<num_of_params;j++){
			*(u64*)(buff + 8 + j*8) = *(u64*)(trace_buffer->buffer + trace_buffer->read_offset);
			trace_buffer->read_offset = ((trace_buffer->read_offset) + 8) % 4096;
			trace_buffer->size_filled-=8;
			//available_data-=8;

		}

		// if(available_data == 0){
		// 	break;
		// }
        printk("read offset break %d\n", trace_buffer->read_offset);
        printk("write offset break %d\n", trace_buffer->write_offset);
		buff = buff + (1+num_of_params)*8;
        if(trace_buffer->read_offset==trace_buffer->write_offset){
            break;
        }
		
	}
	
	printk("to print is : %d \n", to_print);
	return to_print;


	
}

int sys_start_strace(struct exec_context *current, int fd, int tracing_mode) {
    // Check if the provided file descriptor corresponds to a valid trace buffer
	printk("inside start strace\n");
    if (fd < 0 || fd >= MAX_OPEN_FILES || current->files[fd] == NULL || current->files[fd]->type != TRACE_BUFFER) {
        return -EINVAL; // Invalid file descriptor
    }

    // Initialize system call tracing for the current process
    if(!current->st_md_base){current->st_md_base = (struct strace_head*)os_page_alloc(USER_REG);
    if (current->st_md_base == NULL) {
        return -ENOMEM; // Memory allocation failed
    }}

    // Set the tracing mode (FULL TRACING or FILTERED TRACING)
    current->st_md_base->tracing_mode = tracing_mode;
	printk("tracing mode is %d\n", current->st_md_base->tracing_mode);

    // Assign the provided file descriptor (fd) to the process for trace buffer access
    current->st_md_base->strace_fd = fd;
	printk("fd is %d\n", current->st_md_base->strace_fd);

    // Enable system call tracing
    current->st_md_base->is_traced = 1;
	printk("is_traced is %d\n", current->st_md_base->is_traced);

    return 0; 
}


int sys_end_strace(struct exec_context *current) {
    if (!current->st_md_base) {
        return -EINVAL; // Tracing not started
    }

    // Disable system call tracing
    current->st_md_base->is_traced = 0;
	printk("is_traced is %d\n", current->st_md_base->is_traced);
    // Free memory for the traced system call information
    struct strace_info *current_trace_info = current->st_md_base->next;
    while (current_trace_info) {
        struct strace_info *next_trace_info = current_trace_info->next;
        os_page_free(USER_REG, current_trace_info);
        current_trace_info = next_trace_info;
    }
	
    os_page_free(USER_REG, current->st_md_base); // Free the main tracing data structure
	current->st_md_base = NULL; // Reset the system call tracing data structure

    return 0; // Success
}



///////////////////////////////////////////////////////////////////////////
//// 		Start of ftrace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////
int is_disabled(struct exec_context *ctx, unsigned long faddr){
	struct ftrace_info* current_ftrace_info =ctx->ft_md_base->next;
	//make a backup array which stored the values of current_fstrace_info->code_backup
	while(current_ftrace_info){
		if(current_ftrace_info->faddr==faddr){
			break;
		}
		current_ftrace_info=current_ftrace_info->next;
	}	
	for(int i=0;i<4;i++){
		if(current_ftrace_info->code_backup[i]==0xFF){
			continue;
		}
		else{
			return 0;
		}
	}
	return 1;
	
}

long do_ftrace(struct exec_context *ctx, unsigned long faddr, long action, long nargs, int fd_trace_buffer)
{
	printk("inside do ftrace\n");
	//func addr: Address of an user-space function action: Action to be performed (e.g., trace a function, stop tracing a function etc.) nargs: Number of arguments for the function fd_trace_buffer: File descriptor of the trace buffer for storing the function tracing information.
	if (action != ADD_FTRACE && action != REMOVE_FTRACE && action != ENABLE_FTRACE && action!=DISABLE_FTRACE && action!=ENABLE_BACKTRACE && action!=DISABLE_BACKTRACE) {
    	return -EINVAL;
	}
	if(!ctx->ft_md_base){
		ctx->ft_md_base = (struct ftrace_head*)os_page_alloc(USER_REG);
		if (ctx->ft_md_base == NULL) {
			return -ENOMEM; // Memory allocation failed
		}
		ctx->ft_md_base->count=0;
		ctx->ft_md_base->next=NULL;
		ctx->ft_md_base->last=NULL;
	}
	// add_ftrace add the function into the loist to be traced
	if(action==ADD_FTRACE){
		printk("inside add_ftrace\n");
		struct ftrace_info* current_ftrace_info = ctx->ft_md_base->next;
		if(ctx->ft_md_base->count==FTRACE_MAX){
			return -EINVAL;
		}

		while (current_ftrace_info) {
			if (current_ftrace_info->faddr == faddr) {
				return -EINVAL; // function call already traced
			}
			current_ftrace_info = current_ftrace_info->next;
		}

		printk("faddr is %d\n", faddr);
		struct ftrace_info* new_ftrace_info = (struct ftrace_info*)os_page_alloc(USER_REG);
		if (new_ftrace_info == NULL) {
			return -ENOMEM;
		}
		new_ftrace_info->faddr = faddr;
		new_ftrace_info->fd=fd_trace_buffer;
		new_ftrace_info->num_args=nargs;
		new_ftrace_info->next = NULL;
		for(int i=0;i<4;i++){
			new_ftrace_info->code_backup[i]=0xFF;
		}
		
		if (!ctx->ft_md_base->next) {
			ctx->ft_md_base->next = new_ftrace_info;
			ctx->ft_md_base->last = new_ftrace_info;
		} else {
			ctx->ft_md_base->last->next = new_ftrace_info;
			ctx->ft_md_base->last = new_ftrace_info;
		}

		printk("VALUE OF NEXT FADDR : %d \n", ctx->ft_md_base->next->faddr);
		printk("VALUE OF LAST FADDR : %d \n", ctx->ft_md_base->last->faddr);

		ctx->ft_md_base->count++;
		printk("count is %d\n", ctx->ft_md_base->count);
		
		if(is_disabled(ctx,new_ftrace_info->faddr) ==0)
		{
			printk("problem in add ftrace: makes it ftrace enabled\n");
		}
		
		return 0;

	}

	if(action==REMOVE_FTRACE){
		printk("inside remove_ftrace\n");
		struct ftrace_info* current_ftrace_info = ctx->ft_md_base->next;
		struct ftrace_info* previous_ftrace_info = NULL;

		while (current_ftrace_info) {
			if (current_ftrace_info->faddr == faddr) {
				// Found the syscall to remove
				if (previous_ftrace_info) {
					previous_ftrace_info->next = current_ftrace_info->next;
				} else {
					ctx->ft_md_base->next = current_ftrace_info->next;
				}
				if(!is_disabled(ctx,current_ftrace_info->faddr)){
					for(int i=0;i<4;i++){
						*(u64*)(current_ftrace_info->faddr+i*8)=current_ftrace_info->code_backup[i];
					}
					for (int i = 0; i < 4; i++)
					{
						current_ftrace_info->code_backup[i]=0xFF;
					}
					
				}
				os_page_free(USER_REG, current_ftrace_info);
				ctx->ft_md_base->count--;
				break;
			}

			previous_ftrace_info = current_ftrace_info;
			current_ftrace_info = current_ftrace_info->next;
		}
		return 0;
	}

	if(action == ENABLE_FTRACE){
		printk("inside enable_ftrace\n");
		struct ftrace_info* current_ftrace_info = ctx->ft_md_base->next;
		int k=0;
		while (current_ftrace_info) {
			if (current_ftrace_info->faddr == faddr) {
				if(is_disabled(ctx,current_ftrace_info->faddr)==0){
					return -EINVAL;
				}
				for(int i=0;i<4;i++){
						current_ftrace_info->code_backup[i]=*(u8*)(current_ftrace_info->faddr+i);
				}
				for (int i = 0; i < 4; i++)
				{
					*(u8*)(current_ftrace_info->faddr+i)=0xFF;
				}
				k++;
				break;
			}
			current_ftrace_info = current_ftrace_info->next;
		}
		if(is_disabled(ctx,current_ftrace_info->faddr) == 0){
			printk("enabled now\n");
		}
		if(k==0){
			return -EINVAL;
		}
		printk("outside enable ftrace\n");
		return 0;
	}

	if(action==DISABLE_FTRACE){
		printk("inside disable_ftrace\n");
		struct ftrace_info* current_ftrace_info = ctx->ft_md_base->next;
		int k=0;
		while (current_ftrace_info) {
			if (current_ftrace_info->faddr == faddr) {
				if(is_disabled(ctx,current_ftrace_info->faddr)==1){
					return -EINVAL;
				}
				for(int i=0;i<4;i++){
						*(u8*)(current_ftrace_info->faddr+i)=current_ftrace_info->code_backup[i];
				}
				for (int i = 0; i < 4; i++)
				{
					current_ftrace_info->code_backup[i]=0xFF;
				}
				k++;
				break;
			}
			current_ftrace_info = current_ftrace_info->next;
		}

		// if this tries to disable tracing of a function which is not being added to the list then return einval
		if(k==0){
			return -EINVAL;
		}
		
		return 0;
	}

	if(action==ENABLE_BACKTRACE){
		printk("inside enable_backtrace\n");
		struct ftrace_info* current_ftrace_info = ctx->ft_md_base->next;
		int k=0;
		while (current_ftrace_info) {
			if (current_ftrace_info->faddr == faddr) {
				if(is_disabled(ctx,current_ftrace_info->faddr)==0){
					k++;
					break;
				}
				for(int i=0;i<4;i++){
						current_ftrace_info->code_backup[i]=*(u8*)(current_ftrace_info->faddr+i);
				}
				for (int i = 0; i < 4; i++)
				{
					*(u8*)(current_ftrace_info->faddr+i)=0xFF;
				}
				k++;
				break;
				
			}
			current_ftrace_info = current_ftrace_info->next;
		}
		if(k==0){
			return -EINVAL;
		}
		current_ftrace_info->capture_backtrace=1;
		printk("enable backtrace ends\n");
		return 0;
	}

	if(action==DISABLE_BACKTRACE){
		printk("inside disable_backtrace\n");
		struct ftrace_info* current_ftrace_info = ctx->ft_md_base->next;
		int k=0;
		while (current_ftrace_info) {
			if (current_ftrace_info->faddr == faddr) {
				if(is_disabled(ctx,current_ftrace_info->faddr)==1){
					k++;
					break;
				}
				for(int i=0;i<4;i++){
						*(u8*)(current_ftrace_info->faddr+i)=current_ftrace_info->code_backup[i];
				}
				for (int i = 0; i < 4; i++)
				{
					current_ftrace_info->code_backup[i]=0xFF;
				}
				k++;
				break;
				
			}
			current_ftrace_info = current_ftrace_info->next;
		}
		if(k==0){
			return -EINVAL;
		}
		current_ftrace_info->capture_backtrace=0;
		return 0;
	}
}

//Fault handler
long handle_ftrace_fault(struct user_regs *regs)
{		
	printk("-----inside handle ftrace fault-----\n");
	if(regs==NULL){
		return 0;
	}
	struct exec_context *ctx = get_current_ctx();
	struct ftrace_info* current_ftrace_info = ctx->ft_md_base->next;
	if(!current_ftrace_info){
		return -EINVAL;
	}
	int k=0;
	while (current_ftrace_info) {
			if (current_ftrace_info->faddr == regs->entry_rip) {
				if(is_disabled(ctx,current_ftrace_info->faddr)==0){
					k++;
					break;
				}
			}
			current_ftrace_info = current_ftrace_info->next;
	}

	if(k==0){
		return -EINVAL;
	}
	struct trace_buffer_info* trace_buffer = ctx->files[current_ftrace_info->fd]->trace_buffer;
	
	printk("trace buffer size in handle fault : %d\n", trace_buffer->size_filled);
	char * buffer= trace_buffer->buffer;
	int write_offset=trace_buffer->write_offset;
	int read_offset=trace_buffer->read_offset;

	

	if(trace_buffer->size_filled==TRACE_BUFFER_MAX_SIZE){
		return -EINVAL;
	}

	u64* counter=(u64*)(buffer+write_offset);
	int i=1;
	trace_buffer->size_filled+=8;

	*(u64*)(buffer+((write_offset+i*8)%TRACE_BUFFER_MAX_SIZE))=regs->entry_rip;
	i++;
	trace_buffer->size_filled+=8;

	int fill_flag = 0;
	int num_args = current_ftrace_info->num_args;
	for(int j =1; j<=num_args; j++)
	{
		if((write_offset + 8*i) %TRACE_BUFFER_MAX_SIZE== read_offset)
		{
			fill_flag = 1;
			break;
		}
		switch (j)
		{
		case 1:
			*((u64*)(buffer + (write_offset + 8*i)%TRACE_BUFFER_MAX_SIZE )) =  regs->rdi;
			i++;
			trace_buffer->size_filled+=8;
			break;
		case 2:
			*((u64*)(buffer + (write_offset + 8*i)%TRACE_BUFFER_MAX_SIZE )) =  regs->rsi;
			i++;
			trace_buffer->size_filled+=8;
			break;
		case 3:
			*((u64*)(buffer + (write_offset+ 8*i)%TRACE_BUFFER_MAX_SIZE )) =  regs->rdx;
			i++;
			trace_buffer->size_filled+=8;
			break;
		case 4:	
			*((u64*)(buffer + (write_offset + 8*i)%TRACE_BUFFER_MAX_SIZE )) =  regs->rcx;
			i++;
			trace_buffer->size_filled+=8;
			break;
		case 5:	
			*((u64*)(buffer + (write_offset + 8*i)%TRACE_BUFFER_MAX_SIZE )) =  regs->r8;
			i++;
			trace_buffer->size_filled+=8;
			break;
		case 6:	
			*((u64*)(buffer + (write_offset + 8*i)%TRACE_BUFFER_MAX_SIZE )) =  regs->r9;
			i++;
			trace_buffer->size_filled+=8;
			break;
		default:
			break;
		}
	}
	printk("size filed before backtracing is %d\n", trace_buffer->size_filled);

	if(fill_flag!= 1)
	{
		
		// printk("size till now: %d\n", trace_buffer->);
		if(current_ftrace_info->capture_backtrace == 1)
		{
			printk("backtrace tracing onn\n");
			
			if((write_offset + 8*i) %TRACE_BUFFER_MAX_SIZE == read_offset )
			{
				fill_flag=1;

			}
			else{
				*((u64*)(buffer + (write_offset + 8*i)%TRACE_BUFFER_MAX_SIZE )) = regs->entry_rip;
				i++;
				trace_buffer->size_filled+=8;
				if((write_offset + 8*i) %TRACE_BUFFER_MAX_SIZE == read_offset )
				{
					fill_flag=1;
				}
				else
				{
					u64 return_addr = *(u64*)(regs->entry_rsp);
					*((u64*)(buffer + (write_offset + 8*i)%TRACE_BUFFER_MAX_SIZE )) = return_addr;
					i++;
					trace_buffer->size_filled+=8;

					u64 rbp = regs->rbp;
					return_addr = *(u64*)(rbp+ 8);
					while(return_addr != END_ADDR)
					{
						if((write_offset + 8*i) %TRACE_BUFFER_MAX_SIZE == read_offset )
						{
							fill_flag = 1;
							break;
						}
						*((u64*)(buffer + (write_offset + 8*i)%TRACE_BUFFER_MAX_SIZE )) = return_addr;
						i++;
						trace_buffer->size_filled+=8;

						rbp = *(u64*)(rbp);
						return_addr = *(u64*)(rbp + 8);
					}
				}
			}
		}
	}
	if(fill_flag == 1){
		return -EINVAL;
	}
	*(counter) = i;
	trace_buffer->write_offset = (write_offset + i*8)%TRACE_BUFFER_MAX_SIZE ;
	printk("size filed is %d\n", trace_buffer->size_filled);

	*(u64*)(regs->entry_rsp - 8) = regs->rbp;
	regs->entry_rsp = regs->entry_rsp - 8; 
	regs->rbp = regs->entry_rsp; 
	regs->entry_rip = regs->entry_rip + 4; 
	printk("----outside handle fault----\n");
	return 0;
}


int sys_read_ftrace(struct file *filep, char *buff, u64 count)
{
	printk("inside read ftrace\n");
	struct trace_buffer_info* trace_buffer = filep->trace_buffer;
	int to_print=0;
	
	if(trace_buffer->size_filled==0){
		return -EINVAL;
	}
	printk("number of functions to trace: %d\n", count);
	printk("trace buffer in read ftrace is : %d\n", trace_buffer->size_filled);
	for (int i = 0; i < count; i++)
	{
		printk("inside count loop\n");
		int counter =*(u64*)( trace_buffer->buffer+trace_buffer->read_offset);
		int j=1;
		counter--;
		trace_buffer->size_filled -= 8;
		while(counter--){
			*(u64*)buff = *(u64*)(trace_buffer->buffer + (trace_buffer->read_offset+j*8)%TRACE_BUFFER_MAX_SIZE);
			buff=buff+8;
			j++;
			trace_buffer->size_filled-=8;
			to_print+=8;
			if(trace_buffer->size_filled==0){
				printk("size became zero inbetween\n");
				trace_buffer->read_offset = trace_buffer->read_offset + j*8;
				printk("the return of read is : %d\n", to_print);
				return to_print;
			}
			
		}
		trace_buffer->read_offset = trace_buffer->read_offset + j*8;
		// printk("to print after an iteration: %d\n", to_print);
	}
	
	printk("the return of read is : %d\n", to_print);

	return to_print;

}