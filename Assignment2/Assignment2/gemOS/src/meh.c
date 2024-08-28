long handle_ftrace_fault(struct user_regs *regs)
{

	if(regs == NULL)
		return 0;
	struct ftrace_info* node = find_fnode(get_current_ctx(), regs->entry_rip);
	if(node == NULL || !is_tracing(node)) //if node is not present in list or tracing is disabled
		return 0;

	// FILL THE TRACE BUFFER
	//find the trace buffer
	int fd = node->fd;
	struct trace_buffer_info* trace_buffer = get_current_ctx()->files[fd]->trace_buffer;

	char* buffer = trace_buffer->buffer;
	int off_r = trace_buffer->off_r;
	int off_w = trace_buffer->off_w;

	u64* count_ptr = (u64*)( buffer + off_w);
	int i = 1;
	
	*((u64*)(buffer + (off_w + 8*i)%TRACE_BUFFER_MAX_SIZE )) = regs->entry_rip;
	i++;

	int flag = 0;
	int num_args = node->num_args;
	for(int j =0; j<num_args; j++)
	{
		if((off_w + 8*i) == off_w)
		{
			flag = 1;
			break;
		}
		switch (j)
		{
		case 0:
			*((u64*)(buffer + (off_w + 8*i)%TRACE_BUFFER_MAX_SIZE )) =  regs->rdi;
			i++;
			break;
		case 1:
			*((u64*)(buffer + (off_w + 8*i)%TRACE_BUFFER_MAX_SIZE )) =  regs->rsi;
			i++;
			break;
		case 2:
			*((u64*)(buffer + (off_w + 8*i)%TRACE_BUFFER_MAX_SIZE )) =  regs->rdx;
			i++;
			break;
		case 3:	
			*((u64*)(buffer + (off_w + 8*i)%TRACE_BUFFER_MAX_SIZE )) =  regs->rcx;
			i++;
			break;
		case 4:	
			*((u64*)(buffer + (off_w + 8*i)%TRACE_BUFFER_MAX_SIZE )) =  regs->r8;
			i++;
			break;
		case 5:	
			*((u64*)(buffer + (off_w + 8*i)%TRACE_BUFFER_MAX_SIZE )) =  regs->r9;
			i++;
			break;
		default:
			break;
		}
	}
	if(flag != 1)
	{
		// check if backtrace is enabled 
		if(node->capture_backtrace == 1)
		{
			// bharo paincho
			if((off_w + 8*i) == off_w)
			{
				// to be filled
				// how?
				// the buffer is full
				// do the needful
			}
			*((u64*)(buffer + (off_w + 8*i)%TRACE_BUFFER_MAX_SIZE )) = regs->entry_rip;
			i++;
			if((off_w + 8*i) == off_w)
			{
				flag =1;
			}
			
			else
			{
				u64 ret_addr = *(u64*)(regs->entry_rsp);
				*((u64*)(buffer + (off_w + 8*i)%TRACE_BUFFER_MAX_SIZE )) = ret_addr;
				i++;

				u64 rbp = regs->rbp;
				ret_addr = *(u64*)(rbp+ 8);
				while(ret_addr != END_ADDR)
				{
					if((off_w + 8*i) == off_w)
					{
						flag = 1;
						break;
					}
					*((u64*)(buffer + (off_w + 8*i)%TRACE_BUFFER_MAX_SIZE )) = ret_addr;
					i++;

					rbp = *(u64*)(rbp);
					ret_addr = *(u64*)(rbp + 8);
				}
			}
		}
	}
	
	*(count_ptr) = i;
	trace_buffer->off_w = (off_w + i*8)%TRACE_BUFFER_MAX_SIZE ;

	//MANUALLY EXECUTE THE FIRST TWO INSTRUCTIONS

	*(u64*)(regs->entry_rsp - 8) = regs->rbp; // push rbp
	regs->entry_rsp = regs->entry_rsp - 8; // update rsp
	regs->rbp = regs->entry_rsp; // move rsp to rbp
	regs->entry_rip = regs->entry_rip + 4; //update the instruction pointer
	
	return 0;
}