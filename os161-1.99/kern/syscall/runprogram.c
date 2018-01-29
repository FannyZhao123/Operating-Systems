/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>


#include <copyinout.h>


/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, char **args, unsigned long nargs)
//int runprogram (userptr_t progname, userptr_t args)
{
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	 /* Count the number of arguments */
	/*
	  int count_arg = 0;
	  while (args[count_arg]!=NULL){
	    count_arg++;
	    kprintf("check count_arg:%d \n", count_arg);
	    kprintf("check args[count_arg]:%s \n", args[count_arg]);
	  } 

	*/
	 int count_arg = nargs;
	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v);
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

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}


	//
	//
	//
		vaddr_t *u_args =(vaddr_t*) kmalloc((count_arg+1)* sizeof(vaddr_t));
		  if(u_args==NULL) return ENOMEM;
		  for(int i = count_arg-1; i>=0; i--){
		    size_t temp = strlen(args[i])+1;
		    stackptr = stackptr - (ROUNDUP(temp, 4) * sizeof(char));
		    /*
		    int copyout(const void *kaddr, void *uaddr, size_t len);
		    */
		    //kprintf("check 1\n");
		    result = copyout((void *)args[i], (userptr_t)stackptr, temp);
		    //kprintf("check 2\n");
		    if(result) return -1;
		    u_args[i] = stackptr;
		  }
		  u_args[count_arg]=(vaddr_t)NULL;

		  size_t s = sizeof(vaddr_t);
		  //char ** temp = (char **)u_args;
		  for(int i=count_arg; i>=0; i--){
		    stackptr=stackptr-s;
		    result=copyout((void *)&u_args[i], (userptr_t)stackptr,s);

		    //memcpy((void *)stackptr, temp[i], 1 + strlen(((char**)temp)[i]));
		    //kprintf("check i: %d\n", i);
		    //kprintf("u_args[%d]:%s\n",i, (char *)&u_args[i]);
		    if(result) return -1;
		  }




	/* Warp to user mode. */
	//kprintf("check 4\n");
	enter_new_process(count_arg /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,
			  stackptr, entrypoint);
	//kprintf("check 5\n");
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

