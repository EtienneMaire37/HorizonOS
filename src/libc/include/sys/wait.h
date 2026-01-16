#pragma once

static unsigned char wstatus_return_value;

#define WEXITSTATUS(wstatus)    (wstatus_return_value = (unsigned char)wstatus, *(char*)&wstatus_return_value)
#define WTERMSIG(wstatus)       (wstatus_return_value = (unsigned char)wstatus, *(char*)&wstatus_return_value)

#define WEXITBIT                0x00010000
#define WSIGNALBIT              0x00020000

#define WIFEXITED(wstatus)      ((unsigned int)wstatus & WEXITBIT)
#define WIFSIGNALED(wstatus)    ((unsigned int)wstatus & WSIGNALBIT)

typedef int pid_t;

pid_t waitpid(pid_t pid, int* wstatus, int options);