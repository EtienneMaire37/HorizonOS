#pragma once

#define SYSCALL_EXIT        0

#define SYSCALL_TIME        1

#define SYSCALL_READ        2
#define SYSCALL_WRITE       3

#define SYSCALL_GETPID      4
#define SYSCALL_FORK        5

#define SYSCALL_BRK         6

#define SYSCALL_CHDIR       7
#define SYSCALL_GETCWD      18

#define SYSCALL_EXECVE      8
#define SYSCALL_WAITPID     9

#define SYSCALL_ACCESS      10
#define SYSCALL_STAT        11
#define SYSCALL_FSTAT       25

#define SYSCALL_READDIR     12

#define SYSCALL_ISATTY      13

#define SYSCALL_OPEN        14
#define SYSCALL_CLOSE       15

#define SYSCALL_TCGETATTR   16
#define SYSCALL_TCSETATTR   17

#define SYSCALL_REALPATH    19

#define SYSCALL_LSEEK       20

#define SYSCALL_TCGETPGRP   21
#define SYSCALL_TCSETPGRP   22

#define SYSCALL_GETPGID     23
#define SYSCALL_SETPGID     24

#define SYSCALL_FLUSH_INPUT_BUFFER  0x40
#define SYSCALL_SET_KB_LAYOUT       0x8000