#include "userprog/syscall.h" 
#include <stdio.h>

#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "process.h"
#include "threads/vaddr.h"
#include "pagedir.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static bool is_valid_address(void *addr){
	return (addr!=NULL && is_user_vaddr(addr)
			&& pagedir_get_page(thread_current()->pagedir,addr)!=NULL);
}

static void check_stack(void *base_addr, int nr_args){
	uint8_t* p =base_addr;

	bool ok=1;
	int i;
	for (i=0; i<4*nr_args; i++)
		ok&=is_valid_address(p+i);

	if (!ok)
		thread_exit();
}

static void exit(struct intr_frame *f){
	uint32_t *esp = (uint32_t * )f->esp+1;
	check_stack(esp,1);

	int status_code=*esp;

	thread_current()->exit_code = status_code;
	thread_exit();
}


static void write(struct intr_frame *f){
	uint32_t *esp = (uint32_t * )f->esp+1;
	check_stack(esp,3);

	int fd=*esp;
	esp++;

	void * buf = *(void **)esp;

	if (!is_valid_address(buf)){
		f->eax=0;
		return;
	}

	esp++;

	unsigned int size = *(unsigned int *) esp;

	//handle only printf
	if (fd==1){
		putbuf(buf,size);
		f->eax=size;
	}
}

static void wait(struct intr_frame *f){
	
	uint32_t *esp = (uint32_t*)f->esp+1;
	check_stack(esp,1);

	tid_t pid = *esp;

	f->eax = process_wait(pid);
}

static void exec(struct intr_frame *f){
	
	uint32_t *esp = (uint32_t*)f->esp+1;
	check_stack(esp,1);

	char * cmd_line = *(char**)esp;

	tid_t pid;

	if (!is_valid_address(cmd_line))
		pid = -1;
	else
		pid = process_execute(cmd_line);

	f->eax = pid;
}

static void
syscall_handler (struct intr_frame *f) 
{

  check_stack(f->esp,1);

  uint32_t syscall_nr = *(uint32_t *)f->esp;

  switch (syscall_nr){
  	case SYS_EXEC:
  		exec(f);
  		break;

  	case SYS_EXIT:
  		exit(f);
  		break;

  	case SYS_WRITE:
  		write(f);
  		break;


  	case SYS_WAIT:
  		wait(f);
  		break;

  	default:
  		printf("not supported system call %d\n",syscall_nr);
  		thread_exit();
  }
}


