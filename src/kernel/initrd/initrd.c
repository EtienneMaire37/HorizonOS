#include "initrd.h"

initrd_file_t initrd_files[MAX_INITRD_FILES];
uint8_t initrd_files_count = 0;

void initrd_parse(uint64_t initrd_start, uint64_t initrd_size)
{
    LOG(INFO, "Parsing initrd");

    LOG(INFO, "Initrd size : %" PRIu64 " bytes", initrd_size);

    uint64_t initrd_offset = 0;

    while (initrd_offset < initrd_size)
    {
        struct ustar_header* header = (struct ustar_header*)(initrd_start + initrd_offset);

        if (header->name[0] == '\0')
            break;

        if (!USTAR_IS_VALID_HEADER(*header))
        {
            LOG(WARNING, "Invalid USTAR header at offset %" PRIu64, initrd_offset);
            break;
        }

        uint64_t file_size = ustar_get_number(header->size, 12);

        initrd_files[initrd_files_count].name = &header->name[0];
        size_t len = strlen(initrd_files[initrd_files_count].name);
        if (len >= 1)
        {
            if (initrd_files[initrd_files_count].name[len - 1] == '.')
                goto do_loop;
            if (initrd_files[initrd_files_count].name[len - 1] == '/')
                initrd_files[initrd_files_count].name[len - 1] = 0;
        }
        else
            goto do_loop;
            
        initrd_files[initrd_files_count].size = file_size;
        initrd_files[initrd_files_count].data = (uint8_t*)(header + 1); // 512 bytes after the header
        if (header->type == 0) header->type = '0';

        initrd_files[initrd_files_count].type = header->type;
        initrd_files[initrd_files_count].link = (char*)&header->linked_file[0];

        uint64_t mode = ustar_get_number((char*)header->mode, 8);

        struct stat st;
        st.st_mode = (header->type == USTAR_TYPE_DIRECTORY ? S_IFDIR : S_IFREG) | 
        ((mode & TUREAD) ? S_IRUSR : 0) | ((mode & TUEXEC) ? S_IXUSR : 0) | // * | ((mode & TUWRITE) ? S_IWUSR : 0)
        ((mode & TGREAD) ? S_IRGRP : 0) | ((mode & TGEXEC) ? S_IXGRP : 0) | // * | ((mode & TGWRITE) ? S_IWGRP : 0)
        ((mode & TOREAD) ? S_IROTH : 0) | ((mode & TOEXEC) ? S_IXOTH : 0) | // * | ((mode & TOWRITE) ? S_IWOTH : 0)
        ((mode & TSUID) ? S_ISUID : 0)  | ((mode & TSGID) ? S_ISGID : 0) | ((mode & TSVTX) ? S_ISVTX : 0);

        st.st_blksize = 4096;
        st.st_atime = st.st_mtime = st.st_ctime = 0;

        st.st_size = file_size;
        
        // LOG(DEBUG, "header->last_modification: %.12s", header->last_modification);

        initrd_files[initrd_files_count].st = st;

        initrd_files_count++;
        
    do_loop:
        initrd_offset += (file_size + USTAR_BLOCK_SIZE - 1) / USTAR_BLOCK_SIZE * USTAR_BLOCK_SIZE + USTAR_BLOCK_SIZE;
    }

    for (uint8_t i = 0; i < initrd_files_count; i++)
    {
        // if (initrd_files[i].size != 0 && 
        //     initrd_files[i].name != NULL)
        {
            char* tree_inter = initrd_files_count - i > 1 ? "├" : "└";
            tar_file_type this_type = initrd_files[i].type;
            switch (this_type)
            {
            case USTAR_TYPE_FILE_1:
            case USTAR_TYPE_FILE_2:
                LOG(INFO, "%s── File : \"%s\" ; Size : %" PRIu64 " bytes", tree_inter, initrd_files[i].name, initrd_files[i].size);
                break;
            case USTAR_TYPE_HARD_LINK:
                LOG(INFO, "%s── Hard link : \"%s\" pointing to \"%s\"", tree_inter, initrd_files[i].name, initrd_files[i].link);
                break;
            case USTAR_TYPE_SYMBOLIC_LINK:
                LOG(INFO, "%s── Symbolic link : \"%s\" pointing to \"%s\"", tree_inter, initrd_files[i].name, initrd_files[i].link);
                break;
            case USTAR_TYPE_CHARACTER_DEVICE:
                LOG(INFO, "%s── Character device : \"%s\"", tree_inter, initrd_files[i].name);
                break;
            case USTAR_TYPE_BLOCK_DEVICE:
                LOG(INFO, "%s── Block device : \"%s\"", tree_inter, initrd_files[i].name);
                break;
            case USTAR_TYPE_DIRECTORY:
                LOG(INFO, "%s── Directory : \"%s\"", tree_inter, initrd_files[i].name);
                break;
            case USTAR_TYPE_NAMED_PIPE:
                LOG(INFO, "%s── FIFO : \"%s\"", tree_inter, initrd_files[i].name);
                break;
            default:
                ;
            }
        }
    }

    LOG(INFO, "Done parsing initrd (%u files)", initrd_files_count);
}

initrd_file_t* initrd_find_file(const char* name)
{
    LOG(DEBUG, "Opening file \"%s\" from initrd", name);

    for (uint8_t i = 0; i < initrd_files_count; i++)
    {
        if (strcmp(initrd_files[i].name, name) == 0 && initrd_files[i].type == USTAR_TYPE_FILE_1)
        {
            LOG(DEBUG, "Found at index %u", i);
            return &initrd_files[i];
        }
    }

    LOG(WARNING, "Error opening file \"%s\"", name);

    return NULL;
}

initrd_file_t* initrd_find_file_entry(const char* name)
{
    LOG(DEBUG, "Opening file entry \"%s\" from initrd", name);

    for (uint8_t i = 0; i < initrd_files_count; i++)
    {
        if (strcmp(initrd_files[i].name, name) == 0)
        {
            LOG(DEBUG, "Found at index %u", i);
            return &initrd_files[i];
        }
    }

    LOG(WARNING, "Error opening file entry \"%s\"", name);

    return NULL;
}