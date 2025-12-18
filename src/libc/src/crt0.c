extern void* _end;
extern void call_main_exit(int argc, char** argv);

extern uint64_t kernel_data;

void _main()
{
    __builtin_memset(atexit_stack, 0, 32);
    atexit_stack_length = 0;

    errno = 0;
    fd_creation_mask = S_IWGRP | S_IWOTH;

    heap_size = 0;
    break_address = (uint64_t)&_end;
    break_address = ((break_address + 4095) / 4096) * 4096;
    heap_address = break_address;
    alloc_end = break_address;

    malloc_bitmap_init();

    startup_data_struct_t* data = (startup_data_struct_t*)kernel_data;

    environ = data->environ;
    int environ_num = 0;
    while (environ[environ_num])
        environ_num++;

    // for (int i = 0; i < environ_num; i++)
    //     dprintf(STDOUT_FILENO, "\"%s\"\n", environ[i]);

    stdin = FILE_create();
    if (stdin == NULL) abort();
    stdin->fd = STDIN_FILENO;
    stdin->flags = FILE_FLAGS_READ;

    stdout = FILE_create();
    if (stdout == NULL) abort();
    stdout->fd = STDOUT_FILENO;
    stdout->flags = FILE_FLAGS_WRITE | (isatty(stdout->fd) ? FILE_FLAGS_LBF : FILE_FLAGS_FBF);

    stderr = FILE_create();
    if (stderr == NULL) abort();
    stderr->fd = STDERR_FILENO;
    stderr->flags = FILE_FLAGS_WRITE | (isatty(stderr->fd) ? FILE_FLAGS_NBF : FILE_FLAGS_FBF);

    char** _environ = malloc((environ_num + 1) * sizeof(char*));
    if (_environ)
    {
        for (int i = 0; i < environ_num; i++)
        {
            _environ[i] = malloc((__builtin_strlen(environ[i]) + 1));
            if (!_environ[i])
                abort();
            __builtin_strcpy(_environ[i], environ[i]);
        }
        num_environ = environ_num;
        environ = _environ;
    }
    else
    {
        perror("libc");
        abort();
    }

    create_b64_decoding_table();

    char** argv = data->cmd_line;
    int argc = 0;
    if (argv)
    {
        while (argv[argc])
            argc++;
    }

    // printf("argc : %d\n", argc);

    call_main_exit(argc, argv);
    while(true);
}