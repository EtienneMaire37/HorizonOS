#include <signal.h>

#define SIGDEF_TERM 0
#define SIGDEF_IGN  1
#define SIGDEF_CORE 2
#define SIGDEF_STOP 3
#define SIGDEF_CONT 4

static inline int sig_default_action(int sig) 
{
    switch (sig) 
    {
        case SIGABRT:
        case SIGBUS:
        case SIGFPE:
        case SIGILL:
        case SIGQUIT:
        case SIGSEGV:
        case SIGSYS:
        case SIGTRAP:
        case SIGXCPU:
        case SIGXFSZ:
        case SIGSTKFLT:
            return SIGDEF_CORE;

        case SIGCHLD:
        case SIGURG:
        case SIGWINCH:
            return SIGDEF_IGN;

        case SIGSTOP:
        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:
            return SIGDEF_STOP;

        case SIGCONT:
            return SIGDEF_CONT;

        case SIGALRM:
        case SIGHUP:
        case SIGINT:
        case SIGKILL:
        case SIGPIPE:
        case SIGTERM:
        case SIGUSR1:
        case SIGUSR2:
        case SIGPROF:
        case SIGVTALRM:
        case SIGPOLL:
        case SIGPWR:
            return SIGDEF_TERM;

    // * "The default action for an unhandled real-time signal is to
    // *    terminate the receiving process."
        default:
            return SIGDEF_TERM;
    }
}
