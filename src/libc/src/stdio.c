#ifndef BUILDING_KERNEL
#include "../include/math.h"
#include "math_float_util.h"
#include "math_fmod.c"
#endif

#include "../include/fcntl.h"

int putchar(int c)
{
    unsigned char ch = (unsigned char)c;
    if (fputc(ch, stdout) == EOF) return EOF;
    return ch;
}

int puts(const char* s)
{
    if (!s) 
    {
        errno = EINVAL; 
        return EOF;
    }
    if (fwrite(s, strlen(s), 1, stdout) != 1) return EOF;
    if (putchar('\n') == EOF) return EOF;
    return 0;
}

int fputs(const char* s, FILE* stream)
{
    if (!s) 
    {
        errno = EINVAL; 
        return EOF;
    }
    if (fwrite(s, strlen(s), 1, stream) != 1) return EOF;
    return 0;
}

#define LM_NONE 0
#define LM_L    1
#define LM_LL   2
#define LM_Z    3

int _printf(int (*func_c)(char), int (*func_s)(const char*), const char* format, va_list args)
{
    int len = 0;
    void print_string(const char* str)
    {
        if (!str)
            len += func_s("(null)");
        else
            len += func_s(str);
    }
    void print_char(char ch)
    {
        len += func_c(ch);
    }
    void print_unsigned(uint64_t num, bool pad_with_zeroes, bool pad_with_spaces, uint8_t precision)
    {
        uint64_t div = 10000000000000000000ULL; // * max is 1.8446744e+19 
        uint8_t power = 19;
        bool do_print = false;
        while (div >= 1)
        {
            uint8_t digit = num / div;
            if (digit != 0 || power < 1)
                do_print = true;
            if (pad_with_zeroes && power < precision)
                do_print = true;
            if (do_print)
                print_char('0' + digit);
            else if (pad_with_spaces && power < precision)
                print_char(' ');
            num -= ((uint64_t)digit * div);
            div /= 10;
            power--;
        }
    }
    void print_hex(uint64_t num, bool caps, bool pad_with_zeroes, bool pad_with_spaces, uint8_t precision)
    {
        const char* hex = "0123456789abcdef";
        const char* HEX = "0123456789ABCDEF";

        int8_t offset = 60;
        bool do_print = false;
        while (offset >= 0)
        {
            uint8_t digit = (num >> offset) & 0xf;
            if (digit != 0 || offset < 4)
                do_print = true;
            if (pad_with_zeroes && offset < 4 * precision)
                do_print = true;
            if (do_print)
                print_char(caps ? HEX[digit] : hex[digit]);
            else if (pad_with_spaces && offset < 4 * precision)
                print_char(' ');
            offset -= 4;
        }
    }
    void print_signed(int64_t num, bool leave_blank, bool plus_sign, bool pad_with_zeroes, uint8_t precision)
    {
        if (num < 0)
        {
            print_char('-');
            num = -num;
        }
        else if (plus_sign)
        {
            print_char('+');
        }
        else if (leave_blank)
        {
            print_char(' ');
        }
        print_unsigned((uint64_t)num, pad_with_zeroes, leave_blank, precision);
    }
    void parse_specifier(size_t* i)
    {
        unsigned int length_modifier = LM_NONE;
        size_t start_i = *i;
        size_t num_chars = 0;
        bool caps = false;
        bool alternate_form = false;
        bool leave_blank = false;
        bool plus_sign = false;
        bool pad_with_zeroes = true;
        int precision = 1;
        bool reading_precision = false;
    parse:
        num_chars++;
        if (reading_precision)
        {
            if (format[*i] >= '0' && format[*i] <= '9')
            {
                precision *= 10;
                precision += (format[*i] - '0') * (precision < 0 ? -1 : 1);
                (*i)++;
                goto parse;
            }
            else
                reading_precision = false;
        }
        switch (format[*i])
        {
        case 0:
            return;
    // * -------------------- Flag characters
        case '#':
            alternate_form = true;
            (*i)++;
            goto parse;
        case ' ':
            leave_blank = true;
            pad_with_zeroes = false;
            (*i)++;
            goto parse;
        case '+':
            plus_sign = true;
            (*i)++;
            goto parse;
        case '0':
            pad_with_zeroes = true;
            leave_blank = false;
            (*i)++;
            goto parse;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            reading_precision = true;
            precision = (format[*i] - '0');
            (*i)++;
            goto parse;
    // * -------------------- Precision
        case '.':
            reading_precision = true;
            precision = 0;
            (*i)++;
            goto parse;
    // * -------------------- Length modifiers
        case 'l':
            length_modifier = length_modifier == LM_L ? LM_LL : LM_L;
            (*i)++;
            goto parse;
        case 'Z':
        case 'z':
            length_modifier = LM_Z;
            (*i)++;
            goto parse;
    // * -------------------- Conversion specifiers
        case 'c':
            print_char(va_arg(args, int));
            (*i)++;
            break;
        case 'm':
            print_string(strerror(errno));
            (*i)++;
            break;
        case 's':
            print_string(va_arg(args, const char*));
            (*i)++;
            break;
        case 'p':
        {
            void* p = va_arg(args, void*);
            print_string("0x");
            print_hex((uint64_t)p, false, false, leave_blank, precision);
            (*i)++;
            break;
        }
        case '%':
            print_char('%');
            (*i)++;
            break;
        case 'u':
        {
            switch (length_modifier)
            {
            case LM_L:
                unsigned long num_l = va_arg(args, unsigned long);
                print_unsigned(num_l, pad_with_zeroes, leave_blank, precision);
                break;
            case LM_LL:
                unsigned long long num_ll = va_arg(args, unsigned long long);
                print_unsigned(num_ll, pad_with_zeroes, leave_blank, precision);
                break;
            case LM_Z:
                size_t num_z = va_arg(args, size_t);
                print_unsigned(num_z, pad_with_zeroes, leave_blank, precision);
                break;
            default:
                unsigned int num = va_arg(args, unsigned int);
                print_unsigned(num, pad_with_zeroes, leave_blank, precision);
            }
            (*i)++;
            break;
        }
        case 'i':
        case 'd':
        {
            switch (length_modifier)
            {
            case LM_L:
                long num_l = va_arg(args, long);
                print_signed(num_l, leave_blank, plus_sign, pad_with_zeroes, precision);
                break;
            case LM_LL:
                long long num_ll = va_arg(args, long long);
                print_signed(num_ll, leave_blank, plus_sign, pad_with_zeroes, precision);
                break;
            case LM_Z:
                ssize_t num_z = va_arg(args, ssize_t);
                print_signed(num_z, leave_blank, plus_sign, pad_with_zeroes, precision);
                break;
            default:
                int num = va_arg(args, int);
                print_signed(num, leave_blank, plus_sign, pad_with_zeroes, precision);
            }
            (*i)++;
            break;
        }
        case 'X':
            caps = true;
        case 'x':
        {
            if (alternate_form)
            {
                if (caps)
                    print_string("0X");
                else
                    print_string("0x");
            }
            switch (length_modifier)
            {
            case LM_L:
                unsigned long num_l = va_arg(args, unsigned long);
                print_hex(num_l, caps, pad_with_zeroes, leave_blank, precision);
                break;
            case LM_LL:
                unsigned long long num_ll = va_arg(args, unsigned long long);
                print_hex(num_ll, caps, pad_with_zeroes, leave_blank, precision);
                break;
            case LM_Z:
                size_t num_z = va_arg(args, size_t);
                print_hex(num_z, caps, pad_with_zeroes, leave_blank, precision);
                break;
            default:
                unsigned int num = va_arg(args, unsigned int);
                print_hex(num, caps, pad_with_zeroes, leave_blank, precision);
            }
            (*i)++;
            break;
        }
        default:
            print_char('%');
            (*i)++;
            while (start_i < *i)
                print_char(format[start_i++]);
        }
    }
    size_t i = 0;
    char ch;
    while ((ch = format[i++]))
    {
        switch (ch)
        {
        case '%':
            parse_specifier(&i);
            break;
        default:
            print_char(ch);
        }
    }
    return len;
}

int sprintf(char* buffer, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int length = vsprintf(buffer, format, args);
    va_end(args);
    return length;
}

int snprintf(char* buffer, size_t bufsz, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int length = vsnprintf(buffer, bufsz, format, args);
    va_end(args);
    return length;
}

int vsprintf(char* buffer, const char* format, va_list args)
{
    uint32_t index = 0;
    int _putc(char c)
    {
        buffer[index++] = c;
        return 1;
    }
    int _puts(const char* s)
    {
        int tot = 0;
        while (*s)
        {
            _putc(*s);
            s++;
            tot++;
        }
        return tot;
    }

    int length = _printf(_putc, _puts, format, args);
    buffer[index] = 0;
    return length;
}

int vsnprintf(char* buffer, size_t bufsz, const char* format, va_list args)
{
    if (bufsz == 0) return 0;

    uint32_t index = 0;
    int _putc(char c)
    {
        if (index < bufsz - 1)
            buffer[index++] = c;
        return 1;
    }
    int _puts(const char* s)
    {
        int tot = 0;
        while (*s)
        {
            _putc(*s);
            s++;
            tot++;
        }
        return tot;
    }

    _printf(_putc, _puts, format, args);
    buffer[index] = 0;
    return index;
}

int vprintf(const char* format, va_list args)
{
    int _putc(char c)
    {
        putchar(c);
        return 1;
    }
    int _puts(const char* s)
    {
        int tot = 0;
        while (*s)
        {
            _putc(*s);
            s++;
            tot++;
        }
        return tot;
    }

    return _printf(_putc, _puts, format, args);
}

int dprintf(int fd, const char*format, ...)
{
    va_list args;
    va_start(args, format);
    int length = vdprintf(fd, format, args);
    va_end(args);
    return length;
}

int vdprintf(int fd, const char*format, va_list args)
{
    int _putc(char c)
    {
        write(fd, &c, 1);
        return 1;
    }
    int _puts(const char* s)
    {
        size_t len = strlen(s);
        write(fd, s, len);
        return (int)len;
    }

    return _printf(_putc, _puts, format, args);
}

int fprintf(FILE* stream, const char*format, ...)
{
    va_list args;
    va_start(args, format);
    int length = vfprintf(stream, format, args);
    va_end(args);
    return length;
}

int vfprintf(FILE* stream, const char*format, va_list args)
{
    int _putc(char c)
    {
        fwrite(&c, 1, 1, stream);
        return 1;
    }
    int _puts(const char* s)
    {
        size_t len = strlen(s);
        fwrite(s, len, 1, stream);
        return (int)len;
    }

    return _printf(_putc, _puts, format, args);
}

int printf(const char*format, ...)
{
    va_list args;
    va_start(args, format);
    int length = vfprintf(stdout, format, args);
    va_end(args);
    return length;
}

FILE* fopen(const char* path, const char* mode)
{
    if (!path || !mode) 
    {
        errno = EINVAL;
        return NULL;
    }

    int plus = 0;
    int excl = 0;
    int cloexec = 0;
    char base = '\0';

    for (const char* p = mode; *p; p++) 
    {
        char c = *p;
        switch (c) 
        {
        case 'r': 
        case 'w': 
        case 'a':
            if (base != '\0') 
            {
                errno = EINVAL;
                return NULL;
            }
            base = c;
            break;
            
        case '+':
            plus = 1;
            break;
        case 'x':
            excl = 1;
            break;
        case 'e':
            cloexec = 1;
            break;

        case 'b':
        case 't':
            break;
        default:
            errno = EINVAL;
            return NULL;
        }
    }

    if (base == '\0') 
    {
        errno = EINVAL;
        return NULL;
    }

    int oflags = 0;
    mode_t cmode = 0666;

    if (base == 'r')
        oflags = plus ? O_RDWR : O_RDONLY;
    else if (base == 'w') 
    {
        oflags = plus ? O_RDWR : O_WRONLY;
        oflags |= O_CREAT | O_TRUNC;
    } 
    else 
    {
        oflags = plus ? O_RDWR : O_WRONLY;
        oflags |= O_CREAT | O_APPEND;
    }

    if (cloexec) oflags |= O_CLOEXEC;

    if (excl && (oflags & O_CREAT)) oflags |= O_EXCL;
    else if (excl && !(oflags & O_CREAT)) 
    {
        errno = EINVAL;
        return NULL;
    }

    int fd;
    if (oflags & O_CREAT)
        fd = open(path, oflags, cmode);
    else
        fd = open(path, oflags);

    if (fd == -1) 
        return NULL;

    FILE* stream = malloc(sizeof(FILE));
    if (!stream) 
    {
        int saved = errno;
        close(fd);
        errno = saved ? saved : ENOMEM;
        return NULL;
    }

    memset(stream, 0, sizeof(FILE));

    unsigned char* buf = malloc(BUFSIZ);
    if (!buf) 
    {
        int saved = errno;
        close(fd);
        free(stream);
        errno = saved ? saved : ENOMEM;
        return NULL;
    }

    stream->fd = fd;
    stream->buffer = buf;
    stream->buffer_size = BUFSIZ;
    stream->buffer_index = 0;
    stream->buffer_end_index = 0;

    stream->flags = 0;
    stream->current_flags = 0;

    if (plus)
        stream->flags |= (FILE_FLAGS_READ | FILE_FLAGS_WRITE);
    else 
    {
        if (base == 'r') 
            stream->flags |= FILE_FLAGS_READ;
        else stream->flags |= FILE_FLAGS_WRITE;
    }

    stream->flags |= FILE_FLAGS_FBF;

    if ((stream->flags & FILE_FLAGS_WRITE) && isatty(fd)) 
    {
        stream->flags &= ~FILE_FLAGS_FBF;
        stream->flags |= FILE_FLAGS_LBF;
    }

    stream->flags |= FILE_FLAGS_BF_ALLOC;

    stream->buffer_mode = (stream->flags & FILE_FLAGS_READ) ? FILE_BFMD_READ : FILE_BFMD_WRITE;

    return stream;
}

int fclose(FILE* stream)
{
    if (!stream) 
    {
        errno = EBADF;
        return EOF;
    }
    int ret_f = fflush(stream);
    if (stream->buffer && (stream->flags & FILE_FLAGS_BF_ALLOC)) free(stream->buffer);
    int ret_c = close(stream->fd);
    free(stream);
    return (ret_f == EOF || ret_c == -1) ? EOF : 0;
}

size_t fread(void* ptr, size_t size, size_t nitems, FILE* stream)
{
    if (!stream)
    {
        errno = EBADF;
        return 0;
    }

    if (!ptr)
    {
        errno = EINVAL;
        return 0;
    }

    if (!(stream->flags & FILE_FLAGS_READ)) 
    {
        stream->current_flags |= FILE_CFLAGS_ERR;
        errno = EBADF;
        return 0;
    }

    if (stream->current_flags & FILE_CFLAGS_EOF)
        return 0;

    if (size == 0 || nitems == 0) return 0;

    if (stream->fd == STDIN_FILENO) // ! Not defined by the standard but a lot of programs rely on it
        fflush(stdout);

    if (stream->buffer_mode == FILE_BFMD_WRITE)
    {
        fflush(stream);
        stream->buffer_mode = FILE_BFMD_READ;
        int ret = read(stream->fd, stream->buffer, stream->buffer_size);
        if (ret < 0) 
        {
            stream->current_flags |= FILE_CFLAGS_ERR;
            stream->buffer_end_index = 0;
            return 0;
        }
        stream->buffer_end_index = ret;
    }

    size_t bytes = size * nitems; // ! Might overflow
    size_t bytes_copied;
    for (size_t i = 0; i < bytes; i += bytes_copied)
    {
        if (stream->buffer_index >= stream->buffer_end_index)
        {
            stream->buffer_index = 0;
            int ret = read(stream->fd, stream->buffer, stream->buffer_size);
            if (ret == 0)
            {
                stream->current_flags |= FILE_CFLAGS_EOF;
                stream->buffer_end_index = 0;
                return i / size;
            }
            if (ret < 0)
            {
                stream->current_flags |= FILE_CFLAGS_ERR;
                return i / size;
            }
            
            stream->buffer_end_index = ret;
        }
        bytes_copied = stream->buffer_end_index - stream->buffer_index;
        __builtin_memcpy(ptr + i, stream->buffer, bytes_copied);
        stream->buffer_index += bytes_copied;
    }
    return nitems;
}

size_t fwrite(const void* ptr, size_t size, size_t nitems, FILE* stream)
{
    if (!stream)
    {
        errno = EBADF;
        return 0;
    }

    if (!ptr)
    {
        errno = EINVAL;
        return 0;
    }

    if (!(stream->flags & FILE_FLAGS_WRITE)) 
    {
        stream->current_flags |= FILE_CFLAGS_ERR;
        errno = EBADF;
        return 0;
    }

    if (!stream || !ptr || size == 0 || nitems == 0)
    {
        if (stream)
            stream->current_flags |= FILE_CFLAGS_ERR;
        return 0;
    }

    if (stream->buffer_mode == FILE_BFMD_READ)
    {
        stream->buffer_index = 0;
        stream->buffer_end_index = 0;
        stream->buffer_mode = FILE_BFMD_WRITE;
    }

    const uint8_t* data = ptr;
    size_t bytes = size * nitems;

    for (size_t i = 0; i < bytes; i++)
    {
        if (stream->buffer_index >= stream->buffer_size)
        {
            if (fflush(stream) != 0)
                return i / size;
        }

        stream->buffer[stream->buffer_index++] = data[i];

        if ((stream->flags & FILE_FLAGS_LBF) && data[i] == '\n')
        {
            if (fflush(stream) != 0)
                return i / size;
        }

        if (stream->flags & FILE_FLAGS_NBF)
        {
            if (fflush(stream) != 0)
                return i / size;
        }
    }

    return nitems;
}

int fflush(FILE* stream)
{
    if (!stream)
    {
        errno = EBADF;
        return EOF;
    }

    if (stream->buffer_mode == FILE_BFMD_READ) 
        return 0;

    if (stream->buffer_mode == FILE_BFMD_READ || stream->buffer_index == 0)
        return 0;

    if (write(stream->fd, stream->buffer, minint(stream->buffer_index, stream->buffer_size)) < 0)
        return EOF; // * write already sets errno appropriately
    stream->buffer_index = 0;

    return 0;
}

int feof(FILE* stream)
{
    return (stream->current_flags & FILE_CFLAGS_EOF) != 0;  // Posix doesn't imply that it should be 0 or 1 but helps
}

int ferror(FILE* stream)
{
    return (stream->current_flags & FILE_CFLAGS_ERR) != 0;
}

int fileno(FILE* stream)
{
    if (!stream) return (errno = EBADF, -1);
    return stream->fd;
}

int fgetc(FILE* stream)
{
    unsigned char c;
    size_t ret = fread(&c, 1, 1, stream);
    if (ret != 1) return EOF;
    return (int)c;
}

int fputc(int c, FILE* stream)
{
    if (!stream) { errno = EBADF; return EOF; }
    unsigned char ch = (unsigned char)c;
    size_t ret = fwrite(&ch, 1, 1, stream);
    return ret == 1 ? (int)ch : EOF;
}

int getchar()
{
    return getc(stdin);
}

void perror(const char* prefix)
{
    int _errno = errno;
    fprintf(stderr, "%s", prefix);
    if (_errno != 0)
    {
        if (strcmp(prefix, "") != 0)
            fprintf(stderr, ": ");
        fprintf(stderr, "%s", strerror(_errno));
    }
    fputc('\n', stderr);
}