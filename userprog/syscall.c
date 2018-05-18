#include "userprog/syscall.h" 
#include <stdio.h>

#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"


static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


static void exit(struct intr_frame *f){
	uint32_t *esp = (uint32_t * )f->esp+1;

	int status_code = *esp;
	thread_current()->exit_code = status_code;
	thread_exit();
}


static void write(struct intr_frame *f){
	uint32_t *esp = (uint32_t * )f->esp+1;

	int fd=*esp;
	esp++;

	void * buf = *(void **)esp;
	esp++;

	unsigned int size = *(unsigned int *) esp;

	//handle only printf
	if (fd==1){
		putbuf(buf,size);
		f->eax=size;
	}
}

static void
syscall_handler (struct intr_frame *f) 
{

  int syscall_nr = *(int *)f->esp;

  switch (syscall_nr){
  	case SYS_EXIT:
  		exit(f);
  		break;

  	case SYS_WRITE:
  		write(f);
  		break;

  	default:
  		printf("not supported system call %d",syscall_nr);
  		thread_exit();
  }
}


