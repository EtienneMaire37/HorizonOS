#include "mlibc/tcb.hpp"
#include <abi-bits/errno.h>
#include <bits/ensure.h>
#include <bits/syscall.h>
#include <mlibc/all-sysdeps.hpp>

#include "syscall.hpp"
#include "stub.h"

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
		sys_write(2, "\n", 1, &unused);
	}

	int sys_isatty(int fd)
	{
		return syscall1_1(SYS_ISATTY, (uint64_t)fd);
	}

	int sys_read(int fd, void* buf, size_t size, ssize_t* bytes_read) 
	{
		uint64_t r64;
		int ret = syscall3_2(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)size, &r64);
		*bytes_read = (ssize_t)r64;
		return ret;
	}
	int sys_write(int fd, void const* buf, size_t size, ssize_t* bytes_written) 
	{
		uint64_t r64;
		int ret = syscall3_2(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)size, &r64);
		*bytes_written = (ssize_t)r64;
		return ret;
	}

	int sys_tcb_set(void* pointer)
	{
		return syscall1_1(SYS_SETFS, (uint64_t)pointer);
	}

	int sys_anon_allocate(size_t size, void** pointer) 
	{
		return sys_vm_map(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0, pointer);
	}
	int sys_anon_free(void* pointer, size_t size) 
	{ 
		return sys_vm_unmap(pointer, size);
	}

	int sys_seek(int fd, off_t off, int flags, off_t* ret_val) 
	{
		uint64_t _ret_val;
		int ret = syscall3_2(SYS_SEEK, (uint64_t)fd, (uint64_t)off, (uint64_t)flags, &_ret_val);
		*ret_val = (off_t)_ret_val;
		return ret;
	}

	void sys_exit(int status) 
	{
		syscall1_1(SYS_EXIT, status);
		__builtin_unreachable();
	}

	int sys_open(const char* path, int oflags, unsigned int mode, int* fd) 
	{
		uint64_t _fd;
		int ret = syscall3_2(SYS_OPEN, (uint64_t)path, (uint64_t)oflags, (uint64_t)mode, &_fd);
		*fd = (int)_fd;
		return ret;
	}
	int sys_close(int fd) 
	{ 
		return syscall1_1(SYS_CLOSE, (uint64_t)fd);
	}

	int sys_futex_wake(int*) 
	{
		STUB("sys_futex_wake");
	}
	int sys_futex_wait(int*, int, timespec const*) 
	{
		STUB("sys_futex_wait");
	}

	int sys_vm_map(void* hint, size_t size, int prot, int flags, int fd, off_t offset, void** window) 
	{
		uint64_t addr;
		int ret = syscall6_2(SYS_VM_MAP, (uint64_t)hint, (uint64_t)size, (uint64_t)prot, (uint64_t)flags, (uint64_t)fd, (uint64_t)offset, &addr);
		*window = (void*)addr;
		return ret;
	}
	int sys_vm_unmap(void* addr, size_t size) 
	{
		return syscall2_1(SYS_VM_UNMAP, (uint64_t)addr, (uint64_t)size);
	}

	int sys_clock_get(int clock, time_t* secs, long* nanos)
	{
		return syscall3_1(SYS_CLOCK_GET, (uint64_t)clock, (uint64_t)secs, (uint64_t)nanos);
	}

	int sys_ioctl(int fd, unsigned long request, void* arg, int* result)
	{
		uint64_t _res;
		int ret = syscall3_2(SYS_IOCTL, (uint64_t)fd, (uint64_t)request, (uint64_t)arg, &_res);
		*result = (int)_res;
		return ret;
	}

	int sys_execve(const char* path, char* const argv[], char* const envp[])
	{
		return syscall3_1(SYS_EXECVE, (uint64_t)path, (uint64_t)argv, (uint64_t)envp);
	}

	int sys_tcgetattr(int fd, struct termios* attr)
	{
		return syscall2_1(SYS_TCGETATTR, (uint64_t)fd, (uint64_t)attr);
	}
	int sys_tcsetattr(int fd, int opts, const struct termios* attr)
	{
		return syscall3_1(SYS_TCSETATTR, (uint64_t)fd, (uint64_t)opts, (uint64_t)attr);
	}

	int sys_getcwd(char* buffer, size_t size)
	{
		return syscall2_1(SYS_GETCWD, (uint64_t)buffer, (uint64_t)size);
	}

	int sys_chdir(const char* path)
	{
		return syscall1_1(SYS_CHDIR, (uint64_t)path);
	}

	int sys_fork(pid_t* child)
	{
		uint64_t _child;
		int ret = syscall0_2(SYS_FORK, &_child);
		*child = (pid_t)_child;
		return ret;
	}

	int sys_sigprocmask(int how, const sigset_t* __restrict set, sigset_t* __restrict retrieve)
	{
		return syscall3_1(SYS_SIGPROCMASK, (uint64_t)how, (uint64_t)set, (uint64_t)retrieve);
	}

	int sys_waitpid(pid_t pid, int* status, int flags, struct rusage* ru, pid_t* ret_pid) 
	{
		uint64_t _ret_pid;
		int ret = syscall4_2(SYS_WAIT4, (uint64_t)pid, (uint64_t)status, (uint64_t)flags, (uint64_t)ru, &_ret_pid);
		*ret_pid = _ret_pid;
		return ret;
	}

	int sys_sigaction(int signum, const struct sigaction* __restrict act, struct sigaction* __restrict oldact)
	{
		return syscall3_1(SYS_SIGACTION, (uint64_t)signum, (uint64_t)act, (uint64_t)oldact);
	}

	int sys_ttyname(int fd, char* buf, size_t size)
	{
		return syscall3_1(SYS_TTYNAME, (uint64_t)fd, (uint64_t)buf, (uint64_t)size);
	}

	int sys_getresuid(uid_t* ruid, uid_t* euid, uid_t* suid)
	{
		return syscall3_1(SYS_GETRESUID, (uint64_t)ruid, (uint64_t)euid, (uint64_t)suid);
	}
	int sys_getresgid(gid_t* rgid, gid_t* egid, gid_t* sgid)
	{
		return syscall3_1(SYS_GETRESGID, (uint64_t)rgid, (uint64_t)egid, (uint64_t)sgid);
	}

	pid_t sys_getpid()
	{
		return (pid_t)syscall0_1(SYS_GETPID);
	}
	pid_t sys_getppid()
	{
		return (pid_t)syscall0_1(SYS_GETPPID);
	}

	int sys_gethostname(char* buffer, size_t bufsize)
	{
		return syscall2_1(SYS_GETHOSTNAME, (uint64_t)buffer, (uint64_t)bufsize);
	}

	int sys_stat(fsfd_target fsfdt, int fd, const char* path, int flags, struct stat* statbuf)
	{
		if (fsfdt == fsfd_target::none)
			return EINVAL;
		else if (fsfdt == fsfd_target::path)
			fd = AT_FDCWD;
		else if (fsfdt == fsfd_target::fd)
			flags |= AT_EMPTY_PATH;
		else 
			return ENOSYS;

		return syscall4_1(SYS_FSTATAT, (uint64_t)fd, (uint64_t)path, (uint64_t)flags, (uint64_t)statbuf);
	}

	int sys_getpgid(pid_t pid, pid_t* pgid)
	{
		uint64_t _pgid;
		int ret = syscall1_2(SYS_GETPGID, (uint64_t)pid, &_pgid);
		*pgid = (pid_t)_pgid;
		return ret;
	}
}