#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <abi-bits/errno.h>

void* memset(void* dst, int value, size_t n)
{
    if (__builtin_memset != memset) return __builtin_memset(dst, value, n);
    for (size_t i = 0; i < n; i++)
        ((uint8_t*)dst)[i] = (uint8_t)value;
    return dst;
}

void* memcpy(void* dst, const void* src, size_t n)
{
    if (__builtin_memcpy != memcpy) return __builtin_memcpy(dst, src, n);
    for (size_t i = 0; i < n; i++)
        ((uint8_t*)dst)[i] = ((uint8_t*)src)[i];
    return dst;
}

void* memmove(void* dst, const void* src, size_t length)
{
    if (__builtin_memmove != memmove) return __builtin_memmove(dst, src, length);
    uint8_t data[length];
    __builtin_memcpy(data, src, length);
    __builtin_memcpy(dst, data, length);
    return dst;
}

int memcmp(const void* str1, const void* str2, size_t n)
{
    if (__builtin_memcmp != memcmp) return __builtin_memcmp(str1, str2, n);
    for(uint64_t i = 0; i < n; i++)
    {
        if(((uint8_t*)str1)[i] < ((uint8_t*)str2)[i])
            return -1;
        if(((uint8_t*)str1)[i] > ((uint8_t*)str2)[i])
            return 1;
    }

    return 0;
}

size_t strlen(const char* str)
{
    if (__builtin_strlen != strlen) return __builtin_strlen(str);
    size_t s = 0;
    while ((uint64_t)str & 3)
    {
        if (!(*str))
            return s;
        str++;
        s++;
    }

    uint32_t dword;
    while (true)
    {
        dword = *(uint32_t*)str;
        if (!(dword & 0xff))
            return s;
        if (!(dword & 0xff00))
            return s + 1;
        if (!(dword & 0xff0000))
            return s + 2;
        if (!(dword & 0xff000000))
            return s + 3;

        str += 4;
        s += 4;
    }
    __builtin_unreachable();
}

size_t strnlen(const char* str, size_t maxlen)
{
    if (__builtin_strnlen != strnlen) return __builtin_strnlen(str, maxlen);
    size_t s = 0;
    while ((uint64_t)str & 3)
    {
        if (s >= maxlen)
            return maxlen;
        if (!(*str))
            return s;
        str++;
        s++;
    }

    uint32_t dword;
    while (true)
    {
        dword = *(uint32_t*)str;
        if (s >= maxlen)
            return maxlen;
        if (!(dword & 0xff))
            return s;
        if (s + 1 >= maxlen)
            return maxlen;
        if (!(dword & 0xff00))
            return s + 1;
        if (s + 2 >= maxlen)
            return maxlen;
        if (!(dword & 0xff0000))
            return s + 2;
        if (s + 3 >= maxlen)
            return maxlen;
        if (!(dword & 0xff000000))
            return s + 3;

        str += 4;
        s += 4;
    }
    __builtin_unreachable();
}

int strcmp(const char* str1, const char* str2)
{
    if (__builtin_strcmp != strcmp) return __builtin_strcmp(str1, str2);
    const unsigned char* p1 = (const unsigned char*)str1;
    const unsigned char* p2 = (const unsigned char*)str2;

    while (*p1 && (*p1 == *p2))
    {
        p1++;
        p2++;
    }

    return (*p1 > *p2) - (*p2 > *p1);
}

char* strcpy(char* destination, const char* source)
{
    if (__builtin_strcpy != strcpy) return __builtin_strcpy(destination, source);
    char* _dest = destination;
    while (*source)
    {
        *destination = *source;
        destination++;
        source++;
    }
    (*destination) = 0;
    return _dest;
}

char* strncpy(char* destination, const char* source, size_t n)
{
    if (__builtin_strncpy != strncpy) return __builtin_strncpy(destination, source, n);
    char* _dest = destination;
    size_t i = 0;

    while(i < n && source[i] != '\0')
    {
        destination[i] = source[i];
        i++;
    }

    while(i < n)
        destination[i++] = '\0';

    return _dest;
}

char* strdup(const char* str)
{
    size_t len = strlen(str);
    char* dup = malloc(len);
    if (!dup) return NULL;
    __builtin_memcpy(dup, str, len + 1);
    return dup;
}

// * from mlibc
char *strerror(int e) {
	const char *s;
	switch(e) {
	case 0: s = "Success"; break;
	case EAGAIN: s = "Operation would block (EAGAIN)"; break;
	case EACCES: s = "Access denied (EACCESS)"; break;
	case EBADF:  s = "Bad file descriptor (EBADF)"; break;
	case EEXIST: s = "File exists already (EEXIST)"; break;
	case EFAULT: s = "Access violation (EFAULT)"; break;
	case EINTR:  s = "Operation interrupted (EINTR)"; break;
	case EINVAL: s = "Invalid argument (EINVAL)"; break;
	case EIO:    s = "I/O error (EIO)"; break;
	case EISDIR: s = "Resource is directory (EISDIR)"; break;
	case ENOENT: s = "No such file or directory (ENOENT)"; break;
	case ENOMEM: s = "Out of memory (ENOMEM)"; break;
	case ENOTDIR: s = "Expected directory instead of file (ENOTDIR)"; break;
	case ENOSYS: s = "Operation not implemented (ENOSYS)"; break;
	case EPERM:  s = "Operation not permitted (EPERM)"; break;
	case EPIPE:  s = "Broken pipe (EPIPE)"; break;
	case ESPIPE: s = "Seek not possible (ESPIPE)"; break;
	case ENXIO: s = "No such device or address (ENXIO)"; break;
	case ENOEXEC: s = "Exec format error (ENOEXEC)"; break;
	case ENOSPC: s = "No space left on device (ENOSPC)"; break;
	case ENOTSOCK: s = "Socket operation on non-socket (ENOTSOCK)"; break;
	case ENOTCONN: s = "Transport endpoint is not connected (ENOTCONN)"; break;
	case EDOM: s = "Numerical argument out of domain (EDOM)"; break;
	case EILSEQ: s = "Invalid or incomplete multibyte or wide character (EILSEQ)"; break;
	case ERANGE: s = "Numerical result out of range (ERANGE)"; break;
	case E2BIG: s = "Argument list too long (E2BIG)"; break;
	case EADDRINUSE: s = "Address already in use (EADDRINUSE)"; break;
	case EADDRNOTAVAIL: s = "Cannot assign requested address (EADDRNOTAVAIL)"; break;
	case EAFNOSUPPORT: s = "Address family not supported by protocol (EAFNOSUPPORT)"; break;
	case EALREADY: s = "Operation already in progress (EALREADY)"; break;
	case EBADMSG: s = "Bad message (EBADMSG)"; break;
	case EBUSY: s = "Device or resource busy (EBUSY)"; break;
	case ECANCELED: s = "Operation canceled (ECANCELED)"; break;
	case ECHILD: s = "No child processes (ECHILD)"; break;
	case ECONNABORTED: s = "Software caused connection abort (ECONNABORTED)"; break;
	case ECONNREFUSED: s = "Connection refused (ECONNREFUSED)"; break;
	case ECONNRESET: s = "Connection reset by peer (ECONNRESET)"; break;
	case EDEADLK: s = "Resource deadlock avoided (EDEADLK)"; break;
	case EDESTADDRREQ: s = "Destination address required (EDESTADDRREQ)"; break;
	case EDQUOT: s = "Disk quota exceeded (EDQUOT)"; break;
	case EFBIG: s = "File too large (EFBIG)"; break;
	case EHOSTUNREACH: s = "No route to host (EHOSTUNREACH)"; break;
	case EIDRM: s = "Identifier removed (EIDRM)"; break;
	case EINPROGRESS: s = "Operation now in progress (EINPROGRESS)"; break;
	case EISCONN: s = "Transport endpoint is already connected (EISCONN)"; break;
	case ELOOP: s = "Too many levels of symbolic links (ELOOP)"; break;
	case EMFILE: s = "Too many open files (EMFILE)"; break;
	case EMLINK: s = "Too many links (EMLINK)"; break;
	case EMSGSIZE: s = "Message too long (EMSGSIZE)"; break;
	case EMULTIHOP: s = "Multihop attempted (EMULTIHOP)"; break;
	case ENAMETOOLONG: s = "File name too long (ENAMETOOLONG)"; break;
	case ENETDOWN: s = "Network is down (ENETDOWN)"; break;
	case ENETRESET: s = "Network dropped connection on reset (ENETRESET)"; break;
	case ENETUNREACH: s = "Network is unreachable (ENETUNREACH)"; break;
	case ENFILE: s = "Too many open files in system (ENFILE)"; break;
	case ENOBUFS: s = "No buffer space available (ENOBUFS)"; break;
	case ENODEV: s = "No such device (ENODEV)"; break;
	case ENOLCK: s = "No locks available (ENOLCK)"; break;
	case ENOLINK: s = "Link has been severed (ENOLINK)"; break;
	case ENOMSG: s = "No message of desired type (ENOMSG)"; break;
	case ENOPROTOOPT: s = "Protocol not available (ENOPROTOOPT)"; break;
	case ENOTEMPTY: s = "Directory not empty (ENOTEMPTY)"; break;
	case ENOTRECOVERABLE: s = "Sate not recoverable (ENOTRECOVERABLE)"; break;
	case ENOTSUP: s = "Operation not supported (ENOTSUP)"; break;
	case ENOTTY: s = "Inappropriate ioctl for device (ENOTTY)"; break;
	case EOVERFLOW: s = "Value too large for defined datatype (EOVERFLOW)"; break;
#if EOPNOTSUPP != ENOTSUP
	/* these are aliases on the mlibc abi */
	case EOPNOTSUPP: s = "Operation not supported (EOPNOTSUP)"; break;
#endif
	case EOWNERDEAD: s = "Owner died (EOWNERDEAD)"; break;
	case EPROTO: s = "Protocol error (EPROTO)"; break;
	case EPROTONOSUPPORT: s = "Protocol not supported (EPROTONOSUPPORT)"; break;
	case EPROTOTYPE: s = "Protocol wrong type for socket (EPROTOTYPE)"; break;
	case EROFS: s = "Read-only file system (EROFS)"; break;
	case ESRCH: s = "No such process (ESRCH)"; break;
	case ESTALE: s = "Stale file handle (ESTALE)"; break;
	case ETIMEDOUT: s = "Connection timed out (ETIMEDOUT)"; break;
	case ETXTBSY: s = "Text file busy (ETXTBSY)"; break;
	case EXDEV: s = "Invalid cross-device link (EXDEV)"; break;
	case ENODATA: s = "No data available (ENODATA)"; break;
	case ETIME: s = "Timer expired (ETIME)"; break;
	case ENOKEY: s = "Required key not available (ENOKEY)"; break;
	case ESHUTDOWN: s = "Cannot send after transport endpoint shutdown (ESHUTDOWN)"; break;
	case EHOSTDOWN: s = "Host is down (EHOSTDOWN)"; break;
	case EBADFD: s = "File descriptor in bad state (EBADFD)"; break;
	case ENOMEDIUM: s = "No medium found (ENOMEDIUM)"; break;
	case ENOTBLK: s = "Block device required (ENOTBLK)"; break;
	case ENONET: s = "Machine is not on the network (ENONET)"; break;
	case EPFNOSUPPORT: s = "Protocol family not supported (EPFNOSUPPORT)"; break;
	case ESOCKTNOSUPPORT: s = "Socket type not supported (ESOCKTNOSUPPORT)"; break;
	case ESTRPIPE: s = "Streams pipe error (ESTRPIPE)"; break;
	case EREMOTEIO: s = "Remote I/O error (EREMOTEIO)"; break;
	case ERFKILL: s = "Operation not possible due to RF-kill (ERFKILL)"; break;
	case EBADR: s = "Invalid request descriptor (EBADR)"; break;
	case EUNATCH: s = "Protocol driver not attached (EUNATCH)"; break;
	case EMEDIUMTYPE: s = "Wrong medium type (EMEDIUMTYPE)"; break;
	case EREMOTE: s = "Object is remote (EREMOTE)"; break;
	case EKEYREJECTED: s = "Key was rejected by service (EKEYREJECTED)"; break;
	case EUCLEAN: s = "Structure needs cleaning (EUCLEAN)"; break;
	case EBADSLT: s = "Invalid slot (EBADSLT)"; break;
	case ENOANO: s = "No anode (ENOANO)"; break;
	case ENOCSI: s = "No CSI structure available (ENOCSI)"; break;
	case ENOSTR: s = "Device not a stream (ENOSTR)"; break;
	case ETOOMANYREFS: s = "Too many references: cannot splice (ETOOMANYREFS)"; break;
	case ENOPKG: s = "Package not installed (ENOPKG)"; break;
	case EKEYREVOKED: s = "Key has been revoked (EKEYREVOKED)"; break;
	case EXFULL: s = "Exchange full (EXFULL)"; break;
	case ELNRNG: s = "Link number out of range (ELNRNG)"; break;
	case ENOTUNIQ: s = "Name not unique on network (ENOTUNIQ)"; break;
	case ERESTART: s = "Interrupted system call should be restarted (ERESTART)"; break;
	case EUSERS: s = "Too many users (EUSERS)"; break;

#ifdef EIEIO
	case EIEIO: s = "Computer bought the farm; OS internal error (EIEIO)"; break;
#endif

	default:
		s = "Unknown error code (?)";
	}
	return (char*)(s);
}
