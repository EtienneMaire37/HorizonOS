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

char* strerror(int errnum)
{
    switch (errnum) 
    {
        case E2BIG: return "Argument list too long";
        case EACCES: return "Permission denied";
        case EADDRINUSE: return "Address already in use";
        case EADDRNOTAVAIL: return "Address not available";
        case EAFNOSUPPORT: return "Address family not supported";
        case EAGAIN: return "Resource temporarily unavailable";
        case EALREADY: return "Connection already in progress";
        case EBADF: return "Bad file descriptor";
        case EBUSY: return "Device or resource busy";
        case ECHILD: return "No child processes";
        case ECONNABORTED: return "Connection aborted";
        case ECONNREFUSED: return "Connection refused";
        case ECONNRESET: return "Connection reset";
        case EDEADLK: return "Resource deadlock avoided";
        case EDOM: return "Math argument out of domain";
        case EEXIST: return "File exists";
        case EFAULT: return "Bad address";
        case EFBIG: return "File too large";
        case EINTR: return "Interrupted system call";
        case EINVAL: return "Invalid argument";
        case EIO: return "I/O error";
        case EISDIR: return "Is a directory";
        case EMFILE: return "Too many open files";
        case ENOENT: return "No such file or directory";
        case ENOMEM: return "Out of memory";
        case ENOSPC: return "No space left on device";
        case ENOTDIR: return "Not a directory";
        case ENOTEMPTY: return "Directory not empty";
        case ENOTSUP: return "Operation not supported";
        case EPERM: return "Operation not permitted";
        case EPIPE: return "Broken pipe";
        case ERANGE: return "Result too large";
        case EROFS: return "Read-only file system";
        case ESPIPE: return "Illegal seek";
        case ETIMEDOUT: return "Connection timed out";
        case EXDEV: return "Cross-device link";
        default: return "Unknown error";
    }
}