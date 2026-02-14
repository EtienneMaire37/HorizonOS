#pragma once

#include <dirent.h>
#include "../multitasking/mutex.h"
#include "../initrd/initrd.h"
#include <sys/stat.h>

typedef int16_t file_table_index_t;
static const file_table_index_t invalid_fd = -1;

typedef enum drive_type
{
    DT_INVALID = 0,
    DT_VIRTUAL = 1,
    DT_INITRD = 2,
} drive_type_t;

static inline const char* get_drive_type_string(drive_type_t dt)
{
    if (dt > 2 || dt < 0) dt = 0;
    return (const char*[]){"DT_INVALID", "DT_VIRTUAL", "DT_INITRD"}[dt];
}

typedef struct 
{
    drive_type_t type;
    union 
    {
        ;
    } data;
} drive_t;

// * VFS data

#define VFS_NODE_INIT       0

#define VFS_NODE_EXPLORED   1
#define VFS_NODE_LOADING    2
#define VFS_NODE_MOUNTPOINT 4

typedef struct vfs_file_inode vfs_file_inode_t;

typedef struct vfs_file_tnode vfs_file_tnode_t;
typedef struct vfs_folder_tnode vfs_folder_tnode_t;

typedef struct file_entry file_entry_t;

typedef struct 
{
    uint8_t ide_idx, ata_idx;
} ide_descriptor_t;

// * i-nodes
typedef struct vfs_file_inode
{
    drive_t drive;

    union
    {
        initrd_file_t* initrd;
        ide_descriptor_t ide;
    } file_data;

    ssize_t (*io_func)(file_entry_t*, uint8_t* buf, size_t count, uint8_t direction);

    struct stat st;

    vfs_folder_tnode_t* parent;
} vfs_file_inode_t;

typedef struct
{
    drive_t drive;

    struct stat st;

    vfs_file_tnode_t* files;
    vfs_folder_tnode_t* folders;

    uint8_t flags;
    mutex_t lock;

    vfs_folder_tnode_t* parent;
} vfs_folder_inode_t;

// * t-nodes
typedef struct vfs_file_tnode
{
    char* name;
    vfs_file_inode_t* inode;

    struct vfs_file_tnode* next;
} vfs_file_tnode_t;

typedef struct vfs_folder_tnode
{
    char* name;
    vfs_folder_inode_t* inode;

    struct vfs_folder_tnode* next;
} vfs_folder_tnode_t;

// * Open file descriptor data

#define ET_FILE     1
#define ET_FOLDER   2

#define CHR_MODE    (S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)
#define BLK_MODE    (S_IFBLK | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

typedef struct file_entry
{
    int used;
    int flags;
    off_t position;
    uint8_t entry_type;
    union
    {
        vfs_file_tnode_t* file;
        vfs_folder_tnode_t* folder;
    } tnode;
} file_entry_t;

#define MAX_FILE_TABLE_ENTRIES  1024

#define CHR_DIR_READ    1
#define CHR_DIR_WRITE   2

extern file_entry_t file_table[MAX_FILE_TABLE_ENTRIES];
extern vfs_folder_tnode_t* vfs_root;

vfs_file_inode_t* vfs_get_file_inode(const char* path, vfs_folder_tnode_t* pwd);
vfs_file_tnode_t* vfs_get_file_tnode(const char* path, vfs_folder_tnode_t* pwd);
vfs_folder_tnode_t* vfs_get_folder_tnode(const char* path, vfs_folder_tnode_t* pwd);

void vfs_init_file_table();
int vfs_allocate_global_file();
void vfs_remove_global_file(int fd);

ino_t vfs_generate_inode_number();
dev_t vfs_generate_device_id();

vfs_folder_inode_t* vfs_create_empty_folder_inode(vfs_folder_tnode_t* parent, uint8_t flags,
    dev_t device_id, mode_t mode, uid_t uid, gid_t gid,
    drive_t drive);
vfs_folder_tnode_t* vfs_create_empty_folder_tnode(const char* name, vfs_folder_tnode_t* parent, uint8_t flags,
    dev_t device_id, mode_t mode, uid_t uid, gid_t gid,
    drive_t drive);
vfs_file_inode_t* vfs_create_special_file_inode(vfs_folder_tnode_t* parent, mode_t mode, ssize_t (*fun)(file_entry_t*, uint8_t*, size_t, uint8_t), uid_t uid, gid_t gid);
vfs_file_tnode_t* vfs_create_special_file_tnode(const char* name, vfs_folder_tnode_t* parent, mode_t mode, ssize_t (*fun)(file_entry_t*, uint8_t*, size_t, uint8_t), uid_t uid, gid_t gid);
void vfs_mount_device(const char* name, const char* path, drive_t drive, uid_t uid, gid_t gid);
void vfs_unload_folder(vfs_folder_tnode_t* tnode);

void vfs_explore(vfs_folder_tnode_t* tnode);

ssize_t task_chr_stdin(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction);
ssize_t task_chr_stdout(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction);
ssize_t task_chr_stderr(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction);
ssize_t task_chr_tty(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction);

ssize_t initrd_iofunc(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction);

static inline bool vfs_isatty(file_entry_t* entry)
{
    if (!entry) return false;
    return entry->entry_type == ET_FILE ? (S_ISCHR(entry->tnode.file->inode->st.st_mode) && (entry->tnode.file->inode->io_func == task_chr_stdin || entry->tnode.file->inode->io_func == task_chr_stdout || entry->tnode.file->inode->io_func == task_chr_stderr || entry->tnode.file->inode->io_func == task_chr_tty)) : false;
}

vfs_file_tnode_t* vfs_add_special(const char* folder, const char* name, mode_t mode, ssize_t (*fun)(file_entry_t*, uint8_t*, size_t, uint8_t),
    uid_t uid, gid_t gid);

ssize_t vfs_realpath_from_folder_tnode(vfs_folder_tnode_t* tnode, char* res);
ssize_t vfs_realpath_from_file_tnode(vfs_file_tnode_t* tnode, char* res);

bool file_string_cmp(const char* s1, const char* s2);

int vfs_root_stat(struct stat* st);
int vfs_stat(const char* path, vfs_folder_tnode_t* pwd, struct stat* st);
int vfs_fstat(int fd, vfs_folder_tnode_t* pwd, struct stat* st);
int vfs_access(const char* path, vfs_folder_tnode_t* pwd, mode_t mode);
struct dirent* vfs_readdir(struct dirent* dirent, DIR* dirp);

int vfs_read(int fd, void* buffer, size_t num_bytes, ssize_t* bytes_read);
int vfs_write(int fd, const char* buffer, uint64_t bytes_to_write, ssize_t* bytes_written);

void vfs_log_tree();