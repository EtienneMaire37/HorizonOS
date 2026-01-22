#include "mlibc/tcb.hpp"
#include <abi-bits/errno.h>
#include <bits/ensure.h>
#include <bits/syscall.h>
#include <mlibc/all-sysdeps.hpp>
#include <string.h>
#include <stdint.h>

#include "syscall.hpp"

uint64_t syscall1_1(uint64_t calln, uint64_t a1);

void STUB()
{
	while (true);
}

namespace mlibc 
{
	void sys_libc_panic() 
	{
		sys_libc_log("!!! mlibc panic !!!");
		sys_exit(-1);
		__builtin_trap();
	}

	void sys_libc_log(const char* msg) 
	{ 
		ssize_t unused;
		sys_write(2, msg, strlen(msg), &unused);
	}

	int sys_isatty(int fd)
	{
		
		STUB();
	}

	int sys_write(int fd, void const* buf, size_t size, ssize_t* ret) 
	{
		
		STUB();
	}

	int sys_tcb_set(void* pointer) 
	{
		syscall1_1(SYS_SETFS, (uint64_t)pointer);
		STUB();
	}

	int sys_anon_allocate(size_t size, void** pointer) 
	{
		
		STUB();
	}

	int sys_anon_free(void*, unsigned long) 
	{ 
		
		STUB(); 
	}

	int sys_seek(int, off_t, int, off_t*) 
	{
		
		STUB();
	}

	void sys_exit(int status) 
	{
		
		// syscall1_1(SYS_EXIT, status);
		__builtin_unreachable();
	}

	int sys_close(int) 
	{ 
		
		STUB(); 
	}
	int sys_futex_wake(int*) 
	{ 
		
		STUB(); 
	}
	int sys_futex_wait(int*, int, timespec const*) 
	{
		 
		STUB(); 
	}
	int sys_read(int, void*, unsigned long, long*) 
	{
		 
		STUB(); 
	}
	int sys_open(const char*, int, unsigned int, int*) 
	{
		 
		STUB(); 
	}
	int sys_vm_map(void*, size_t, int, int, int, off_t, void**) 
	{
		
		STUB();
	}
	int sys_vm_unmap(void* , size_t) 
	{
		 
		STUB(); 
	}
	int sys_clock_get(int, time_t*, long*) 
	{
		 
		STUB(); 
	}
}