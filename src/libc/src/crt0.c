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

    uint64_t* data = (uint64_t*)kernel_data;

    int argc = data[0];
    char** argv = (char**)data[1];

    char** __environ = (char**)data[2];

    int environ_num = 0;
    if (__environ)
        while (__environ[environ_num])
            environ_num++;

    // while (true);

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
            _environ[i] = malloc((__builtin_strlen(__environ[i]) + 1));
            if (!_environ[i])
                abort();
            __builtin_strcpy(_environ[i], __environ[i]);
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

    // dprintf(STDOUT_FILENO, "environ:\n");
    // for (int i = 0; i < environ_num; i++)
    //     dprintf(STDOUT_FILENO, "\"%s\"\n", __environ[i]);
    // dprintf(STDOUT_FILENO, "environ:\n");
    // for (int i = 0; i < environ_num; i++)
    //     dprintf(STDOUT_FILENO, "\"%s\"\n", environ[i]);
    // dprintf(STDOUT_FILENO, "argv:\n");
    // for (int i = 0; i < argc; i++)
    //     dprintf(STDOUT_FILENO, "\"%s\"\n", argv[i]);
    // dprintf(STDOUT_FILENO, "---\n");

    call_main_exit(argc, argv);
    while(true);
}