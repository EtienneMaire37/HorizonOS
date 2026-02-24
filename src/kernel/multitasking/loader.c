#include "loader.h"
#include "vas.h"
#include "task.h"
#include "multitasking.h"
#include <elf.h>
#include "startup_data.h"
#include "../vfs/vfs.h"

thread_t* multitasking_add_task_from_function(const char* name, void (*func)())
{
    LOG(DEBUG, "Adding task \"%s\" from function", name);

    thread_t* task = task_create_empty();
    task_set_name(task, name);
    task->cr3 = task_create_empty_vas(PG_SUPERVISOR);
    task->ring = 0;

    task->rsp = TASK_STACK_TOP_ADDRESS - 8;
    task_setup_stack(task, (uint64_t)func);

    multitasking_add_task(task);
    task_count++;

    LOG(DEBUG, "Done");

    return task;
}

thread_t* multitasking_add_task_from_initrd(const char* name, const char* path, uint8_t ring, bool system, const startup_data_struct_t* data, vfs_folder_tnode_t* cwd)
{
    LOG(INFO, "Loading ELF file \"/initrd/%s\"", path);

    if (ring != 0 && ring != 3)
    {
        LOG(ERROR, "Invalid privilege level");
        return NULL;
    }

    initrd_file_t* file;
    if (!(file = initrd_find_file(path))) 
    {
        LOG(ERROR, "Coudln't load file");
        return NULL;
    }

    Elf64_Ehdr* header = (Elf64_Ehdr*)file->data;
    if (memcmp("\x7f""ELF", header->e_ident, 4) != 0) 
    {
        LOG(ERROR, "Invalid ELF signature");
        return NULL;
    }

    if (header->e_ident[4] != ELFCLASS64 || 
        header->e_ident[5] != ELFDATA2LSB || 
        header->e_machine != EM_X86_64)
    {
        LOG(ERROR, "Non x86_64 ELF file");
        return NULL;
    }

    if (header->e_type != ET_EXEC) 
    {
        LOG(ERROR, "Non executable ELF file (%#x)", header->e_type);
        return NULL;
    }

    thread_t* task = task_create_empty();
    task_set_name(task, name);
    task->cr3 = task_create_empty_vas(ring == 0 ? PG_SUPERVISOR : PG_USER);

    task->rsp = TASK_STACK_TOP_ADDRESS - 8;

    task->ring = ring;

    task->system_task = system;
    task->cwd = cwd;

    LOG(DEBUG, "Entry point : %#" PRIx64, header->e_entry);

    const Elf64_Half n_ph = header->e_phnum;

    for (Elf64_Half i = 0; i < n_ph; i++)
    {
        Elf64_Phdr* ph = (Elf64_Phdr*)&file->data[header->e_phoff + i * header->e_phentsize];
        if (ph->p_type == PT_NULL) continue;

        LOG(DEBUG, "Program header %u : ", i);
        LOG(DEBUG, "├── Type : %#x", ph->p_type);
        LOG(DEBUG, "├── Virtual address : %#" PRIx64, ph->p_vaddr);
        LOG(DEBUG, "├── File offset : %#" PRIx64, ph->p_offset);
        LOG(DEBUG, "├── Memory size : %" PRIu64 " bytes", ph->p_memsz);
        LOG(DEBUG, "└── File size : %" PRIu64 " bytes", ph->p_filesz);

        if (ph->p_type != PT_LOAD) 
            continue;

        virtual_address_t start_address = ph->p_vaddr & ~0xfff;
        virtual_address_t end_address = ph->p_vaddr + ph->p_memsz;
        uint64_t num_pages = (end_address - start_address + 0xfff) >> 12;

        // LOG(DEBUG, "%#" PRIx64 " : %" PRIu64 " pages", start_address, num_pages);

        allocate_range((uint64_t*)(task->cr3 + PHYS_MAP_BASE), 
                    start_address, num_pages, 
                    ring == 0 ? PG_SUPERVISOR : PG_USER,
                    ph->p_flags & PF_W ? PG_READ_WRITE : PG_READ_ONLY, 
                    CACHE_WB);

        uint64_t file_offset = ph->p_offset;
        uint64_t remaining_file = ph->p_filesz;
        uint64_t remaining_zero = ph->p_memsz;

        for (uint64_t page = 0; page < num_pages; page++)
        {
            uint64_t page_vaddr = start_address + page * 0x1000;
            uint8_t* phys = (uint8_t*)virtual_to_physical((uint64_t*)(task->cr3 + PHYS_MAP_BASE), page_vaddr);

            if (!phys)
                continue;

            size_t offset_in_page = (page == 0) ? (ph->p_vaddr & 0xfff) : 0;
            size_t bytes_in_page = 0x1000 - offset_in_page;

            size_t to_copy = (remaining_file < bytes_in_page) ? remaining_file : bytes_in_page;

            memcpy(phys + offset_in_page, &file->data[file_offset], to_copy);

            size_t to_zero = bytes_in_page - to_copy;
            memset(phys + offset_in_page + to_copy, 0, to_zero);

            remaining_file -= to_copy;
            remaining_zero -= bytes_in_page;
            file_offset += to_copy;
        }
    }

    const Elf64_Half n_sh = header->e_shnum;
    const Elf64_Shdr* shstrtab = (Elf64_Shdr*)&file->data[header->e_shoff + header->e_shstrndx * header->e_shentsize];

    for (Elf64_Half i = 0; i < n_sh; i++)
    {
        Elf64_Shdr* sh = (Elf64_Shdr*)&file->data[header->e_shoff + i * header->e_shentsize];
        if (sh->sh_type == SHT_NULL) continue;

        const char* name = (const char*)&file->data[shstrtab->sh_offset + sh->sh_name];

        LOG(DEBUG, "Section header %u : ", i);
        LOG(DEBUG, "├── Name : \"%s\"", name);
        LOG(DEBUG, "├── Type : %#x", sh->sh_type);
        LOG(DEBUG, "├── Address : %#" PRIx64, sh->sh_addr);
        LOG(DEBUG, "└── Size : %" PRIu64 " bytes", sh->sh_size);
    }



    startup_data_struct_t data_cpy = *data;

    for (int i = 0; i <= data->envc; i++)
        task_stack_push(task, (uint64_t)data->environ[data->envc - i]);

    data_cpy.environ = (char**)task->rsp;

    for (int i = 0; i <= data->envc; i++)
    {
        if (data->environ[i])
        {
            task_stack_push_string(task, data->environ[i]);
            task_write_at_address_8b(task, (uint64_t)&data_cpy.environ[i], task->rsp);
        }
        else
            task_write_at_address_8b(task, (uint64_t)&data_cpy.environ[i], 0);
    }

    for (int i = 0; i <= data->argc; i++)
        task_stack_push(task, (uint64_t)data->cmd[data->argc - i]);

    data_cpy.cmd = (char**)task->rsp;

    for (int i = 0; i <= data->argc; i++)
    {
        if (data->cmd[i])
        {
            task_stack_push_string(task, data->cmd[i]);
            task_write_at_address_8b(task, (uint64_t)&data_cpy.cmd[i], task->rsp);
        }
        else
            task_write_at_address_8b(task, (uint64_t)&data_cpy.cmd[i], 0);
    }

    // const int auxc = 0;
    task_stack_push_auxv(task, (Elf64_auxv_t){.a_type = AT_NULL, .a_un.a_val = 0});
    // for (int i = 0; i < auxc; i++)
    //     task_stack_push_auxv(task, (Elf64_auxv_t){.a_type = , .a_un.a_val = });
        
    for (int i = 0; i < data->envc + 1; i++)
        task_stack_push(task, task_read_at_aligned_address_8b(task, (uint64_t)&data_cpy.environ[data->envc - i]));
    for (int i = 0; i < data->argc + 1; i++)
        task_stack_push(task, task_read_at_aligned_address_8b(task, (uint64_t)&data_cpy.cmd[data->argc - i]));

    task_stack_push(task, (uint64_t)data_cpy.argc);

    task_setup_stack(task, header->e_entry);

    multitasking_add_task(task);
    task_count++;

    LOG(DEBUG, "Done");

    return task;
}

thread_t* multitasking_add_task_from_vfs(const char* name, const char* path, uint8_t ring, bool system, const startup_data_struct_t* data, vfs_folder_tnode_t* cwd)
{
    if (!name) return false;
    if (!data) 
    {
        LOG(DEBUG, "multitasking_add_task_from_vfs: data is NULL");
        abort();
    }

    if (strlen(path) == 0) return NULL;

    lock_scheduler();

    vfs_file_tnode_t* tnode = vfs_get_file_tnode(path, NULL);

    if (!tnode)
    {
        LOG(ERROR, "Couldn't find program \"%s\"", path);
        unlock_scheduler();
        return NULL;
    }

    char* simplified_path = malloc(PATH_MAX);
    if (!simplified_path) 
    {
        LOG(DEBUG, "multitasking_add_task_from_vfs: Out of memory");
        abort();
    }
    
    vfs_realpath_from_file_tnode(tnode, simplified_path);

    LOG(DEBUG, "Loading file \"%s\"", simplified_path);
    vfs_folder_tnode_t* mount_point = tnode->inode->parent;
    while (!(mount_point->inode->flags & VFS_NODE_MOUNTPOINT))
        mount_point = mount_point->inode->parent;
    // !! Horrible way to do things
    // TODO: Add more general program loading with the vfs read/write syscalls directly
    if (mount_point->inode->drive.type != DT_INITRD)
    {
        LOG(ERROR, "Invalid path");
        free(simplified_path);
        unlock_scheduler();
        return NULL;
    }
    char* prefix = malloc(PATH_MAX);
    if (!prefix) 
    {
        LOG(DEBUG, "multitasking_add_task_from_vfs: Out of memory");
        abort();
    }
    vfs_realpath_from_folder_tnode(mount_point, prefix);
    size_t prefix_length = strlen(prefix);

    thread_t* ret = multitasking_add_task_from_initrd(simplified_path, strcmp(simplified_path, prefix) == 0 ? "" : &simplified_path[(mount_point == vfs_root ? 0 : 1) + prefix_length], ring, system, data, cwd);
    
    free(simplified_path);
    free(prefix);
    unlock_scheduler();
    return ret;
}