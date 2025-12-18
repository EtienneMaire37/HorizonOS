#include <limits.h>

uint32_t rand_next = 1;

int rand()
{
    rand_next = rand_next * 1103515245 + 12345;
    return (int)((rand_next / 65536) % (RAND_MAX + 1));
}

void srand(unsigned int seed)
{
    rand_next = seed;
}

static char* encoding_table = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static char decoding_table[256];

void create_b64_decoding_table()
{
    // memset(decoding_table, 0xff, 256);
    for (int i = 0; i < 256; i++)
        decoding_table[i] = 0xff;
    for (int i = 0; i < 64; i++)
        decoding_table[(unsigned char)encoding_table[i]] = i;
}

long a64l(const char* s)
{
    long result = 0;
    unsigned long magnitude = 1;
    uint8_t i = 0;
    while (*s && i < 6)
    {
        uint8_t digit = decoding_table[(uint8_t)*s];
        if (digit == 0xff)
            break;
        result += digit * magnitude;
        s++;
        i++;
        magnitude *= 64;
    }
    return result;
}

char l64a_buffer[7];

char* l64a(long value)
{
    if (value < 0)
        return NULL;
    uint8_t length = 0;
    for (uint8_t i = 0; i < 6; i++)
    {
        if (value == 0)
            break;
        l64a_buffer[i] = encoding_table[value & 0x3f];
        value >>= 6;
        length++;
    }
    l64a_buffer[length] = 0;
    return l64a_buffer;
}

int abs(int n)
{
    return n < 0 ? -n : n;
}

void (*atexit_stack[32])();
uint8_t atexit_stack_length;

int atexit(void (*function)(void))
{
    if (atexit_stack_length >= 32)
        return 1;
    atexit_stack[atexit_stack_length++] = function;
    return 0;
}

int isdigit(int c)
{
    return (c >= '0' && c <= '9');
}

int isspace(int c)
{
    return (c == ' ' || c == '\t' || c == '\n' || 
            c == '\v' || c == '\f' || c == '\r');
}

int isalpha(int c)
{
    return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

int tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

int toupper(int c)
{
    if (c >= 'a' && c <= 'z')
        return c - ('a' - 'A');
    return c;
}

long strtol(const char* nptr, char** endptr, int base) 
{
    const char*s = nptr;
    long result = 0;
    int neg = 0;
    int overflow = 0;
    int any_digits = 0;
    long cutoff;
    int cutlim;

    while (isspace((unsigned char)*s))
        s++;

    if (*s == '-') 
    {
        neg = 1;
        s++;
    } 
    else if (*s == '+')
        s++;

    if (base == 0) 
    {
        if (*s == '0') 
        {
            if (s[1] == 'x' || s[1] == 'X') 
            {
                base = 16;
                s += 2;
            } 
            else 
            {
                base = 8;
                s++;
            }
        } 
        else 
        {
            base = 10;
        }
    } 
    else if (base == 16) 
    {
        if (*s == '0' && (s[1] == 'x' || s[1] == 'X'))
            s += 2;
    }

    if (base < 2 || base > 36) 
    {
        if (endptr != NULL) 
            *endptr = (char*)nptr;
        errno = EINVAL;
        return 0;
    }

    cutoff = neg ? LONG_MIN / base : LONG_MAX / base;
    cutlim = neg ? -(LONG_MIN % base) : LONG_MAX % base;

    while(true)
    {
        int c = (unsigned char)*s;
        int digit;

        if (isdigit(c)) 
            digit = c - '0';
        else if (isalpha(c)) 
            digit = tolower(c) - 'a' + 10;
        else
            break; 

        if (digit >= base)
            break;

        any_digits = 1;

        if (neg) 
        {
            if (result < cutoff || (result == cutoff && digit > cutlim)) 
            {
                overflow = 1;
                break;
            }
            result = result * base - digit;
        } 
        else 
        {
            if (result > cutoff || (result == cutoff && digit > cutlim)) 
            {
                overflow = 1;
                break;
            }
            result = result * base + digit;
        }
        s++;
    }

    if (!any_digits) 
    {
        if (endptr != NULL) 
            *endptr = (char*)nptr;
        errno = EINVAL;
        return 0;
    }

    if (overflow) 
    {
        errno = ERANGE;
        result = neg ? LONG_MIN : LONG_MAX;
    } 
    else if (neg) 
        result = -result;

    if (endptr != NULL) 
        *endptr = (char*)s;

    return result;
}

int atoi(const char* str)
{
    return (int)strtol(str, (char**)NULL, 10);
}

#ifdef BUILDING_KERNEL
void __attribute__((noreturn)) abort_core(int line, const char* file, const char* function)
{
    printf("Kernel aborted. (function \"%s\" at line %u in file \"%s\")", function, line, file);
    LOG(ERROR, "Kernel aborted. (function \"%s\" at line %u in file \"%s\")", function, line, file);
    fflush(stdout);
    halt();
    __builtin_unreachable();
}
#else
void __attribute__((noreturn)) abort()
{
    exit(EXIT_FAILURE);
    __builtin_unreachable();
}
#endif

void malloc_bitmap_init();
bool malloc_bitmap_get_page(uint64_t page);
void malloc_bitmap_set_page(uint64_t page, bool state);

char* getenv(const char* name)
{
    if (name == NULL) return NULL;
    if (environ == NULL) return NULL;

    int i = 0;
    while (environ[i])
    {
        int j = 0;
        while (environ[i][j] == name[j] && name[j] != 0 && environ[i][j] != 0 && environ[i][j] != '=')
            j++;
        if (environ[i][j] == '=')
            return &environ[i][j + 1];
        i++;
    }

    return NULL;
}

int setenv(const char* name, const char* value, int overwrite)
{
    if (name == NULL || value == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    if (environ == NULL)
    {
        errno = ENOMEM;
        return -1;
    }
    char* old_var = getenv(name);
    if ((!overwrite && old_var))
        return 0;
    int name_len = strlen(name), val_len = strlen(value);
    int i = 0;
    int new_num_environ = num_environ;
    if (old_var)
    {
        if (strcmp(old_var, value) == 0)
            return 0;
        while (environ[i])
        {
            int j = 0;
            while (environ[i][j] == name[j] && name[j] != 0 && environ[i][j] != 0 && environ[i][j] != '=')
            {
                if (name[j] == '=')
                {
                    errno = EINVAL;
                    return -1;
                }
                j++;
            }
            if (environ[i][j] == '=')
                goto allocate_new_var;
            i++;
        }
        errno = EINVAL;
        return -1;
    }

    new_num_environ++;
    i = num_environ;
    // goto allocate_new_var;

allocate_new_var:
    char* new_var = malloc(name_len + 1 + val_len + 1);
    if (!new_var)
    {
        errno = ENOMEM;
        return -1;
    }
    char** new_environ = malloc((new_num_environ + 1) * sizeof(char*));
    if (!new_environ)
    {
        errno = ENOMEM;
        return -1;
    }
    for (int k = 0; k < new_num_environ; k++)
    {
        if (k == i)
            continue;
        if (k < i || i != num_environ)
            new_environ[k] = environ[k];
        else
            new_environ[k] = environ[k - 1];
    }
    free(environ);
    __builtin_memcpy(new_var, name, name_len);
    new_var[name_len] = '=';
    __builtin_memcpy(new_var + name_len + 1, value, val_len);
    new_var[name_len + 1 + val_len] = 0;
    new_environ[new_num_environ] = NULL;
    new_environ[i] = new_var;
    environ = new_environ;
    num_environ++;
    return 0;
}

int unsetenv(const char* name)
{
    if (name == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    if (environ == NULL)
        return 0;
    int i = 0;
    while (environ[i])
    {
        int j = 0;
        while (environ[i][j] == name[j] && name[j] != 0 && environ[i][j] != 0 && environ[i][j] != '=')
        {
            if (name[j] == '=')
            {
                errno = EINVAL;
                return -1;
            }
            j++;
        }
        if (environ[i][j] == '=')
        {
            char** new_environ = malloc(num_environ * sizeof(char*));   // num_environ - 1 + 1
            if (!new_environ)
            {
                errno = ENOMEM;
                return -1;
            }
            for (int k = 0; k < num_environ; k++)
            {
                if (k < i)
                    new_environ[k] = environ[k];
                else
                    new_environ[k] = environ[k + 1];
            }
            free(environ[i]);
            free(environ);
            environ = new_environ;
            num_environ--;
            return 0;
        }
        i++;
    }
    return 0;
}

int system(const char* command)
{
    if (!command) return 1;

    size_t bytes = strlen(command) + 1;
    char* cmd_data = malloc(bytes);
    if (!cmd_data) 
    {
        errno = EAGAIN;
        return -1;
    }
    int return_value = 0;
    strncpy(cmd_data, command, bytes);

    if (bytes <= 1)
    {
        return_value = 1;
        goto do_return;
    }

    size_t characters = bytes - 1;
    bool string = false;
    for (size_t i = 0; i < characters; i++)
    {
        if (cmd_data[i] == ' ' && !string) cmd_data[i] = 0;
        if (cmd_data[i] == '\"') 
        {
            string ^= true;
            cmd_data[i] = 0;
        }
    }
    int bytes_left = characters;
    
    char* first_arg = find_first_arg(cmd_data, &bytes_left);
    if (!first_arg)
    {
        return_value = 127;
        goto do_return;
    }

    char* arg = first_arg;
    int bytes_left_backup = bytes_left;
    int argc = 1;
    while ((arg = find_next_contiguous_string(arg, &bytes_left)))
        argc++;
    bytes_left = bytes_left_backup;

    char** argv = malloc((argc + 1) * sizeof(char*));
    
    if (!argv) 
    {
        return_value = 1;
        goto do_return;
    }

    arg = first_arg;
    argv[0] = arg;
    int i = 1;
    while ((arg = find_next_contiguous_string(arg, &bytes_left)))
        argv[i++] = arg;
    argv[i] = NULL;

    pid_t pgrp = getpgrp();

    pid_t ret = fork();
    if (ret == -1)
    {
        return_value = 0x7f | WEXITBIT;
        goto do_return;
    }

    if (ret == 0)
    {
        execvp(first_arg, argv);
        perror(first_arg);
        exit(0x7f);
    }
    else
    {
        setpgid(ret, ret);
        tcsetpgrp(STDIN_FILENO, ret);
        
        int wstatus;
        waitpid(ret, &wstatus, 0);

        tcsetpgrp(STDIN_FILENO, pgrp);

        return_value = wstatus;

        free(argv);
        goto do_return;
    }

    free(argv);

    fprintf(stderr, "%s: command not found\n", first_arg);

    return_value = 127;
    goto do_return;

do_return:
    free(cmd_data);

    if (WIFSIGNALED(return_value))
    {
        switch (WTERMSIG(return_value))
        {
        case SIGSEGV:
        case SIGBUS:
        case SIGFPE:
        case SIGILL:
            printf("%s", strsignal(WTERMSIG(return_value)));
        default:
        }
    }
    return return_value;
}