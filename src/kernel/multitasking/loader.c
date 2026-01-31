#include "loader.h"
#include "vas.h"
#include "task.h"
#include "../files/elf.h"
#include "startup_data.h"
#include "../vfs/vfs.h"

void multitasking_add_task_from_function(const char* name, void (*func)())
{
    LOG(DEBUG, "Adding task \"%s\" from function", name);

    thread_t task = task_create_empty();
    task_set_name(&task, name);
    task.cr3 = task_create_empty_vas(PG_SUPERVISOR);

    task.rsp = TASK_STACK_TOP_ADDRESS - 8;
    task_setup_stack(&task, (uint64_t)func, KERNEL_CODE_SEGMENT, KERNEL_DATA_SEGMENT);

    tasks[task_count++] = task;

    LOG(DEBUG, "Done");
}

bool multitasking_add_task_from_initrd(const char* name, const char* path, uint8_t ring, bool system, const startup_data_struct_t* data, vfs_folder_tnode_t* cwd)
{
    LOG(INFO, "Loading ELF file \"/initrd/%s\"", path);

    if (ring != 0 && ring != 3)
    {
        LOG(ERROR, "Invalid privilege level");
        return false;
    }

    initrd_file_t* file;
    if (!(file = initrd_find_file(path))) 
    {
        LOG(ERROR, "Coudln't load file");
        return false;
    }

    elf64_header_t* header = (elf64_header_t*)file->data;
    if (memcmp("\x7f""ELF", header->magic, 4) != 0) 
    {
        LOG(ERROR, "Invalid ELF signature");
        return false;
    }

    if (header->architecture != ELF_CLASS_64 || header->byte_order != ELF_DATA_LITTLE_ENDIAN || header->machine != ELF_INSTRUCTION_SET_x86_64)
    {
        LOG(ERROR, "Non x86_64 ELF file");
        return false;
    }

    if (header->type != ELF_TYPE_EXECUTABLE) 
    {
        LOG(ERROR, "Non executable ELF file");
        return false;
    }

    thread_t task = task_create_empty();
    task_set_name(&task, name);
    task.cr3 = task_create_empty_vas(ring == 0 ? PG_SUPERVISOR : PG_USER);

    task.rsp = TASK_STACK_TOP_ADDRESS - 8;

    startup_data_struct_t data_cpy = *data;

    for (int i = 0; i <= data->envc; i++)
        task_stack_push(&task, (uint64_t)data->environ[data->envc - i]);

    data_cpy.environ = (char**)task.rsp;

    for (int i = 0; i <= data->envc; i++)
    {
        if (data->environ[i])
        {
            task_stack_push_string(&task, data->environ[i]);
            task_write_at_address_8b(&task, (uint64_t)&data_cpy.environ[i], task.rsp);
        }
        else
            task_write_at_address_8b(&task, (uint64_t)&data_cpy.environ[i], 0);
    }

    for (int i = 0; i <= data->argc; i++)
        task_stack_push(&task, (uint64_t)data->cmd[data->argc - i]);

    data_cpy.cmd = (char**)task.rsp;

    for (int i = 0; i <= data->argc; i++)
    {
        if (data->cmd[i])
        {
            task_stack_push_string(&task, data->cmd[i]);
            task_write_at_address_8b(&task, (uint64_t)&data_cpy.cmd[i], task.rsp);
        }
        else
            task_write_at_address_8b(&task, (uint64_t)&data_cpy.cmd[i], 0);
    }

    // task_stack_push_data(&task, &data_cpy, sizeof(data_cpy));
    
    for (int i = 0; i < data->envc + 1; i++)
        task_stack_push(&task, task_read_at_aligned_address_8b(&task, (uint64_t)&data_cpy.environ[data->envc - i]));
    for (int i = 0; i < data->argc + 1; i++)
        task_stack_push(&task, task_read_at_aligned_address_8b(&task, (uint64_t)&data_cpy.cmd[data->argc - i]));

    // task_stack_push(&task, (uint64_t)data_cpy.environ);
    // task_stack_push(&task, (uint64_t)data_cpy.cmd);
    task_stack_push(&task, (uint64_t)data_cpy.argc);

    // for (int i = 0; i < 16; i++)
    //     LOG(DEBUG, "%d: %#llx", i, task_read_at_aligned_address_8b(&task, task.rsp + 8 * i));

    // task_stack_push(&task, task.rsp);

    task_setup_stack(&task, header->entry, 
        ring == 0 ? KERNEL_CODE_SEGMENT : USER_CODE_SEGMENT, 
        ring == 0 ? KERNEL_DATA_SEGMENT : USER_DATA_SEGMENT);

    task.ring = ring;

    task.system_task = system;
    task.cwd = cwd;

    LOG(DEBUG, "Entry point : %#llx", header->entry);

    const elf64_half_t n_ph = header->phnum;

    for (elf64_half_t i = 0; i < n_ph; i++)
    {
        elf64_program_header_t* ph = (elf64_program_header_t*)&file->data[header->phoff + i * header->phentsize];
        if (ph->type == ELF_PROGRAM_TYPE_NULL) continue;

        LOG(DEBUG, "Program header %u : ", i);
        LOG(DEBUG, "├── Type : \"%s\"", elf64_get_phtype_string(ph->type));
        LOG(DEBUG, "├── Virtual address : %#llx", ph->p_vaddr);
        LOG(DEBUG, "├── File offset : %#llx", ph->p_offset);
        LOG(DEBUG, "├── Memory size : %u bytes", ph->p_memsz);
        LOG(DEBUG, "└── File size : %u bytes", ph->p_filesz);

        if (ph->type != ELF_PROGRAM_TYPE_LOAD) 
        {
            // LOG(ERROR, "Unsupported ELF program header type");
            // task_destroy(&task);
            // return false;
            continue;
        }

        virtual_address_t start_address = ph->p_vaddr & ~0xfff;
        virtual_address_t end_address = ph->p_vaddr + ph->p_memsz;
        uint64_t num_pages = (end_address - start_address + 0xfff) >> 12;

        // LOG(DEBUG, "%#llx : %llu pages", start_address, num_pages);

        allocate_range((uint64_t*)(task.cr3 + PHYS_MAP_BASE), 
                    start_address, num_pages, 
                    ring == 0 ? PG_SUPERVISOR : PG_USER,
                    ph->flags & ELF_FLAG_WRITABLE ? PG_READ_WRITE : PG_READ_ONLY, 
                    CACHE_WB);

        uint64_t file_offset = ph->p_offset;
        uint64_t remaining_file = ph->p_filesz;
        uint64_t remaining_zero = ph->p_memsz;

        for (uint64_t page = 0; page < num_pages; page++)
        {
            uint64_t page_vaddr = start_address + page * 0x1000;
            uint8_t* phys = (uint8_t*)virtual_to_physical((uint64_t*)(task.cr3 + PHYS_MAP_BASE), page_vaddr);

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

    const elf64_half_t n_sh = header->shnum;
    const elf64_section_header_t* shstrtab = (elf64_section_header_t*)&file->data[header->shoff + header->shstrndx * header->shentsize];

    for (elf64_half_t i = 0; i < n_sh; i++)
    {
        elf64_section_header_t* sh = (elf64_section_header_t*)&file->data[header->shoff + i * header->shentsize];
        if (sh->type == ELF_SECTION_TYPE_NULL) continue;

        const char* name = (const char*)&file->data[shstrtab->offset + sh->name];

        LOG(DEBUG, "Section header %u : ", i);
        LOG(DEBUG, "├── Name : \"%s\"", name);
        LOG(DEBUG, "├── Type : \"%s\"", elf64_get_shtype_string(sh->type));
        LOG(DEBUG, "├── Address : %#llx", sh->addr);
        LOG(DEBUG, "└── Size : %llu bytes", sh->size);
    }

    tasks[task_count++] = task;

    LOG(DEBUG, "Done");

    return true;
}

bool multitasking_add_task_from_vfs(const char* name, const char* path, uint8_t ring, bool system, const startup_data_struct_t* data, vfs_folder_tnode_t* cwd)
{
    if (!name) return false;
    if (!data) abort();

    if (strlen(path) == 0) return false;

    vfs_file_tnode_t* tnode = vfs_get_file_tnode(path, NULL);

    if (!tnode)
    {
        LOG(ERROR, "Couldn't find program \"%s\"", path);
        return false;
    }

    char* simplified_path = malloc(PATH_MAX);
    if (!simplified_path) abort();
    
    vfs_realpath_from_file_tnode(tnode, simplified_path);

    LOG(DEBUG, "Loading file \"%s\"", simplified_path);
    if (file_string_cmp(simplified_path + 1, "initrd"))
    {
        bool ret = multitasking_add_task_from_initrd(simplified_path, &simplified_path[strlen("/initrd/")], ring, system, data, cwd);
        free(simplified_path);
        return ret;
    }
    
    LOG(ERROR, "Invalid path");
    free(simplified_path);
    return false;
}