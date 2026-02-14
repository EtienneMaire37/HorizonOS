#include "vfs.h"
#include <dirent.h>
#include "../multitasking/mutex.h"
#include "../initrd/initrd.h"
#include <sys/stat.h>
#include "../initrd/vfs.h"
#include "../multitasking/task.h"
#include "../multitasking/multitasking.h"
#include <stdlib.h>
// #include <errno.h>
extern int errno;
#include <abi-bits/errno.h>
#include <fcntl.h>
#include "../util/math.h"
#include "../vga/textio.h"
#include "../util/access.h"

file_entry_t file_table[MAX_FILE_TABLE_ENTRIES];
vfs_folder_tnode_t* vfs_root = NULL;

void vfs_init_file_table()
{
    acquire_mutex(&file_table_lock);
    for (int i = 0; i < MAX_FILE_TABLE_ENTRIES; i++)
    {
        if (i < 3)  file_table[i].used = 1; // * Should always be used
        else        file_table[i].used = 0;
    }
    
    file_table[0].entry_type = ET_FILE;
    file_table[0].tnode.file = vfs_get_file_tnode("/dev/tty", NULL);
    file_table[0].position = 0;
    file_table[0].flags = O_RDONLY;

    file_table[1].entry_type = ET_FILE;
    file_table[1].tnode.file = vfs_get_file_tnode("/dev/tty", NULL);
    file_table[1].position = 0;
    file_table[1].flags = O_WRONLY;

    file_table[2].entry_type = ET_FILE;
    file_table[2].tnode.file = vfs_get_file_tnode("/dev/tty", NULL);
    file_table[2].position = 0;
    file_table[2].flags = O_WRONLY;

    release_mutex(&file_table_lock);
}

int vfs_allocate_global_file()
{
	lock_scheduler();
    acquire_mutex(&file_table_lock);
    for (int i = 3; i < MAX_FILE_TABLE_ENTRIES; i++)
    {
        if (file_table[i].used == 0)
        {
            file_table[i].used = 1;
            release_mutex(&file_table_lock);
			unlock_scheduler();
            return i;
        }
    }
    release_mutex(&file_table_lock);
	unlock_scheduler();
    return -1;
}

void vfs_remove_global_file(int fd)
{
	lock_scheduler();
    acquire_mutex(&file_table_lock);
    
    if (fd >= 3 && fd < MAX_FILE_TABLE_ENTRIES)
    {
        file_table[fd].used--;
        if (file_table[fd].used <= 0)
            file_table[fd].used = 0;
            
        release_mutex(&file_table_lock);
		unlock_scheduler();
        return;
    }
    
    release_mutex(&file_table_lock);
	unlock_scheduler();
}

ino_t vfs_generate_inode_number()
{
    static ino_t current_inode_number = 1;
    return current_inode_number++;
}

dev_t vfs_generate_device_id()
{
    static dev_t current_device_id = 1;
    return current_device_id++;
}

vfs_folder_inode_t* vfs_create_empty_folder_inode(vfs_folder_tnode_t* parent, uint8_t flags,
    dev_t device_id, mode_t mode, uid_t uid, gid_t gid,
    drive_t drive)
{
    vfs_folder_inode_t* inode = malloc(sizeof(vfs_folder_inode_t));

    inode->files = NULL;
    inode->folders = NULL;

    inode->flags = flags;

    inode->st.st_dev = device_id;
    inode->st.st_ino = vfs_generate_inode_number();
    inode->st.st_mode = mode;
    inode->st.st_nlink = 1;
    inode->st.st_uid = uid;
    inode->st.st_gid = gid;
    inode->st.st_rdev = 0;
    inode->st.st_size = 0;
    inode->st.st_blksize = 0;
    inode->st.st_blocks = 0;
    inode->st.st_atime = 0;
    inode->st.st_mtime = 0;
    inode->st.st_ctime = 0;

    inode->drive = drive;

    inode->parent = parent;
    inode->lock = MUTEX_INIT;

    return inode;
}

vfs_folder_tnode_t* vfs_create_empty_folder_tnode(const char* name, vfs_folder_tnode_t* parent, uint8_t flags,
    dev_t device_id, mode_t mode, uid_t uid, gid_t gid,
    drive_t drive)
{
    vfs_folder_tnode_t* tnode = malloc(sizeof(vfs_folder_tnode_t));
    if (!tnode)
    {
        LOG(ERROR, "Couldn't allocate tnode");
        return NULL;
    }
    tnode->name = strdup(name);
    if (!tnode->name)
    {
        free(tnode);
        LOG(ERROR, "Couldn't allocate name");
        return NULL;
    }
    tnode->next = NULL;
    tnode->inode = vfs_create_empty_folder_inode(parent, flags, device_id, mode, uid, gid, drive);
    if (!tnode->inode)
    {
        free(tnode->name);
        free(tnode);
        LOG(ERROR, "Couldn't allocate inode");
        return NULL;
    }
    return tnode;
}

vfs_file_inode_t* vfs_create_special_file_inode(vfs_folder_tnode_t* parent, mode_t mode, ssize_t (*fun)(file_entry_t*, uint8_t*, size_t, uint8_t), uid_t uid, gid_t gid)
{
    vfs_file_inode_t* inode = malloc(sizeof(vfs_file_inode_t));
    if (!inode) return NULL;

    inode->drive.type = DT_VIRTUAL;
    inode->io_func = fun;
    inode->parent = parent;
    
    inode->st.st_dev = 0;   // * root device
    inode->st.st_ino = vfs_generate_inode_number();
    inode->st.st_mode = mode;
    inode->st.st_nlink = 1;
    inode->st.st_uid = uid;
    inode->st.st_gid = gid;
    inode->st.st_rdev = vfs_generate_device_id();
    inode->st.st_size = 0;
    inode->st.st_blksize = 0;
    inode->st.st_blocks = 0;
    inode->st.st_atime = 0;
    inode->st.st_mtime = 0;
    inode->st.st_ctime = 0;

    return inode;
}

vfs_file_tnode_t* vfs_create_special_file_tnode(const char* name, vfs_folder_tnode_t* parent, mode_t mode, ssize_t (*fun)(file_entry_t*, uint8_t*, size_t, uint8_t), uid_t uid, gid_t gid)
{
    vfs_file_tnode_t* tnode = malloc(sizeof(vfs_file_tnode_t));
    if (!tnode)
    {
        LOG(ERROR, "Couldn't allocate tnode");
        return NULL;
    }
    tnode->name = strdup(name);
    if (!tnode->name)
    {
        free(tnode);
        LOG(ERROR, "Couldn't allocate name");
        return NULL;
    }
    tnode->next = NULL;
    tnode->inode = vfs_create_special_file_inode(parent, mode, fun, uid, gid);
    if (!tnode->inode)
    {
        free(tnode->name);
        free(tnode);
        LOG(ERROR, "Couldn't allocate inode");
        return NULL;
    }
    return tnode;
}

void vfs_mount_device(const char* name, const char* path, drive_t drive, uid_t uid, gid_t gid)
{
    LOG(DEBUG, "Mounting device \"%s\" (%s) at \"%s\"", name, get_drive_type_string(drive.type), path);

    vfs_folder_tnode_t* parent = vfs_get_folder_tnode(path, NULL);

    if (!parent)
    {
        LOG(ERROR, "vfs_mount_device: Couldn't mount partition: Nonexistent parent folder");
        return;
    }
    
    acquire_mutex(&parent->inode->lock);

    vfs_folder_tnode_t** current = &parent->inode->folders;

    if (!(*current)) 
        goto mount;

    while (*current)
    {
        if (strcmp(name, (*current)->name) == 0)
        {
            LOG(ERROR, "vfs_mount_device: Couldn't mount partition: Mount point already exists");
            release_mutex(&parent->inode->lock);
            return;
        }

        current = &(*current)->next;
    }

mount: 
    (*current) = vfs_create_empty_folder_tnode(name, parent, 
            VFS_NODE_INIT | VFS_NODE_MOUNTPOINT, 
            drive.type == DT_VIRTUAL ? 0 : vfs_generate_device_id(), 
            S_IFDIR | 
            S_IRUSR | S_IXUSR |
            S_IRGRP | S_IXGRP |
            S_IROTH | S_IXOTH, 
            uid, gid,
            drive);
    
    release_mutex(&parent->inode->lock);

    return;
}

static void vfs_unload_folder_helper(vfs_folder_tnode_t* tnode)
{
    if (!tnode) return;
    if (tnode->inode->parent == tnode) return;

    acquire_mutex(&tnode->inode->lock);

    while (tnode->inode->folders)
    {
        vfs_folder_tnode_t* next_folder_tnode = tnode->inode->folders->next;
        vfs_unload_folder_helper(tnode->inode->folders);
        tnode->inode->folders = next_folder_tnode;
    }

    while (tnode->inode->files)
    {
        vfs_file_tnode_t* file_tnode = tnode->inode->files; 
        free(file_tnode->inode);
        free(file_tnode->name);
        tnode->inode->files = tnode->inode->files->next;
        free(file_tnode);
    }

    // * useless
    // release_mutex(&tnode->inode->lock);

    free(tnode->inode);
    free(tnode->name);
    free(tnode);
}

void vfs_unload_folder(vfs_folder_tnode_t* tnode)
{
    vfs_folder_tnode_t* parent = tnode->inode->parent;
    acquire_mutex(&parent->inode->lock);
    vfs_folder_tnode_t** current_folder = &parent->inode->folders;
    while (*current_folder && (*current_folder) != tnode)
        current_folder = &(*current_folder)->next;
    if (!*current_folder)
        abort();     // !!! Should be impossible
    *current_folder = tnode->next;
    release_mutex(&parent->inode->lock);
    vfs_unload_folder_helper(tnode);
}

bool file_string_cmp(const char* s1, const char* s2)
{
    if (s1 == s2) return true;
    if (s1 == NULL || s2 == NULL) return false;
    while (*s1 && *s2 && *s1 != '/' && *s2 != '/')
    {
        if (*s1 != *s2) 
            return false;
        s1++;
        s2++;
    }
    return ((!(*s1) || (*s1 == '/')) && (!(*s2) || (*s2 == '/')));
}

size_t file_string_len(const char* str)
{
    size_t s = 0;
    while ((uint64_t)str & 3)
    {
        if ((*str == 0) || (*str == '/'))
            return s;
        str++;
        s++;
    }

    uint32_t dword;
    while (true)
    {
        dword = *(uint32_t*)str;
        if ((dword & 0xff) == 0 || (dword & 0xff) == '/')
            return s;
        if ((dword & 0xff00) == 0 || ((dword & 0xff00) >> 8) == '/')
            return s + 1;
        if ((dword & 0xff0000) == 0 || ((dword & 0xff0000) >> 16) == '/')
            return s + 2;
        if ((dword & 0xff000000) == 0 || ((dword & 0xff000000) >> 24) == '/')
            return s + 3;

        str += 4;
        s += 4;
    }
}

static size_t vfs_realpath_from_folder_tnode_helper(vfs_folder_tnode_t* tnode, char* res, size_t idx)
{
    if (!tnode || !tnode->inode) return idx;
    if (tnode == vfs_root)
        return 0;
    
    idx = vfs_realpath_from_folder_tnode_helper(tnode->inode->parent, res, idx);
    if (idx + 1 >= PATH_MAX) return idx;
    res[idx] = '/';
    size_t len = strlen(tnode->name);
    if (len + idx + 1 >= PATH_MAX)
        len = PATH_MAX - idx - 2;
    memcpy(&res[idx + 1], tnode->name, len);
    // !! TODO: Refactor all this
    return idx + len + 1;
}

ssize_t vfs_realpath_from_folder_tnode(vfs_folder_tnode_t* tnode, char* res)
{
    if (tnode == vfs_root) 
    {
        res[0] = '/';
        res[1] = 0;
        return 2;
    }
    size_t ret = vfs_realpath_from_folder_tnode_helper(tnode, res, 0);
    if (ret >= PATH_MAX) return -1;
    res[ret++] = 0;
    return ret;
}

ssize_t vfs_realpath_from_file_tnode(vfs_file_tnode_t* tnode, char* res)
{
    size_t ret = vfs_realpath_from_folder_tnode_helper(tnode->inode->parent, res, 0);
    size_t len = strlen(tnode->name);
    if (ret >= PATH_MAX - len - 2 || ret == -1)
        return -1;
    res[ret] = '/';
    memcpy(&res[ret + 1], tnode->name, len);
    res[ret + len + 1] = 0;
    return ret + len + 2;
}

void vfs_explore(vfs_folder_tnode_t* tnode)
{
    // LOG(TRACE, "exploring folder: %s", tnode->name);
    if (!tnode || !tnode->inode)
    {
        LOG(WARNING, "vfs_explore: node == NULL");
        return;
    }
    if (tnode->inode->flags & VFS_NODE_LOADING)
    {
        while (tnode->inode->flags & VFS_NODE_LOADING);
        return;
    }
    if (tnode->inode->flags & VFS_NODE_EXPLORED)
    {
        LOG(WARNING, "vfs_explore: Node already explored");
        return;
    }
    tnode->inode->flags |= VFS_NODE_LOADING;

    vfs_folder_tnode_t* mount_point = tnode;
    while (!(mount_point->inode->flags & VFS_NODE_MOUNTPOINT))
        mount_point = mount_point->inode->parent;

    switch (tnode->inode->drive.type)
    {
    case DT_INITRD:
        vfs_initrd_do_explore(tnode, mount_point);
        break;
    case DT_VIRTUAL:
        break;
    default:
        abort();
    }

    tnode->inode->flags |= VFS_NODE_EXPLORED;
    tnode->inode->flags &= ~VFS_NODE_LOADING;
}

vfs_file_tnode_t* vfs_add_special(const char* folder, const char* name, mode_t mode, ssize_t (*fun)(file_entry_t*, uint8_t*, size_t, uint8_t),
    uid_t uid, gid_t gid)
{
    if (!folder)
    {
        LOG(WARNING, "vfs_add_special: folder == NULL");
        return NULL;
    }
    if (!name)
    {
        LOG(WARNING, "vfs_add_special: name == NULL");
        return NULL;
    }

    vfs_folder_tnode_t* parent = vfs_get_folder_tnode(folder, NULL);
    if (!parent)
    {
        LOG(WARNING, "vfs_add_special: \"%s\" is not a valid folder", folder);
        return NULL;
    }

    acquire_mutex(&parent->inode->lock);

    vfs_file_tnode_t** current_tnode = &parent->inode->files;
    while (*current_tnode)
        current_tnode = &(*current_tnode)->next;

    *current_tnode = vfs_create_special_file_tnode(name, parent, mode, fun, uid, gid);

    release_mutex(&parent->inode->lock);

    return *current_tnode;
}

vfs_file_tnode_t* vfs_get_file_tnode(const char* path, vfs_folder_tnode_t* pwd)
{
    // LOG(DEBUG, "vfs_get_file_tnode(\"%s\", %p)", path, pwd);

    if (!path) return NULL;
    size_t path_len = strlen(path);
    if (path_len == 0) return NULL;

    if (!pwd) pwd = vfs_root;

    size_t i = 0;
    vfs_folder_tnode_t* current_folder_tnode = path[0] == '/' ? (i++, vfs_root) : pwd;
    vfs_folder_inode_t* current_folder = current_folder_tnode->inode;
    while (i < path_len)
    {
        // LOG(TRACE, "current_folder->name = %s", current_folder_tnode->name);
        if (!(current_folder->flags & VFS_NODE_EXPLORED))
            vfs_explore(current_folder_tnode);

        if (file_string_len(&path[i]) == strlen(&path[i]))
        // * Parse files
        {
            vfs_file_tnode_t* current_child = current_folder->files;
            while (current_child)
            {
                if (file_string_cmp(current_child->name, &path[i]))
                    return current_child;
                current_child = current_child->next;
            }
            return NULL;
        }

        if (file_string_cmp("", &path[i]) && path[i] != 0)
        {
            i++;
            continue;
        }
        if (file_string_cmp(".", &path[i]))
        {
            i += 2;
            continue;
        }
        if (file_string_cmp("..", &path[i]))
        {
            i += 3;
            current_folder_tnode = current_folder->parent;
            current_folder = current_folder_tnode->inode;
            continue;
        }
        vfs_folder_tnode_t* current_child = current_folder->folders;
        while (current_child)
        {
            // LOG(TRACE, "cname: %s", current_child->name);
            if (file_string_cmp(current_child->name, &path[i]))
            {
                current_folder_tnode = current_child;
                current_folder = current_child->inode;
                i += strlen(current_child->name);
                // LOG(TRACE, "found folder %s", current_child->name);
                break;
            }
            current_child = current_child->next;
        }
        if (!current_child)
            return NULL;
    }
    return NULL;
}

vfs_file_inode_t* vfs_get_file_inode(const char* path, vfs_folder_tnode_t* pwd)
{
    vfs_file_tnode_t* tnode = vfs_get_file_tnode(path, pwd);
    if (!tnode) return NULL;
    return tnode->inode;
}

vfs_folder_tnode_t* vfs_get_folder_tnode(const char* path, vfs_folder_tnode_t* pwd)
{
    // LOG(DEBUG, "vfs_get_folder_tnode(\"%s\", %p)", path, pwd);

    if (!path) return NULL;
    if (!pwd) pwd = vfs_root;

    size_t path_len = strlen(path);
    if (path_len == 0) return pwd;

    size_t i = 0;
    vfs_folder_tnode_t* current_folder = path[0] == '/' ? (i++, vfs_root) : pwd;
    while (i < path_len)
    {
        if (!(current_folder->inode->flags & VFS_NODE_EXPLORED))
            vfs_explore(current_folder);

        if (file_string_cmp("", &path[i]) && path[i] != 0)
        {
            i++;
            continue;
        }
        if (file_string_cmp(".", &path[i]))
        {
            i += 2;
            continue;
        }
        if (file_string_cmp("..", &path[i]))
        {
            i += 3;
            current_folder = current_folder->inode->parent;
            continue;
        }
        vfs_folder_tnode_t* current_child = current_folder->inode->folders;
        while (current_child)
        {
            if (file_string_cmp(current_child->name, &path[i]))
            {
                current_folder = current_child;
                i += strlen(current_child->name);
                break;
            }
            current_child = current_child->next;
        }
        if (!current_child)
            return NULL;
    }
    return current_folder;
}

int vfs_stat(const char* path, vfs_folder_tnode_t* pwd, struct stat* st)
{
    if (!path || !st)
        return EFAULT;

    vfs_folder_tnode_t* folder_tnode = vfs_get_folder_tnode(path, pwd);

    if (folder_tnode)
    {
        *st = folder_tnode->inode->st;
        return 0;
    }

    vfs_file_tnode_t* file_tnode = vfs_get_file_tnode(path, pwd);

    if (file_tnode)
    {
        *st = file_tnode->inode->st;
        return 0;
    }

    return ENOENT;
}

int vfs_fstat(int fd, vfs_folder_tnode_t* pwd, struct stat* st)
{
    if (!is_fd_valid(fd) || !st)
        return EFAULT;

    file_entry_t* entry = get_global_file_entry(fd);

    switch (entry->entry_type)
    {
    case ET_FILE: *st = entry->tnode.file->inode->st; return 0;
    case ET_FOLDER: *st = entry->tnode.folder->inode->st; return 0;
    default:
    }

    return ENOENT;
}

int vfs_access(const char* path, vfs_folder_tnode_t* pwd, mode_t mode)
{
    if (mode == 0) return 0;
    struct stat st;
    int ret = vfs_stat(path, pwd, &st);
    if (ret)
        return ret;
    // * Assume we're the owner of every file for now
    if ((mode & R_OK) && ((st.st_mode & S_IRUSR) == 0))
        return EACCES;
    if ((mode & W_OK) && ((st.st_mode & S_IWUSR) == 0))
        return EACCES;
    if ((mode & X_OK) && ((st.st_mode & S_IXUSR) == 0))
        return EACCES;
    return 0;
}

struct dirent* vfs_readdir(struct dirent* dirent, DIR* dirp)
{
    // vfs_folder_tnode_t* folder_tnode = vfs_get_folder_tnode(dirp->path, NULL);
    // if (!folder_tnode)
    // {
    //     errno = ENOENT;
    //     return NULL;
    // }

    // if (!(folder_tnode->inode->flags & VFS_NODE_EXPLORED))
    //     vfs_explore(folder_tnode);

    // if (strcmp(dirp->current_entry, "") == 0)
    // {
    //     strcpy(dirp->current_entry, ".");
    //     memcpy(dirent->d_name, dirp->current_entry, 2);
    //     dirent->d_ino = folder_tnode->inode->st.st_ino;
    //     errno = 0;
    //     return dirent;
    // }

    // if (strcmp(dirp->current_entry, ".") == 0)
    // {
    //     strcpy(dirp->current_entry, "..");
    //     memcpy(dirent->d_name, dirp->current_entry, 3);
    //     dirent->d_ino = folder_tnode->inode->parent->inode->st.st_ino;
    //     errno = 0;
    //     return dirent;
    // }

    // bool found_last_entry = strcmp(dirp->current_entry, "..") == 0;

    // vfs_folder_tnode_t* current_folder = folder_tnode->inode->folders;

    // while (current_folder)
    // {
    //     if (found_last_entry)
    //     {
    //         memcpy(dirp->current_entry, current_folder->name, PATH_MAX);
    //         memcpy(dirent->d_name, dirp->current_entry, PATH_MAX);
    //         dirent->d_ino = current_folder->inode->st.st_ino;
    //         errno = 0;
    //         return dirent;
    //     }
    //     if (strcmp(dirp->current_entry, current_folder->name) == 0)
    //         found_last_entry = true;
    //     current_folder = current_folder->next;
    // }

    // vfs_file_tnode_t* current_file = folder_tnode->inode->files;

    // while (current_file)
    // {
    //     if (found_last_entry)
    //     {
    //         memcpy(dirp->current_entry, current_file->name, PATH_MAX);
    //         memcpy(dirent->d_name, dirp->current_entry, PATH_MAX);
    //         dirent->d_ino = current_file->inode->st.st_ino;
    //         errno = 0;
    //         return dirent;
    //     }
    //     if (strcmp(dirp->current_entry, current_file->name) == 0)
    //         found_last_entry = true;
    //     current_file = current_file->next;
    // }

    errno = 0;
    return NULL;
}

int vfs_read(int fd, void* buffer, size_t num_bytes, ssize_t* bytes_read)
{
    if (!bytes_read) abort();
    if (!is_fd_valid(fd))
    {
        *bytes_read = -1;
        return EBADF;
    }
    if ((get_global_file_entry(fd)->flags & O_ACCMODE) == O_WRONLY) 
    {
        *bytes_read = -1;
        return EBADF;
    }

    if (get_global_file_entry(fd)->entry_type == ET_FILE)
    {
        mode_t mode = get_global_file_entry(fd)->tnode.file->inode->st.st_mode;
        *bytes_read = get_global_file_entry(fd)->tnode.file->inode->io_func(get_global_file_entry(fd), buffer, num_bytes, CHR_DIR_READ);
        if (*bytes_read > 0)
            get_global_file_entry(fd)->position += *bytes_read;
        return 0;
    }
    *bytes_read = 0;
    return 0;
}

int vfs_write(int fd, const char* buffer, uint64_t bytes_to_write, ssize_t* bytes_written)
{
    if (!bytes_written) abort();
    if (!is_fd_valid(fd))
    {
        *bytes_written = (uint64_t)-1;
        return EBADF;
    }
    if ((get_global_file_entry(fd)->flags & O_ACCMODE) == O_RDONLY)
    {
        *bytes_written = -1;
        return EBADF;
    }
    if (get_global_file_entry(fd)->entry_type == ET_FILE)
    {
        mode_t mode = get_global_file_entry(fd)->tnode.file->inode->st.st_mode;
        *bytes_written = get_global_file_entry(fd)->tnode.file->inode->io_func(get_global_file_entry(fd), (unsigned char*)buffer, bytes_to_write, CHR_DIR_WRITE);
        if (*bytes_written > 0)
            get_global_file_entry(fd)->position += *bytes_written;
        return 0;
    }
    *bytes_written = 0;
    return 0;
}

void vfs_log_tree(vfs_folder_tnode_t* tnode, int depth)
{
    if (!tnode) return;

    char access_str[11];

    LOG(DEBUG, "");
    for (int i = 0; i < depth; i++)
        CONTINUE_LOG(DEBUG, "    ");
    CONTINUE_LOG(DEBUG, "`%s` [inode %" PRId64 "] (access: %s, device id: %lu)%s", tnode->name, tnode->inode->st.st_ino, get_access_string(tnode->inode->st.st_mode, access_str), tnode->inode->st.st_dev, tnode->inode->flags & VFS_NODE_EXPLORED ? ":" : " (not explored)");
    vfs_folder_tnode_t* current_folder = tnode->inode->folders;
    while (current_folder)
    {
        vfs_log_tree(current_folder, depth + 1);
        current_folder = current_folder->next;
    }
    vfs_file_tnode_t* current_file = tnode->inode->files;
    while (current_file)
    {
        LOG(DEBUG, "");
        for (int i = 0; i < depth + 1; i++)
            CONTINUE_LOG(DEBUG, "    ");
        CONTINUE_LOG(DEBUG, "`%s` [inode %" PRId64 "] (access: %s", current_file->name, current_file->inode->st.st_ino, get_access_string(current_file->inode->st.st_mode, access_str));
        if (S_ISCHR(current_file->inode->st.st_mode) || S_ISBLK(current_file->inode->st.st_mode))
            CONTINUE_LOG(DEBUG, ", special file device id: %lu", tnode->inode->st.st_rdev);
        CONTINUE_LOG(DEBUG, ")");
        current_file = current_file->next;
    }
}

ssize_t task_chr_stdin(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction)
{
    switch(direction)
    {
    case CHR_DIR_READ:
        if (count == 0)
            return 0;
        lock_scheduler();
        if (no_buffered_characters(keyboard_input_buffer))
        {
            task_reading_stdin = current_task->pid;
            unlock_scheduler();
            switch_task();
        }
        else
            unlock_scheduler();
        lock_scheduler();
        uint64_t ret = minint(get_buffered_characters(keyboard_input_buffer), count);
        for (uint32_t i = 0; i < count; i++)
            // *** Only ASCII for now ***
            buf[i] = utf32_to_bios_oem(utf32_buffer_getchar(&keyboard_input_buffer));
        unlock_scheduler();
        return ret;
    case CHR_DIR_WRITE:
        return 0;
    }
    return 0;
}

ssize_t task_chr_stdout(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction)
{
    switch(direction)
    {
    case CHR_DIR_READ:
        return 0;
    case CHR_DIR_WRITE:
        for (uint32_t i = 0; i < count; i++)
            tty_outc(buf[i]);
        return count;
    }
    return 0;
}

ssize_t task_chr_stderr(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction)
{
    return task_chr_stdout(entry, buf, count, direction);
}

ssize_t task_chr_tty(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction)
{
    switch (direction)
    {
    case CHR_DIR_READ:
        return task_chr_stdin(entry, buf, count, direction);
    case CHR_DIR_WRITE:
        return task_chr_stdout(entry, buf, count, direction);
    default:
        return 0;
    }
}