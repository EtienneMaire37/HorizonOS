uintptr_t break_address, heap_address, alloc_end;
uintptr_t heap_size;

void* sbrk(intptr_t incr)
{
    void* old_break = (void*)break_address;
    if (brk((void*)((uintptr_t)(break_address + incr))) != 0)
        return (void*)-1;
    return old_break;
}

static const char* host_name = "horizonos-pc";

int gethostname(char* name, size_t namelen)
{
    if (!name || namelen <= 0) return -1;
    __builtin_memcpy(name, host_name, namelen - 1);
    name[namelen - 1] = 0;

    return 0;
}

int execv(const char* path, char* const argv[])
{
    return execve(path, argv, environ);
}

int execvpe(const char* file, char* const argv[], char* const envp[])
{
    execve(file, argv, envp);

    if (!(errno == ENOENT || errno == ENOTDIR || errno == EACCES))
        return -1;

    const size_t file_len = strlen(file);
    for (size_t i = 0; i < file_len; i++)
    {
        if (file[i] == '/') 
        {
            errno = ENOENT;
            return -1;
        }
    }

    const char* PATH = getenv("PATH");
    if (!PATH) 
    {
        errno = ENOENT;
        return -1;
    }
    int bytes = strlen(PATH) + 1;
    char* path_data = malloc(bytes);
    strncpy(path_data, PATH, bytes);
    for (int i = 0; i < bytes; i++)
    {
        if (path_data[i] == ':')
            path_data[i] = 0;
    }
    char* path = path_data;
    do
    {
        int path_len = strlen(path);
        char* combined = malloc(path_len + file_len + 2);
        strcpy(combined, path);
        combined[path_len] = '/';
        strcpy(((void*)combined + path_len + 1), file);
        execve(combined, argv, envp);
        free(combined);
    }
    while ((path = find_next_contiguous_string(path, &bytes)));
    free(path_data);
    errno = ENOENT;
    return -1;
}

int execvp(const char* file, char* const argv[])
{
    return execvpe(file, argv, environ);
}

pid_t getpgrp()
{
    return getpgid(0);
}