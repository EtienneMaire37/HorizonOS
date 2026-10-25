#include "initrd.h"
#include "../util/assert.h"
#include "../debug/out.h"
#include "../files/ustar.h"
#include "../vfs/fs/initrd.h"

#include <string.h>
#include <stdlib.h>

initrd_file_t initrd_files[MAX_INITRD_FILES];
uint16_t initrd_files_count = 0;

void initrd_parse(uint64_t initrd_start, uint64_t initrd_size)
{
    LOG(INFO, "Parsing initrd");

    LOG(INFO, "Initrd address : %#" PRIx64" | Initrd size : %" PRIu64 " bytes", initrd_start, initrd_size);

    ASSERT(sizeof(ustar_header_t) == 512);
    ustar_header_t* header = (ustar_header_t*)initrd_start;
    while (((intptr_t)header - (intptr_t)initrd_start) <= (intptr_t)initrd_size - 512)
    {
        if (!USTAR_IS_VALID_HEADER(*header))
            break;
        size_t filesize = ustar_get_number(header->size, sizeof(header->size));
        size_t blocks = (filesize + 511) / 512;
        // Skip "./"
        if (!(header->name[0] != 0 && header->name[1] != 0))
        {
            header += blocks + 1;
            continue;
        }
        initrd_files[initrd_files_count].name = header->name + 1;
        size_t len = strlen(initrd_files[initrd_files_count].name);
        if (initrd_files[initrd_files_count].name[len - 1] == '/')
            initrd_files[initrd_files_count].name[len - 1] = 0;
        initrd_files[initrd_files_count].data = (void*)&header[1];
        initrd_files[initrd_files_count].link = header->linked_file;
        initrd_files[initrd_files_count].st.st_nlink = 1;
        initrd_files[initrd_files_count].st.st_blksize = 512;
        initrd_files[initrd_files_count].st.st_blocks = blocks;
        initrd_files[initrd_files_count].st.st_uid = header->owner_id;
        initrd_files[initrd_files_count].st.st_gid = header->group_id;
        initrd_files[initrd_files_count].st.st_ino = initrd_generate_ino();
        initrd_files[initrd_files_count].st.st_size = filesize;
        mode_t filetype;
        switch (header->type)
        {
        case USTAR_TYPE_FILE_1:
        case USTAR_TYPE_FILE_2:
            filetype = S_IFREG;
            break;
        case USTAR_TYPE_DIRECTORY:
            filetype = S_IFDIR;
            break;
        case USTAR_TYPE_SYMBOLIC_LINK:
            filetype = S_IFLNK;
            break;
        default:
            LOG(ERROR, "Unsupported ustar file type in initrd (%d)", header->type);
            abort();
        }
        uint64_t mode = ustar_get_number((char*)header->mode, 8);
        initrd_files[initrd_files_count].st.st_mode = filetype |
            ((mode & TUREAD) ? S_IRUSR : 0) | ((mode & TUEXEC) ? S_IXUSR : 0) | // * | ((mode & TUWRITE) ? S_IWUSR : 0)
            ((mode & TGREAD) ? S_IRGRP : 0) | ((mode & TGEXEC) ? S_IXGRP : 0) | // * | ((mode & TGWRITE) ? S_IWGRP : 0)
            ((mode & TOREAD) ? S_IROTH : 0) | ((mode & TOEXEC) ? S_IXOTH : 0) | // * | ((mode & TOWRITE) ? S_IWOTH : 0)
            ((mode & TSUID)  ? S_ISUID : 0) | ((mode & TSGID)  ? S_ISGID : 0)
                                            | ((mode & TSVTX)  ? S_ISVTX : 0);
        
        initrd_files[initrd_files_count].st.st_atim = (struct timespec){ 0, 0 };
        initrd_files[initrd_files_count].st.st_ctim = (struct timespec){ 0, 0 };
        initrd_files[initrd_files_count].st.st_mtim = (struct timespec){ 0, 0 };
        initrd_files_count++;
        header += blocks + 1;
    }

    for (uint16_t i = 0; i < initrd_files_count; i++)
    {
        char* tree_inter = initrd_files_count - i > 1 ? "├" : "└";
        if (S_ISREG(initrd_files[i].st.st_mode))
            LOG(INFO, "%s── File : \"%s\" ; Size : %" PRIu64 " bytes", tree_inter, initrd_files[i].name, initrd_files[i].st.st_size);
        if (S_ISLNK(initrd_files[i].st.st_mode))
            LOG(INFO, "%s── Symbolic link : \"%s\" -> \"%s\"", tree_inter, initrd_files[i].name, initrd_files[i].link);
        if (S_ISDIR(initrd_files[i].st.st_mode))
            LOG(INFO, "%s── Directory : \"%s\"", tree_inter, initrd_files[i].name);
    }


    LOG(INFO, "Done parsing initrd (%u files)", initrd_files_count);
}

initrd_file_t* initrd_find_file(const char* name)
{
    initrd_file_t* f = initrd_find_file_entry(name);
    if (!f) return NULL;
    if (S_ISREG(f->st.st_mode)) return f;
    return NULL;
}

initrd_file_t* initrd_find_file_entry(const char* name)
{
    LOG(DEBUG, "Opening file entry \"%s\" from initrd", name);

    for (uint16_t i = 0; i < initrd_files_count; i++)
    {
        if (strcmp(initrd_files[i].name, name) == 0)
            return &initrd_files[i];
    }

    LOG(WARNING, "Error opening file entry \"%s\"", name);

    return NULL;
}
