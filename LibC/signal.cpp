#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <Kernel/Syscall.h>

extern "C" {

int kill(pid_t pid, int sig)
{
    int rc = Syscall::invoke(Syscall::SC_kill, (dword)pid, (dword)sig);
    __RETURN_WITH_ERRNO(rc, rc, -1);
}

int killpg(int pgrp, int sig)
{
    int rc = Syscall::invoke(Syscall::SC_killpg, (dword)pgrp, (dword)sig);
    __RETURN_WITH_ERRNO(rc, rc, -1);
}

sighandler_t signal(int signum, sighandler_t handler)
{
    struct sigaction new_act;
    struct sigaction old_act;
    new_act.sa_handler = handler;
    new_act.sa_flags = 0;
    new_act.sa_mask = 0;
    new_act.sa_restorer = nullptr;
    int rc = sigaction(signum, &new_act, &old_act);
    if (rc < 0)
        return SIG_ERR;
    return old_act.sa_handler;
}

int sigaction(int signum, const struct sigaction* act, struct sigaction* old_act)
{
    int rc = Syscall::invoke(Syscall::SC_sigaction, (dword)signum, (dword)act, (dword)old_act);
    __RETURN_WITH_ERRNO(rc, rc, -1);
}

int sigemptyset(sigset_t* set)
{
    *set = 0;
    return 0;
}

int sigfillset(sigset_t* set)
{
    *set = 0xffffffff;
    return 0;
}

int sigaddset(sigset_t* set, int sig)
{
    if (sig < 1 || sig > 32) {
        errno = EINVAL;
        return -1;
    }
    *set |= 1 << (sig);
    return 0;
}

int sigdelset(sigset_t* set, int sig)
{
    if (sig < 1 || sig > 32) {
        errno = EINVAL;
        return -1;
    }
    *set &= ~(1 << (sig));
    return 0;
}

int sigismember(const sigset_t* set, int sig)
{
    if (sig < 1 || sig > 32) {
        errno = EINVAL;
        return -1;
    }
    if (*set & (1 << (sig)))
        return 1;
    return 0;
}

int sigprocmask(int how, const sigset_t* set, sigset_t* old_set)
{
    int rc = Syscall::invoke(Syscall::SC_sigprocmask, (dword)how, (dword)set, (dword)old_set);
    __RETURN_WITH_ERRNO(rc, rc, -1);
}

int sigpending(sigset_t* set)
{
    int rc = Syscall::invoke(Syscall::SC_sigpending, (dword)set);
    __RETURN_WITH_ERRNO(rc, rc, -1);
}

const char* sys_siglist[NSIG] = {
#undef __SIGNAL
#define __SIGNAL(a, b) b,
    __ENUMERATE_ALL_SIGNALS
#undef __SIGNAL
};

}
