#pragma once

#include "types.h"

struct stat 
{
    dev_t     st_dev;      // ID of device containing file
    ino_t     st_ino;      // Inode number
    mode_t    st_mode;     // File mode
    nlink_t   st_nlink;    // Number of hard links
    uid_t     st_uid;      // User ID of owner
    gid_t     st_gid;      // Group ID of owner
    dev_t     st_rdev;     // Device ID (if special file)
    off_t     st_size;     // Total size in bytes
    blksize_t st_blksize;  // Block size for I/O
    blkcnt_t  st_blocks;   // Number of allocated blocks
    time_t    st_atime;    // Time of last access
    time_t    st_mtime;    // Time of last modification
    time_t    st_ctime;    // Time of last status change
};

//*  Bitmask for file type
#define S_IFMT   0xf000

#define S_IFSOCK 0xc000
#define S_IFLNK  0xa000
#define S_IFREG  0x8000
#define S_IFBLK  0x6000
#define S_IFDIR  0x4000
#define S_IFCHR  0x2000
#define S_IFIFO  0x1000

#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)


#define S_IRWXU  00700

#define S_IRUSR  00400
#define S_IWUSR  00200
#define S_IXUSR  00100


#define S_IRWXG  00070

#define S_IRGRP  00040
#define S_IWGRP  00020
#define S_IXGRP  00010


#define S_IRWXO  00007

#define S_IROTH  00004
#define S_IWOTH  00002
#define S_IXOTH  00001


#define S_ISUID  04000
#define S_ISGID  02000
#define S_ISVTX  01000

static mode_t fd_creation_mask;

int stat(const char* path, struct stat* buf);
int fstat(int fd, struct stat* buf);
// int lstat(const char* path, struct stat* buf);
int chmod(const char* path, mode_t mode);
int mkdir(const char* path, mode_t mode);
mode_t umask(mode_t mask);