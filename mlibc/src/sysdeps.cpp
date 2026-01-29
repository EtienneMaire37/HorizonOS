#include "mlibc/tcb.hpp"
#include <abi-bits/errno.h>
#include <bits/ensure.h>
#include <bits/syscall.h>
#include <mlibc/all-sysdeps.hpp>
#include <string.h>
#include <stdint.h>

#include "syscall.hpp"

void STUB()
{
	while (true);
	__builtin_unreachable();
}

namespace mlibc
{
	void sys_libc_panic() 
	{
		sys_libc_log("\n\x1b[31m[[!!! mlibc panic !!!]]\x1b[0m\n");
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
		return syscall1_1(SYS_ISATTY, (uint64_t)fd);
	}

	int sys_write(int fd, void const* buf, size_t size, ssize_t* written) 
	{
		uint64_t r64;
		int ret = syscall3_2(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)size, &r64);
		*written = (ssize_t)r64;
		return ret;
	}

	int sys_tcb_set(void* pointer)
	{
		syscall1_1(SYS_SETFS, (uint64_t)pointer);
		return 0;
	}

	int sys_anon_allocate(size_t size, void** pointer) 
	{
		return sys_vm_map(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0, pointer);
	}

	int sys_anon_free(void* pointer, size_t size) 
	{ 
		return sys_vm_unmap(pointer, size);
	}

	int sys_seek(int, off_t, int, off_t*) 
	{
		
		STUB();
	}

	void sys_exit(int status) 
	{
		syscall1_1(SYS_EXIT, status);
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
	int sys_vm_map(void* hint, size_t size, int prot, int flags, int fd, off_t offset, void** window) 
	{
		uint64_t addr;
		int ret = syscall6_2(SYS_VM_MAP, (uint64_t)hint, (uint64_t)size, (uint64_t)prot, (uint64_t)flags, (uint64_t)fd, (uint64_t)offset, &addr);
		*window = (void*)addr;
		return ret;
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