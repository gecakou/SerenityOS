/*
 * Copyright (c) 2020, Itamar S. <itamar8910@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Command.h"
#include <AK/Format.h>
#include <AK/ScopeGuard.h>
#include <LibCore/File.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

namespace Core {

// Only supported in serenity mode because we use `posix_spawn_file_actions_addchdir`
#ifdef __serenity__

String command(String const& command_string, Optional<LexicalPath> chdir)
{
    auto parts = command_string.split(' ');
    if (parts.is_empty())
        return {};
    auto program = parts[0];
    parts.remove(0);
    return command(program, parts, chdir);
}

String command(String const& program, const Vector<String>& arguments, Optional<LexicalPath> chdir)
{
    int stdout_pipe[2] = {};
    int stderr_pipe[2] = {};
    if (pipe2(stdout_pipe, O_CLOEXEC)) {
        perror("pipe2");
        VERIFY_NOT_REACHED();
    }
    if (pipe2(stderr_pipe, O_CLOEXEC)) {
        perror("pipe2");
        VERIFY_NOT_REACHED();
    }

    auto close_pipes = ScopeGuard([stderr_pipe, stdout_pipe] {
        // The write-ends of these pipes are closed manually
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
    });

    Vector<char const*> parts = { program.characters() };
    for (auto const& part : arguments) {
        parts.append(part.characters());
    }
    parts.append(nullptr);

    char const** argv = parts.data();

    posix_spawn_file_actions_t action;
    posix_spawn_file_actions_init(&action);
    if (chdir.has_value()) {
        posix_spawn_file_actions_addchdir(&action, chdir.value().string().characters());
    }
    posix_spawn_file_actions_adddup2(&action, stdout_pipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&action, stderr_pipe[1], STDERR_FILENO);

    pid_t pid;
    if ((errno = posix_spawnp(&pid, program.characters(), &action, nullptr, const_cast<char**>(argv), environ))) {
        perror("posix_spawn");
        VERIFY_NOT_REACHED();
    }
    int wstatus;
    waitpid(pid, &wstatus, 0);
    posix_spawn_file_actions_destroy(&action);

    // close the write-ends so reading wouldn't block
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    auto read_all_from_pipe = [](int pipe[2]) {
        auto result_file = Core::File::construct();
        if (!result_file->open(pipe[0], Core::OpenMode::ReadOnly, Core::File::ShouldCloseFileDescriptor::Yes)) {
            perror("open");
            VERIFY_NOT_REACHED();
        }
        return String::copy(result_file->read_all());
    };

    if (WEXITSTATUS(wstatus) != 0) {
#    ifdef DBG_FAILED_COMMANDS
        dbgln("command failed. stderr: {}", read_all_from_pipe(stderr_pipe));
#    endif
        return {};
    }

    auto result = read_all_from_pipe(stdout_pipe);
    if (result.is_null())
        return "";
    return result;
}

#endif

}
