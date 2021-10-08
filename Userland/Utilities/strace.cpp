/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Assertions.h>
#include <AK/Format.h>
#include <AK/IPv4Address.h>
#include <AK/StdLibExtras.h>
#include <AK/Types.h>
#include <LibC/sys/arch/i386/regs.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/File.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <syscall.h>
#include <unistd.h>

#define HANDLE(VALUE) \
    case VALUE:       \
        return #VALUE;
#define VALUES_TO_NAMES(FUNC_NAME)     \
    static String FUNC_NAME(int value) \
    {                                  \
        switch (value) {
#define END_VALUES_TO_NAMES()              \
    }                                      \
    return String::formatted("{}", value); \
    }

VALUES_TO_NAMES(errno_name)
HANDLE(EPERM)
HANDLE(ENOENT)
HANDLE(ESRCH)
HANDLE(EINTR)
HANDLE(EIO)
HANDLE(ENXIO)
HANDLE(E2BIG)
HANDLE(ENOEXEC)
HANDLE(EBADF)
HANDLE(ECHILD)
HANDLE(EAGAIN)
HANDLE(ENOMEM)
HANDLE(EACCES)
HANDLE(EFAULT)
HANDLE(ENOTBLK)
HANDLE(EBUSY)
HANDLE(EEXIST)
HANDLE(EXDEV)
HANDLE(ENODEV)
HANDLE(ENOTDIR)
HANDLE(EISDIR)
HANDLE(EINVAL)
HANDLE(ENFILE)
HANDLE(EMFILE)
HANDLE(ENOTTY)
HANDLE(ETXTBSY)
HANDLE(EFBIG)
HANDLE(ENOSPC)
HANDLE(ESPIPE)
HANDLE(EROFS)
HANDLE(EMLINK)
HANDLE(EPIPE)
HANDLE(ERANGE)
HANDLE(ENAMETOOLONG)
HANDLE(ELOOP)
HANDLE(EOVERFLOW)
HANDLE(EOPNOTSUPP)
HANDLE(ENOSYS)
HANDLE(ENOTIMPL)
HANDLE(EAFNOSUPPORT)
HANDLE(ENOTSOCK)
HANDLE(EADDRINUSE)
HANDLE(EWHYTHO)
HANDLE(ENOTEMPTY)
HANDLE(EDOM)
HANDLE(ECONNREFUSED)
HANDLE(EADDRNOTAVAIL)
HANDLE(EISCONN)
HANDLE(ECONNABORTED)
HANDLE(EALREADY)
HANDLE(ECONNRESET)
HANDLE(EDESTADDRREQ)
HANDLE(EHOSTUNREACH)
HANDLE(EILSEQ)
HANDLE(EMSGSIZE)
HANDLE(ENETDOWN)
HANDLE(ENETUNREACH)
HANDLE(ENETRESET)
HANDLE(ENOBUFS)
HANDLE(ENOLCK)
HANDLE(ENOMSG)
HANDLE(ENOPROTOOPT)
HANDLE(ENOTCONN)
HANDLE(EPROTONOSUPPORT)
HANDLE(EDEADLK)
HANDLE(ETIMEDOUT)
HANDLE(EPROTOTYPE)
HANDLE(EINPROGRESS)
HANDLE(ENOTHREAD)
HANDLE(EPROTO)
HANDLE(ENOTSUP)
HANDLE(EPFNOSUPPORT)
HANDLE(EDIRINTOSELF)
HANDLE(EDQUOT)
HANDLE(EMAXERRNO)
END_VALUES_TO_NAMES()

VALUES_TO_NAMES(whence_name)
HANDLE(SEEK_SET)
HANDLE(SEEK_CUR)
HANDLE(SEEK_END)
END_VALUES_TO_NAMES()

VALUES_TO_NAMES(domain_name)
HANDLE(AF_UNSPEC)
HANDLE(AF_UNIX)
HANDLE(AF_INET)
HANDLE(AF_INET6)
END_VALUES_TO_NAMES()

VALUES_TO_NAMES(socket_type_name)
HANDLE(SOCK_STREAM)
HANDLE(SOCK_DGRAM)
HANDLE(SOCK_RAW)
HANDLE(SOCK_RDM)
HANDLE(SOCK_SEQPACKET)
END_VALUES_TO_NAMES()

VALUES_TO_NAMES(protocol_name)
HANDLE(PF_UNSPEC)
HANDLE(PF_UNIX)
HANDLE(PF_INET)
HANDLE(PF_INET6)
END_VALUES_TO_NAMES()

static int g_pid = -1;

#if ARCH(I386)
using syscall_arg_t = u32;
#else
using syscall_arg_t = u64;
#endif

static void handle_sigint(int)
{
    if (g_pid == -1)
        return;

    if (ptrace(PT_DETACH, g_pid, 0, 0) == -1) {
        perror("detach");
    }
}

static void copy_from_process(const void* source_p, Bytes target)
{
    auto source = static_cast<const char*>(source_p);
    size_t offset = 0;
    size_t left = target.size();
    while (left) {
        int value = ptrace(PT_PEEK, g_pid, const_cast<char*>(source) + offset, 0);
        size_t to_copy = min(sizeof(int), left);
        target.overwrite(offset, &value, to_copy);
        left -= to_copy;
        offset += to_copy;
    }
}

static ByteBuffer copy_from_process(const void* source, size_t length)
{
    auto buffer = ByteBuffer::create_uninitialized(length).value();
    copy_from_process(source, buffer.bytes());
    return buffer;
}

template<typename T>
static T copy_from_process(const T* source)
{
    T value {};
    copy_from_process(source, Bytes { &value, sizeof(T) });
    return value;
}

struct PointerArgument {
    const void* value;
};

namespace AK {
template<>
struct Formatter<PointerArgument> : StandardFormatter {
    Formatter() = default;
    explicit Formatter(StandardFormatter formatter)
        : StandardFormatter(formatter)
    {
    }

    void format(FormatBuilder& format_builder, PointerArgument const& value)
    {
        auto& builder = format_builder.builder();
        if (value.value == nullptr)
            builder.append("null");
        else
            builder.appendff("{}", value.value);
    }
};
}

class FormattedSyscallBuilder {

public:
    FormattedSyscallBuilder(Syscall::Function syscall_function)
    {
        m_builder.append(Syscall::to_string(syscall_function));
        m_builder.append('(');
    }

    template<typename T>
    void add_argument(CheckedFormatString<T> format, T&& arg)
    {
        add_argument_separator();
        m_builder.appendff(format.view(), forward<T>(arg));
    }

    template<typename T>
    void add_argument(T&& arg)
    {
        add_argument("{}", forward<T>(arg));
    }

    void add_string_argument(Syscall::StringArgument const& string_argument)
    {
        if (string_argument.characters == nullptr)
            add_argument("null");
        else {
            auto string = copy_from_process(string_argument.characters, string_argument.length);
            add_argument("\"{}\"", StringView(string.data(), string.size()));
        }
    }

    template<typename... Ts>
    void add_arguments(Ts&&... args)
    {
        (add_argument(forward<Ts>(args)), ...);
    }

    template<typename T>
    void format_result_no_error(T res)
    {
        m_builder.appendff(") = {}\n", res);
    }

    void format_result(Integral auto res)
    {
        m_builder.append(") = ");
        if (res < 0)
            m_builder.appendff("{} {}", res, errno_name(-(int)res));
        else
            m_builder.appendff("{}", res);
        m_builder.append('\n');
    }

    void format_result(void* res)
    {
        m_builder.appendff(") = {}\n", res);
    }

    void format_result()
    {
        m_builder.append(")\n");
    }

    StringView string_view()
    {
        return m_builder.string_view();
    }

private:
    void add_argument_separator()
    {
        if (!m_first_arg) {
            m_builder.append(", ");
        }
        m_first_arg = false;
    }

    StringBuilder m_builder;
    bool m_first_arg { true };
};

static void format_getrandom(FormattedSyscallBuilder& builder, void* buffer, size_t size, unsigned flags)
{
    builder.add_arguments(buffer, size, flags);
}

static void format_realpath(FormattedSyscallBuilder& builder, Syscall::SC_realpath_params* params_p)
{
    auto params = copy_from_process(params_p);
    builder.add_string_argument(params.path);
    if (params.buffer.size == 0)
        builder.add_argument("null");
    else {
        auto buffer = copy_from_process(params.buffer.data, params.buffer.size);
        builder.add_argument("\"{}\"", StringView { (const char*)buffer.data() });
    }
}

static void format_exit(FormattedSyscallBuilder& builder, int status)
{
    builder.add_argument(status);
}

static void format_open(FormattedSyscallBuilder& builder, Syscall::SC_open_params* params_p)
{
    auto params = copy_from_process(params_p);

    if (params.dirfd == AT_FDCWD)
        builder.add_argument("AT_FDCWD");
    else
        builder.add_argument(params.dirfd);

    builder.add_string_argument(params.path);

    Vector<StringView> active_flags;
    if (params.options & O_RDWR)
        active_flags.append("O_RDWR");
    else if (params.options & O_RDONLY)
        active_flags.append("O_RDONLY");
    else if (params.options & O_WRONLY)
        active_flags.append("O_WRONLY");

    if (params.options & O_APPEND)
        active_flags.append("O_APPEND");
    if (params.options & O_CREAT)
        active_flags.append("O_CREAT");
    // TODO: etc...

    // TODO: add to FormattedSyscallBuilder
    StringBuilder sbuilder;
    sbuilder.join(" | ", active_flags);
    builder.add_argument(sbuilder.to_string());

    if (params.options & O_CREAT)
        builder.add_argument("{:04o}", params.mode);
}

namespace AK {
template<>
struct Formatter<struct timespec> : StandardFormatter {
    void format(FormatBuilder& format_builder, struct timespec value)
    {
        auto& builder = format_builder.builder();
        builder.appendff("{{tv_sec={}, tv_nsec={}}}", value.tv_sec, value.tv_nsec);
    }
};

template<>
struct Formatter<struct timeval> : StandardFormatter {
    void format(FormatBuilder& format_builder, struct timeval value)
    {
        auto& builder = format_builder.builder();
        builder.appendff("{{tv_sec={}, tv_usec={}}}", value.tv_sec, value.tv_usec);
    }
};

template<>
struct Formatter<struct stat> : StandardFormatter {
    void format(FormatBuilder& format_builder, struct stat value)
    {
        auto& builder = format_builder.builder();
        builder.appendff(
            "{{st_dev={}, st_ino={}, st_mode={}, st_nlink={}, st_uid={}, st_gid={}, st_rdev={}, "
            "st_size={}, st_blksize={}, st_blocks={}, st_atim={}, st_mtim={}, st_ctim={}}}",
            value.st_dev, value.st_ino, value.st_mode, value.st_nlink, value.st_uid, value.st_gid, value.st_rdev,
            value.st_size, value.st_blksize, value.st_blocks, value.st_atim, value.st_mtim, value.st_ctim);
    }
};
}

static void format_fstat(FormattedSyscallBuilder& builder, int fd, struct stat* buf_p)
{
    auto buf = copy_from_process(buf_p);
    builder.add_arguments(fd, buf);
}

static void format_stat(FormattedSyscallBuilder& builder, Syscall::SC_stat_params* params_p)
{
    auto params = copy_from_process(params_p);
    if (params.dirfd == AT_FDCWD)
        builder.add_argument("AT_FDCWD");
    else
        builder.add_argument(params.dirfd);
    builder.add_string_argument(params.path);
    builder.add_arguments(copy_from_process(params.statbuf), params.follow_symlinks);
}

static void format_lseek(FormattedSyscallBuilder& builder, int fd, off_t offset, int whence)
{
    builder.add_arguments(fd, offset, whence_name(whence));
}

static void format_read(FormattedSyscallBuilder& builder, int fd, void* buf, size_t nbyte)
{
    builder.add_arguments(fd, buf, nbyte);
}

static void format_write(FormattedSyscallBuilder& builder, int fd, void* buf, size_t nbyte)
{
    builder.add_arguments(fd, buf, nbyte);
}

static void format_close(FormattedSyscallBuilder& builder, int fd)
{
    builder.add_arguments(fd);
}

static void format_select(FormattedSyscallBuilder& builder, Syscall::SC_select_params* params_p)
{
    // TODO: format fds and sigmask properly
    auto params = copy_from_process(params_p);
    builder.add_arguments(
        params.nfds,
        PointerArgument { params.readfds },
        PointerArgument { params.writefds },
        PointerArgument { params.exceptfds },
        copy_from_process(params.timeout),
        PointerArgument { params.sigmask });
}

namespace AK {
template<>
struct Formatter<struct sockaddr> : StandardFormatter {
    void format(FormatBuilder& format_builder, struct sockaddr address)
    {
        auto& builder = format_builder.builder();
        builder.append("{sa_family=");
        builder.append(domain_name(address.sa_family));
        if (address.sa_family == AF_INET) {
            auto* address_in = (const struct sockaddr_in*)&address;
            builder.appendff(
                ", sin_port={}, sin_addr={}",
                address_in->sin_port,
                IPv4Address(address_in->sin_addr.s_addr).to_string());
        }
        builder.append('}');
    }
};
}

static void format_socket(FormattedSyscallBuilder& builder, int domain, int type, int protocol)
{
    // TODO: show additional options in type
    builder.add_arguments(domain_name(domain), socket_type_name(type & SOCK_TYPE_MASK), protocol_name(protocol));
}

static void format_connect(FormattedSyscallBuilder& builder, int socket, const struct sockaddr* address_p, socklen_t address_len)
{
    builder.add_arguments(socket, copy_from_process(address_p), address_len);
}

static void format_recvmsg(FormattedSyscallBuilder& builder, int socket, struct msghdr* message, int flags)
{
    // TODO: format message
    builder.add_arguments(socket, message);

    Vector<StringView> active_flags;
    if (flags & MSG_OOB)
        active_flags.append("MSG_OOB");
    if (flags & MSG_PEEK)
        active_flags.append("MSG_PEEK");
    // TODO: add MSG_WAITALL once its definition is added
    if (!active_flags.is_empty()) {
        StringBuilder sbuilder;
        sbuilder.join(" | ", active_flags);
        builder.add_argument(sbuilder.to_string());
    } else
        builder.add_argument("0");
}

struct MmapFlags {
    int value;
};

struct MemoryProtectionFlags {
    int value;
};

namespace AK {
template<>
struct Formatter<MmapFlags> : StandardFormatter {
    void format(FormatBuilder& format_builder, MmapFlags value)
    {
        auto& builder = format_builder.builder();
        auto flags = value.value;
        Vector<StringView> active_flags;
        if (flags & MAP_SHARED)
            active_flags.append("MAP_SHARED");
        if (flags & MAP_PRIVATE)
            active_flags.append("MAP_PRIVATE");
        if (flags & MAP_FIXED)
            active_flags.append("MAP_FIXED");
        builder.join(" | ", active_flags);
    }
};

template<>
struct Formatter<MemoryProtectionFlags> : StandardFormatter {
    void format(FormatBuilder& format_builder, MemoryProtectionFlags value)
    {
        auto& builder = format_builder.builder();
        int prot = value.value;
        Vector<StringView> active_prot;
        if (prot == PROT_NONE)
            active_prot.append("PROT_NONE");
        else {
            if (prot & PROT_READ)
                active_prot.append("PROT_READ");
            if (prot & PROT_WRITE)
                active_prot.append("PROT_WRITE");
            if (prot & PROT_EXEC)
                active_prot.append("PROT_EXEC");
        }
        builder.join(" | ", active_prot);
    }
};
}

static void format_mmap(FormattedSyscallBuilder& builder, Syscall::SC_mmap_params* params_p)
{
    auto params = copy_from_process(params_p);
    builder.add_arguments(params.addr, params.size, MemoryProtectionFlags { params.prot }, MmapFlags { params.flags }, params.fd, params.offset, params.alignment);
    builder.add_string_argument(params.name);
}

static void format_munmap(FormattedSyscallBuilder& builder, void* addr, size_t size)
{
    builder.add_arguments(addr, size);
}

static void format_mprotect(FormattedSyscallBuilder& builder, void* addr, size_t size, int prot)
{
    builder.add_arguments(addr, size, MemoryProtectionFlags { prot });
}

static void format_set_mmap_name(FormattedSyscallBuilder& builder, Syscall::SC_set_mmap_name_params* params_p)
{
    auto params = copy_from_process(params_p);
    builder.add_arguments(params.addr, params.size);
    builder.add_string_argument(params.name);
}

static void format_syscall(FormattedSyscallBuilder& builder, Syscall::Function syscall_function, syscall_arg_t arg1, syscall_arg_t arg2, syscall_arg_t arg3, syscall_arg_t res)
{
    enum ResultType {
        Int,
        Ssize,
        VoidP,
        Void
    };

    ResultType result_type { Int };
    switch (syscall_function) {
    case SC_getrandom:
        format_getrandom(builder, (void*)arg1, (size_t)arg2, (unsigned)arg3);
        break;
    case SC_realpath:
        format_realpath(builder, (Syscall::SC_realpath_params*)arg1);
        break;
    case SC_exit:
        format_exit(builder, (int)arg1);
        result_type = Void;
        break;
    case SC_open:
        format_open(builder, (Syscall::SC_open_params*)arg1);
        break;
    case SC_fstat:
        format_fstat(builder, (int)arg1, (struct stat*)arg2);
        result_type = Ssize;
        break;
    case SC_stat:
        format_stat(builder, (Syscall::SC_stat_params*)arg1);
        break;
    case SC_lseek:
        format_lseek(builder, (int)arg1, (off_t)arg2, (int)arg3);
        break;
    case SC_read:
        format_read(builder, (int)arg1, (void*)arg2, (size_t)arg3);
        result_type = Ssize;
        break;
    case SC_write:
        format_write(builder, (int)arg1, (void*)arg2, (size_t)arg3);
        result_type = Ssize;
        break;
    case SC_close:
        format_close(builder, (int)arg1);
        break;
    case SC_select:
        format_select(builder, (Syscall::SC_select_params*)arg1);
        break;
    case SC_socket:
        format_socket(builder, (int)arg1, (int)arg2, (int)arg3);
        break;
    case SC_recvmsg:
        format_recvmsg(builder, (int)arg1, (struct msghdr*)arg2, (int)arg3);
        result_type = Ssize;
        break;
    case SC_connect:
        format_connect(builder, (int)arg1, (const struct sockaddr*)arg2, (socklen_t)arg3);
        break;
    case SC_mmap:
        format_mmap(builder, (Syscall::SC_mmap_params*)arg1);
        result_type = VoidP;
        break;
    case SC_munmap:
        format_munmap(builder, (void*)arg1, (size_t)arg2);
        break;
    case SC_mprotect:
        format_mprotect(builder, (void*)arg1, (size_t)arg2, (int)arg3);
        break;
    case SC_set_mmap_name:
        format_set_mmap_name(builder, (Syscall::SC_set_mmap_name_params*)arg1);
        break;
    default:
        builder.add_arguments((void*)arg1, (void*)arg2, (void*)arg3);
        result_type = VoidP;
    }

    switch (result_type) {
    case Int:
        builder.format_result((int)res);
        break;
    case Ssize:
        builder.format_result((ssize_t)res);
        break;
    case VoidP:
        builder.format_result((void*)res);
        break;
    case Void:
        builder.format_result();
        break;
    }
}

int main(int argc, char** argv)
{
    if (pledge("stdio wpath cpath proc exec ptrace sigaction", nullptr) < 0) {
        perror("pledge");
        return 1;
    }

    Vector<const char*> child_argv;

    const char* output_filename = nullptr;
    auto trace_file = Core::File::standard_error();

    Core::ArgsParser parser;
    parser.set_stop_on_first_non_option(true);
    parser.set_general_help(
        "Trace all syscalls and their result.");
    parser.add_option(g_pid, "Trace the given PID", "pid", 'p', "pid");
    parser.add_option(output_filename, "Filename to write output to", "output", 'o', "output");
    parser.add_positional_argument(child_argv, "Arguments to exec", "argument", Core::ArgsParser::Required::No);

    parser.parse(argc, argv);

    if (output_filename != nullptr) {
        auto open_result = Core::File::open(output_filename, Core::OpenMode::WriteOnly);
        if (open_result.is_error()) {
            outln(stderr, "Failed to open output file: {}", open_result.error());
            return 1;
        }
        trace_file = open_result.value();
    }

    if (pledge("stdio proc exec ptrace sigaction", nullptr) < 0) {
        perror("pledge");
        return 1;
    }

    int status;
    if (g_pid == -1) {
        if (child_argv.is_empty()) {
            warnln("strace: Expected either a pid or some arguments");
            return 1;
        }

        child_argv.append(nullptr);
        int pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }

        if (!pid) {
            if (ptrace(PT_TRACE_ME, 0, 0, 0) == -1) {
                perror("traceme");
                return 1;
            }
            int rc = execvp(child_argv.first(), const_cast<char**>(child_argv.data()));
            if (rc < 0) {
                perror("execvp");
                exit(1);
            }
            VERIFY_NOT_REACHED();
        }

        g_pid = pid;
        if (waitpid(pid, &status, WSTOPPED | WEXITED) != pid || !WIFSTOPPED(status)) {
            perror("waitpid");
            return 1;
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, nullptr);

    if (ptrace(PT_ATTACH, g_pid, 0, 0) == -1) {
        perror("attach");
        return 1;
    }
    if (waitpid(g_pid, &status, WSTOPPED | WEXITED) != g_pid || !WIFSTOPPED(status)) {
        perror("waitpid");
        return 1;
    }

    for (;;) {
        if (ptrace(PT_SYSCALL, g_pid, 0, 0) == -1) {
            perror("syscall");
            return 1;
        }
        if (waitpid(g_pid, &status, WSTOPPED | WEXITED) != g_pid || !WIFSTOPPED(status)) {
            perror("wait_pid");
            return 1;
        }
        PtraceRegisters regs = {};
        if (ptrace(PT_GETREGS, g_pid, &regs, 0) == -1) {
            perror("getregs");
            return 1;
        }
#if ARCH(I386)
        syscall_arg_t syscall_index = regs.eax;
        syscall_arg_t arg1 = regs.edx;
        syscall_arg_t arg2 = regs.ecx;
        syscall_arg_t arg3 = regs.ebx;
#else
        syscall_arg_t syscall_index = regs.rax;
        syscall_arg_t arg1 = regs.rdx;
        syscall_arg_t arg2 = regs.rcx;
        syscall_arg_t arg3 = regs.rbx;
#endif

        if (ptrace(PT_SYSCALL, g_pid, 0, 0) == -1) {
            perror("syscall");
            return 1;
        }
        if (waitpid(g_pid, &status, WSTOPPED | WEXITED) != g_pid || !WIFSTOPPED(status)) {
            perror("wait_pid");
            return 1;
        }

        if (ptrace(PT_GETREGS, g_pid, &regs, 0) == -1) {
            perror("getregs");
            return 1;
        }

#if ARCH(I386)
        u32 res = regs.eax;
#else
        u64 res = regs.rax;
#endif

        auto syscall_function = (Syscall::Function)syscall_index;
        FormattedSyscallBuilder builder(syscall_function);
        format_syscall(builder, syscall_function, arg1, arg2, arg3, res);

        if (!trace_file->write(builder.string_view())) {
            warnln("write: {}", trace_file->error_string());
            return 1;
        }
    }
}
