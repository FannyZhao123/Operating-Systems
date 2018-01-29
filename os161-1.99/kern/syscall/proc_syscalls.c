#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include "opt-A2.h"
#include <synch.h>
#include <limits.h>
#include <mips/trapframe.h>
#include <kern/fcntl.h>
#include <vfs.h>
#include <vm.h>
#include <test.h>

int sys_fork(struct trapframe *tf, pid_t *retval)
{
    /*
    • Create process structure for child process
    • Create and copy address space
    • Assign PID to child process and create the parent/child relationship
    • Create thread for child process
    • Child thread needs to put the trapframe onto the stack 
        and modify it so that it returns the current value (and executes the next instruction)
    • Call mips_usermode in the child to go back to userspace
    */

    //Create process structure for child process
    struct proc *child_proc = proc_create_runprogram(curproc->p_name);
    if(child_proc == NULL){
        panic("proc_create_runprogram failed.");
    }
    /*
     Create and copy address space
    – Child process must be identical to the parent process
    – as_copy() creates a new address spaces, and copies the pages from the old address space to the new one
    – Address space is not associated with the new process yet • Look at curproc_setas to figure out how to give a process an
    address space
    – Remember to handle any error conditions correctly (what if as_copy returns an error?)
      */
    struct trapframe *trapframe_fork = kmalloc(sizeof(struct trapframe));
    //if the trapframe is NULL, no more memory
    if(trapframe_fork == NULL){
      proc_destroy(child_proc);
      return ENOMEM;
    }
    //
    struct addrspace *addrspace_fork = kmalloc(sizeof(struct addrspace));
    if(addrspace_fork == NULL){
      kfree(trapframe_fork);
      proc_destroy(child_proc);
      return ENOMEM;
    }
    //
    memcpy(trapframe_fork,tf,sizeof(struct trapframe));
    as_copy(curthread->t_proc->p_addrspace, &addrspace_fork);
    //check 
    if(addrspace_fork == NULL){
      kfree(trapframe_fork);
      proc_destroy(child_proc);
      return ENOMEM;
    }
    child_proc->p_addrspace = addrspace_fork;

    /*
    • Assign PID to child process and create the parent/child relationship
    – PIDs should be unique (no two processes should have the
    same PID)
    – PIDs should be reusable.
    – Remember that you need to provide mutual exclusion for any global structure!
    */
    //live PID for now
    child_proc->parent = curproc;

    /*
    • Create thread for child process
    – Use thread_fork() to create a new thread
    – Need to pass trapframe to the child thread
    – pass the trapframe pointer directly from the parent to the child
    */
    /*int
    thread_fork(const char *name,
          struct proc *proc,
          void (*entrypoint)(void *data1, unsigned long data2),
          void *data1, unsigned long data2)
    */

    *retval=child_proc->pid;

    int error = thread_fork(curproc->p_name, child_proc, (void*) enter_forked_process, trapframe_fork, (unsigned long)child_proc->p_addrspace);
    //error will not be equal to 0 if there is error, aka, if error is int other than 0, it will return error
    if(error){
      kfree(trapframe_fork);
      proc_destroy(child_proc);
      as_destroy(addrspace_fork);
      return ENOMEM;
    }
    tf->tf_v0 = 0;
    tf->tf_a3 = 0;
    return 0;
}



  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  lock_acquire(lock1);
  p->quit=__WEXITED;
  p->exitcode=_MKWAIT_EXIT (exitcode);
  clean_pid(p->pid);

  KASSERT(curproc->p_addrspace != NULL);

  if(p->parent==NULL || p->parent->pid==2){
    as_deactivate();
    /*
     * clear p_addrspace before calling as_destroy. Otherwise if
     * as_destroy sleeps (which is quite possible) when we
     * come back we'll be calling as_activate on a
     * half-destroyed address space. This tends to be
     * messily fatal.
     */
    as = curproc_setas(NULL);
    as_destroy(as);
    cv_broadcast(p->wcv, lock1);
    lock_release(lock1);
    proc_remthread(curthread);
    proc_destroy(p);
  }
  else{
    cv_broadcast(p->wcv, lock1);
    lock_release(lock1);
    as_deactivate();
    /*
     * clear p_addrspace before calling as_destroy. Otherwise if
     * as_destroy sleeps (which is quite possible) when we
     * come back we'll be calling as_activate on a
     * half-destroyed address space. This tends to be
     * messily fatal.
     */
    as = curproc_setas(NULL);
    as_destroy(as);
    /* detach this thread from its process */
    /* note: curproc cannot be used after this call */
    proc_remthread(curthread);
  }
  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  //proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  *retval=curthread->t_proc->pid;
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  //*retval = 1;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid, userptr_t status, int options, pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */
  struct proc *temp_proc=get_proc(pid);
  if(temp_proc->parent==NULL || temp_proc->parent!=curproc){
    return ESRCH;
  }
  if (options != 0) {
    return EINVAL;
  }
  lock_acquire(lock1);
  while(temp_proc->quit!=__WEXITED){
    cv_wait(temp_proc->wcv,lock1);
  }
  lock_release(lock1);
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = temp_proc->exitcode;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}


//int sys_execv(const char *program, char **args){
int sys_execv(userptr_t progname, userptr_t args){
  /*
  ENODEV  The device prefix of program did not exist.
  ENOTDIR A non-final component of program was not a directory.
  ENOENT  program did not exist.
  EISDIR  program is a directory.
  ENOEXEC program is not in a recognizable executable file format, was for the wrong platform, or contained invalid fields.
  ENOMEM  Insufficient virtual memory is available.
  E2BIG The total size of the argument strings is too large.
  EIO A hard I/O error occurred.
  EFAULT  One of the args is an invalid pointer.
  */
  struct addrspace *as;
  struct vnode *v;
  vaddr_t entrypoint, stackptr;
  int result;
  size_t plen = 0;

  /* Count the number of arguments and copy them into the kernel */
  int count_arg = 0;
  while (((char**)args)[count_arg]!=NULL){
    count_arg++;
  } 
  size_t mem_alen = (count_arg+1) * sizeof(char*);
  char **k_args = (char**)kmalloc(mem_alen);
  if(k_args == NULL) return ENOMEM;

  char **temp = (char **)args;
  for(int i = 0; i<count_arg; i++){
    //init array
    size_t length = 1 + strlen(((char**)args)[i]);
    size_t l = length* sizeof(char);
    k_args[i] = (char*)kmalloc(l);
    if(k_args[i]==NULL){
      return ENOMEM;
      }
    memcpy(k_args[i], temp[i], length);
  }
  // the last element
  k_args[count_arg]=NULL;

  /* Copy the program path into the kernel */
  plen = 1 + strlen((char *)progname);
  size_t mem_plen = plen * sizeof(char);
  char *k_progname = (char*)kmalloc(mem_plen);
  if(k_progname == NULL) return ENOMEM; 

  /* 
  When copying from/to userspace
    – Use copyin/copyout for fixed size variables (integers, arrays, etc.)
    – Use copyinstr/copyoutstr when copying NULL terminated strings
  */
  //int copyin(const void *uaddr, void *kaddr, size_t len);
  result = copyin ((const_userptr_t)progname, (void *)k_progname, mem_plen);
  if(result) return -1;

  /* Open the file. */
  result = vfs_open((char *)progname, O_RDONLY, 0, &v);
  if (result) {
    return result;
  }

  /* We should be a new process. */
  //KASSERT(curproc_getas() == NULL);

  /* Create a new address space. */
  as = as_create();
  if (as ==NULL) {
    vfs_close(v);
    return ENOMEM;
  }

  /* Switch to it and activate it. */
  curproc_setas(as);
  as_activate();

  /* Load the executable. */
  result = load_elf(v, &entrypoint);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    vfs_close(v);
    return result;
  }

  /* Done with the file now. */
  vfs_close(v);


  /* copy the arguments into the new address space */

  /* Define the user stack in the address space */
  result = as_define_stack(as, &stackptr);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    return -1;
  }
  vaddr_t *u_args =(vaddr_t*) kmalloc((count_arg+1)* sizeof(vaddr_t));
  if(u_args==NULL) return ENOMEM;
  for(int i = count_arg-1; i>=0; i--){
    size_t temp = strlen(k_args[i])+1;
    stackptr = stackptr - (ROUNDUP(temp, 4) * sizeof(char));
    /*
    int copyout(const void *kaddr, void *uaddr, size_t len);
    */
    result = copyout((void *)k_args[i], (userptr_t)stackptr, temp);
    if(result) return -1;
    u_args[i] = stackptr;
  }
  u_args[count_arg]=(vaddr_t)NULL;

  size_t s = sizeof(vaddr_t);
  for(int i=count_arg; i>=0; i--){
    stackptr=stackptr-s;
    //kprintf("stackptr:%s\n", (char *)stackptr);
    result=copyout((void *)&u_args[i], (userptr_t)stackptr,s);
    //kprintf("u_args[%d]:%s\n",i, (char *)&u_args[i]);
    if(result) return -1;
  }

  /* Warp to user mode. */
  /*
  Call enter_new_process with address to the arguments on the stack, 
  the stack pointer (from as_define_stack), 
  and the program entry point (from vfs_open)
  */
  enter_new_process(count_arg, (userptr_t)stackptr,
        stackptr, entrypoint);
  
  /* enter_new_process does not return. */
  panic("enter_new_process returned\n");
  return EINVAL;
}
