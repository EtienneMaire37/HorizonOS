#include "mlibc/tcb.hpp"
#include <abi-bits/errno.h>
#include <bits/ensure.h>
#include <bits/syscall.h>
#include <mlibc/all-sysdeps.hpp>
#include <pwd.h>
#include <termios.h>
#include <asm-generic/ioctls.h>

#include "syscall.hpp"
#include "stub.h"

namespace mlibc
{
	void sys_libc_panic()
	{
        const char* str_print = "\n\x1b[31m[[!!! mlibc panic !!!]]\x1b[0m (see logs)\n";
	    const char* str_log = "[[!!! mlibc panic !!!]]";
		ssize_t bytes;
		sys_write(2, str_print, strlen(str_print), &bytes);
		sys_libc_log(str_log);
		sys_exit(-1);
		__builtin_trap();
	}

	void sys_libc_log(const char* msg)
	{
	    syscall1_0(SYS_LOG, (uint64_t)msg);
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

	int sys_vm_protect(void* pointer, size_t size, int prot)
	{
		return syscall3_1(SYS_VM_PROTECT, (uint64_t)pointer, (uint64_t)size, (uint64_t)prot);
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
		// * from linux
		if (fsfdt == fsfd_target::path)
			fd = AT_FDCWD;
		else if (fsfdt == fsfd_target::fd)
			flags |= AT_EMPTY_PATH;
		else
			__ensure(fsfdt == fsfd_target::fd_path);

		return syscall4_1(SYS_FSTATAT, (uint64_t)fd, (uint64_t)path, (uint64_t)flags, (uint64_t)statbuf);
	}

	int sys_getpgid(pid_t pid, pid_t* pgid)
	{
		uint64_t _pgid;
		int ret = syscall1_2(SYS_GETPGID, (uint64_t)pid, &_pgid);
		*pgid = (pid_t)_pgid;
		return ret;
	}

	int sys_setpgid(pid_t pid, pid_t pgid)
	{
		return syscall2_1(SYS_SETPGID, (uint64_t)pid, (uint64_t)pgid);
	}

	int sys_dup(int fd, int flags, int* newfd)
	{
		// * do the same as in the linux sysdeps.cpp
		__ensure(!flags);
		uint64_t _newfd;
		int ret = syscall1_2(SYS_DUP, (uint64_t)fd, &_newfd);
		*newfd = (int)_newfd;
		return ret;
	}

	int sys_dup2(int fd, int flags, int newfd)
	{
		return syscall3_1(SYS_DUP3, (uint64_t)fd, (uint64_t)flags, (uint64_t)newfd);
	}

	// * same code as in posix/generic/unistd.cpp but without the logs
	long sysconf_helper(int number, int* _errno)
	{
		/* default return values, if not overriden by sysdep */
		switch(number)
		{
		case _SC_ARG_MAX:
			// On linux, it is defined to 2097152 in most cases, so define it to be 2097152
			return 2097152;
		case _SC_PAGE_SIZE:
			return 4096;
		case _SC_OPEN_MAX:
			return 256;
		case _SC_COLL_WEIGHTS_MAX:
			return COLL_WEIGHTS_MAX;
		case _SC_TZNAME_MAX:
			return -1;
		case _SC_PHYS_PAGES:
#if __MLIBC_LINUX_OPTION
			if(mlibc::sys_sysinfo) {
				struct sysinfo info{};
				if(mlibc::sys_sysinfo(&info) == 0)
					return info.totalram * info.mem_unit / 4096;
			}
#endif
			return 1024;
		case _SC_AVPHYS_PAGES:
#if __MLIBC_LINUX_OPTION
			if(mlibc::sys_sysinfo) {
				struct sysinfo info{};
				if(mlibc::sys_sysinfo(&info) == 0)
					return info.freeram * info.mem_unit / 4096;
			}
#endif
			return 1024;
		case _SC_NPROCESSORS_ONLN:
			return 1;
		case _SC_GETPW_R_SIZE_MAX:
			return NSS_BUFLEN_PASSWD;
		case _SC_GETGR_R_SIZE_MAX:
			return 1024;
		case _SC_CHILD_MAX:
			// On linux, it is defined to 25 in most cases, so define it to be 25
			return 25;
		case _SC_JOB_CONTROL:
			// If 1, job control is supported
			return 1;
		case _SC_CLK_TCK:
			// TODO: This should be obsolete?
			return 1000000;
		case _SC_NGROUPS_MAX:
			// On linux, it is defined to 65536 in most cases, so define it to be 65536
			return 65536;
		case _SC_RE_DUP_MAX:
			return RE_DUP_MAX;
		case _SC_LINE_MAX:
			// Linux defines it as 2048.
			return 2048;
		case _SC_XOPEN_CRYPT:
			return -1;
		case _SC_NPROCESSORS_CONF:
			// TODO: actually return a proper value for _SC_NPROCESSORS_CONF
			return 1;
		case _SC_HOST_NAME_MAX:
			return HOST_NAME_MAX;
		case _SC_LOGIN_NAME_MAX:
			return LOGIN_NAME_MAX;
		case _SC_FSYNC:
			return _POSIX_FSYNC;
		case _SC_SAVED_IDS:
			return _POSIX_SAVED_IDS;
		case _SC_SYMLOOP_MAX:
			return 8;
		case _SC_VERSION:
			return _POSIX_VERSION;
		case _SC_2_VERSION:
			return _POSIX2_VERSION;
		case _SC_XOPEN_VERSION:
			return _XOPEN_VERSION;
		case _SC_MEMLOCK:
			return _POSIX_MEMLOCK;
		case _SC_MEMLOCK_RANGE:
			return _POSIX_MEMLOCK_RANGE;
		case _SC_MAPPED_FILES:
			return _POSIX_MAPPED_FILES;
		case _SC_SHARED_MEMORY_OBJECTS:
			return _POSIX_SHARED_MEMORY_OBJECTS;
		default:
			*_errno = EINVAL;
			return -1;
		}
	}

	int sys_sysconf(int num, long* ret)
	{
		int _errno;
		*ret = sysconf_helper(num, &_errno);
		return _errno;
	}

	int sys_fcntl(int fd, int request, va_list args, int* result)
	{
		uint64_t arg = va_arg(args, uint64_t);
		uint64_t _result;
		int ret = syscall3_2(SYS_FCNTL, (uint64_t)fd, (uint64_t)request, arg, &_result);
		*result = (int)_result;
		return ret;
	}

	int sys_tcgetwinsize(int fd, struct winsize* winsz)
	{
		int ret;
		return sys_ioctl(fd, TIOCGWINSZ, winsz, &ret);
	}

	int sys_tcsetwinsize(int fd, const struct winsize* winsz)
	{
		int ret;
		return sys_ioctl(fd, TIOCSWINSZ, (struct winsize*)winsz, &ret);
	}

	int sys_pselect(int num_fds, fd_set* read_set, fd_set* write_set, fd_set* except_set, const struct timespec* timeout, const sigset_t* sigmask, int* num_events)
	{
		uint64_t _num_events;
		int ret = syscall6_2(SYS_PSELECT, (uint64_t)num_fds, (uint64_t)read_set, (uint64_t)write_set, (uint64_t)except_set, (uint64_t)timeout, (uint64_t)sigmask, &_num_events);
		*num_events = (int)_num_events;
		return ret;
	}

	int sys_open_dir(const char* path, int* handle)
	{
		uint64_t _handle;
		int ret = syscall1_2(SYS_OPEN_DIR, (uint64_t)path, &_handle);
		*handle = (int)_handle;
		return ret;
	}
	int sys_read_entries(int handle, void* buffer, size_t max_size, size_t* bytes_read)
	{
		uint64_t _bytes_read;
		int ret = syscall3_2(SYS_READ_ENTRIES, (uint64_t)handle, (uint64_t)buffer, (uint64_t)max_size, &_bytes_read);
		*bytes_read = (size_t)_bytes_read;
		return ret;
	}

	int sys_access(const char* path, int mode)
	{
		return syscall2_1(SYS_ACCESS, (uint64_t)path, (uint64_t)mode);
	}

	gid_t sys_getgid()
	{
		gid_t rgid, egid, sgid;
		int ret = sys_getresgid(&rgid, &egid, &sgid);
		return rgid;
	}
	gid_t sys_getegid()
	{
		gid_t rgid, egid, sgid;
		int ret = sys_getresgid(&rgid, &egid, &sgid);
		return egid;
	}
	uid_t sys_getuid()
	{
		uid_t ruid, euid, suid;
		int ret = sys_getresuid(&ruid, &euid, &suid);
		return ruid;
	}
	uid_t sys_geteuid()
	{
		uid_t ruid, euid, suid;
		int ret = sys_getresuid(&ruid, &euid, &suid);
		return euid;
	}

	int sys_fadvise(int fd, off_t offset, off_t length, int advice)
	{
		return syscall4_1(SYS_FADVISE, (uint64_t)fd, (uint64_t)offset, (uint64_t)length, (uint64_t)advice);
	}

	int sys_lgetxattr(const char* path, const char* name, void* val, size_t size, ssize_t* nread)
	{
		*nread = 0;
		return 0;
	}

	int sys_listxattr(const char* path, char* list, size_t size, ssize_t* nread)
	{
		*nread = 0;
		return 0;
	}

	int sys_llistxattr(const char* path, char* list, size_t size, ssize_t* nread)
	{
		*nread = 0;
		return 0;
	}

	int sys_pipe(int* fds, int flags)
	{
		return syscall2_1(SYS_PIPE2, (uint64_t)fds, (uint64_t)flags);
	}

	int sys_tcflow(int fd, int action)
	{
		int res;
		return sys_ioctl(fd, TCXONC, (void*)action, &res);
	}

	int sys_kill(int pid, int sig)
	{
	    return syscall2_1(SYS_KILL, (uint64_t)pid, (uint64_t)sig);
	}

	int sys_uname(struct utsname* buf)
	{
	    return syscall1_1(SYS_UNAME, (uint64_t)buf);
	}

	int sys_fsync(int fd)
	{
	    return syscall1_1(SYS_FSYNC, (uint64_t)fd);
	}

	int sys_sleep(time_t* secs, long* nanos)
	{
		return syscall2_1(SYS_SLEEP, (uint64_t)secs, (uint64_t)nanos);
	}

	int sys_poll(struct pollfd* fds, nfds_t count, int timeout, int* num_events)
	{
	    uint64_t _num_events;
        int ret = syscall3_2(SYS_POLL, (uint64_t)fds, (uint64_t)count, (uint64_t)timeout, &_num_events);
        *num_events = (int)_num_events;
        return ret;
	}
}
