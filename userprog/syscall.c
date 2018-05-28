#include "userprog/syscall.h" 
#include <stdio.h>

#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "process.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "threads/synch.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);
struct lock filesys_lock;

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static bool is_valid_address(const void *addr){
	return (addr!=NULL && is_user_vaddr(addr)
			&& pagedir_get_page(thread_current()->pagedir,addr)!=NULL);
}

static bool is_valid_buffer(const void *buf, unsigned size){
	const uint8_t * addr = buf;
	unsigned i;

	
	for (i=0; i<size; i++)
		if (!is_valid_address(addr+i))
			return 0;

	return 1;
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

static void halt(void){
	shutdown_power_off();
}

static void exit(struct intr_frame *f){
	uint32_t *esp = (uint32_t * )f->esp+1;
	check_stack(esp,1);

	int status_code=*esp;

	thread_current()->exit_code = status_code;
	thread_exit();
}

static void create(struct intr_frame *f){
	uint32_t *esp = (uint32_t * )f->esp+1;
	check_stack(esp,2);

	char* file = *(char **)esp;
	esp++;
	unsigned initial_size = *(unsigned *)esp;


	if (!is_valid_address(file))
		thread_exit();
	

	lock_acquire(&filesys_lock);
	f->eax = filesys_create(file, initial_size);
	lock_release(&filesys_lock);
}

static void remove(struct intr_frame *f){
	uint32_t *esp = (uint32_t * )f->esp+1;
	check_stack(esp,1);

	char* file = *(char **)esp;

	if (!is_valid_address(file))
		thread_exit();
	
	lock_acquire(&filesys_lock);
	f->eax = filesys_remove(file);
	lock_release(&filesys_lock);
}

static void open(struct intr_frame *f){
	uint32_t *esp = (uint32_t * )f->esp+1;
	check_stack(esp,1);

	char* filename = *(char **)esp;

	if (!is_valid_address(filename))
		thread_exit();
	

	lock_acquire(&filesys_lock);
	struct file* file = filesys_open(filename);
	lock_release(&filesys_lock);

	if (!file){
		f->eax = -1;
		return;
	}

	f->eax = add_file(file); 
}

static void close(struct intr_frame *f){
	uint32_t *esp = (uint32_t * )f->esp+1;
	check_stack(esp,1);

	fd_t fd = *esp;

	struct file* file = get_file_by_fd(fd);
	if (file==NULL)
		return;

	lock_acquire(&filesys_lock);
	file_close(file);
	lock_release(&filesys_lock);

	remove_file(fd); 
}


static void write(struct intr_frame *f){
	uint32_t *esp = (uint32_t * )f->esp+1;
	check_stack(esp,3);

	fd_t fd=*esp;
	esp++;

	void * buf = *(void **)esp;
	esp++;
	unsigned int size = *(unsigned int *) esp;


	if (!is_valid_buffer(buf,size))
		thread_exit();

	//write to console
	if (fd==1){
		putbuf(buf,size);
		f->eax=size;
	}
	//else write to file
	else {

		struct file * file = get_file_by_fd(fd);
		if (file==NULL){
			f->eax = 0;
			return;
		}

		lock_acquire(&filesys_lock);
		f->eax = file_write(file,buf,size);
		lock_release(&filesys_lock);

	}
}

static void read(struct intr_frame *f){
	uint32_t *esp = (uint32_t * )f->esp+1;
	check_stack(esp,3);
	
	fd_t fd=*esp;
	esp++;

	void * buf = *(void **)esp;
	esp++;
	unsigned int size = *(unsigned int *) esp;

	if (size==0){
		f->eax = 0;
		return;
	}

	if (!is_valid_buffer(buf,size))
		thread_exit();


	//read from console
	if (fd==0){
		char * buffer = buf;
		unsigned k=0;

		while (k<size)
			buffer[k++]=input_getc();

		f->eax=size;
	}
	//else read from file
	else {

		struct file * file = get_file_by_fd(fd);
		if (file==NULL){

			f->eax = 0;
			return;
		}

		lock_acquire(&filesys_lock);
		f->eax = file_read(file,buf,size);
		lock_release(&filesys_lock);

	}
}

static void filesize(struct intr_frame *f){
	uint32_t *esp = (uint32_t * )f->esp+1;
	check_stack(esp,1);

	fd_t fd = *esp;

	struct file* file = get_file_by_fd(fd);
	if (file==NULL)
		return;

	lock_acquire(&filesys_lock);
	f->eax = file_length(file);
	lock_release(&filesys_lock);
} 

static void seek(struct intr_frame *f){
	uint32_t *esp = (uint32_t * )f->esp+1;
	check_stack(esp,2);

	fd_t fd = *esp;
	esp++;
	unsigned position = *esp;

	struct file* file = get_file_by_fd(fd);
	if (file==NULL)
		return; 

	lock_acquire(&filesys_lock);
	file_seek(file,position);
	lock_release(&filesys_lock);
}

static void tell(struct intr_frame *f){
	uint32_t *esp = (uint32_t * )f->esp+1;
	check_stack(esp,1);

	fd_t fd = *esp;

	struct file* file = get_file_by_fd(fd);
	if (file==NULL)
		return; 

	lock_acquire(&filesys_lock);
	f->eax = file_tell(file);
	lock_release(&filesys_lock);
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
  	case SYS_HALT:
  		halt();
  		break;

  	case SYS_EXEC:
  		exec(f);
  		break;

  	case SYS_EXIT:
  		exit(f);
  		break;

  	case SYS_CREATE:
  		create(f);
  		break;

  	case SYS_REMOVE:
  		remove(f);
  		break;

  	case SYS_OPEN:
  		open(f);
  		break;

  	case SYS_CLOSE:
  		close(f);
  		break;

  	case SYS_WRITE:
  		write(f);
  		break;

  	case SYS_READ:
  		read(f);
  		break;

  	case SYS_FILESIZE:
  		filesize(f);
  		break;

  	case SYS_SEEK:
  		seek(f);
  		break;

  	case SYS_WAIT:
  		wait(f);
  		break;

  	case SYS_TELL:
  		tell(f);
  		break;

  	default:
  		printf("not supported system call %d\n",syscall_nr);
  		thread_exit();
  }
}


