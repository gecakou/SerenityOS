/*
 * Copyright (c) 2021, János Tóth <toth-janos@outlook.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Vector.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

char const* usage = "usage:\n"
                    "\tdd <options>\n"
                    "options:\n"
                    "\tif=<file>\tinput file (default: stdin)\n"
                    "\tof=<file>\toutput file (default: stdout)\n"
                    "\tbs=<size>\tblocks size may be followed by multiplicate suffixes: k=1024, M=1024*1024, G=1024*1024*1024 (default: 512)\n"
                    "\tcount=<size>\t<size> blocks to copy (default: 0 (until end-of-file))\n"
                    "\tseek=<size>\tskip <size> blocks at start of output (default: 0)\n"
                    "\tskip=<size>\tskip <size> blocks at start of input (default: 0)\n"
                    "\tstatus=<level>\tlevel of output (default: default)\n"
                    "\t\t\tdefault - error messages + final statistics\n"
                    "\t\t\tnone - just error messages\n"
                    "\t\t\tnoxfer - no final statistics\n"
                    "\t--help\t\tshows this text\n";

enum Status {
    Default,
    None,
    Noxfer
};

static String split_at_equals(char const* argument)
{
    String string_value(argument);

    auto values = string_value.split_limit('=', 2);
    if (values.size() < 2) {
        warnln("Unable to parse: {}", argument);
        return {};
    } else {
        return values[1];
    }
}

static int handle_io_file_arguments(int& fd, int flags, char const* argument)
{
    auto value = split_at_equals(argument);
    if (value.is_empty()) {
        return -1;
    }

    fd = open(value.characters(), flags, 0666);
    if (fd == -1) {
        warnln("Unable to open: {}", value);
        return -1;
    } else {
        return 0;
    }
}

static int handle_size_arguments(size_t& numeric_value, char const* argument)
{
    auto value = split_at_equals(argument);
    if (value.is_empty()) {
        return -1;
    }

    unsigned suffix_multiplier = 1;
    switch (value.to_lowercase()[value.length() - 1]) {
    case 'k':
        suffix_multiplier = KiB;
        value = value.substring(0, value.length() - 1);
        break;
    case 'm':
        suffix_multiplier = MiB;
        value = value.substring(0, value.length() - 1);
        break;
    case 'g':
        suffix_multiplier = GiB;
        value = value.substring(0, value.length() - 1);
        break;
    }

    Optional<unsigned> numeric_optional = value.to_uint();
    if (!numeric_optional.has_value()) {
        warnln("Invalid size-value: {}", value);
        return -1;
    }

    numeric_value = numeric_optional.value() * suffix_multiplier;
    if (numeric_value < 1) {
        warnln("Invalid size-value: {}", numeric_value);
        return -1;
    } else {
        return 0;
    }
}

static int handle_status_arguments(Status& status, char const* argument)
{
    auto value = split_at_equals(argument);
    if (value.is_empty()) {
        return -1;
    }

    if (value == "default") {
        status = Default;
        return 0;
    } else if (value == "noxfer") {
        status = Noxfer;
        return 0;
    } else if (value == "none") {
        status = None;
        return 0;
    } else {
        warnln("Unknown status: {}", value);
        return -1;
    }
}

int main(int argc, char** argv)
{
    int input_fd = 0;
    int input_flags = O_RDONLY;
    int output_fd = 1;
    int output_flags = O_CREAT | O_WRONLY | O_TRUNC;
    size_t block_size = 512;
    size_t count = 0;
    size_t skip = 0;
    size_t seek = 0;
    Status status = Default;

    size_t total_bytes_copied = 0;
    size_t total_blocks_in = 0, partial_blocks_in = 0;
    size_t total_blocks_out = 0, partial_blocks_out = 0;
    uint8_t* buffer = nullptr;
    ssize_t nread = 0, nwritten = 0;

    for (int a = 1; a < argc; a++) {
        if (!strcmp(argv[a], "--help")) {
            out("{}", usage);
            return 0;
        } else if (!strncmp(argv[a], "if=", 3)) {
            if (handle_io_file_arguments(input_fd, input_flags, argv[a]) < 0) {
                return 1;
            }
        } else if (!strncmp(argv[a], "of=", 3)) {
            if (handle_io_file_arguments(output_fd, output_flags, argv[a]) < 0) {
                return 1;
            }
        } else if (!strncmp(argv[a], "bs=", 3)) {
            if (handle_size_arguments(block_size, argv[a]) < 0) {
                return 1;
            }
        } else if (!strncmp(argv[a], "count=", 6)) {
            if (handle_size_arguments(count, argv[a]) < 0) {
                return 1;
            }
        } else if (!strncmp(argv[a], "seek=", 5)) {
            if (handle_size_arguments(seek, argv[a]) < 0) {
                return 1;
            }
        } else if (!strncmp(argv[a], "skip=", 5)) {
            if (handle_size_arguments(skip, argv[a]) < 0) {
                return 1;
            }
        } else if (!strncmp(argv[a], "status=", 7)) {
            if (handle_status_arguments(status, argv[a]) < 0) {
                return 1;
            }
        } else {
            warn("{}", usage);
            return 1;
        }
    }

    if ((buffer = (uint8_t*)malloc(block_size)) == nullptr) {
        warnln("Unable to allocate {} bytes for the buffer.", block_size);
        return -1;
    }

    if (seek > 0) {
        if (lseek(output_fd, seek * block_size, SEEK_SET) < 0) {
            warnln("Unable to seek {} bytes.", seek * block_size);
            return -1;
        }
    }

    while (1) {
        nread = read(input_fd, buffer, block_size);
        if (nread < 0) {
            warnln("Cannot read from the input.");
            break;
        } else if (nread == 0) {
            break;
        } else {
            if ((size_t)nread != block_size) {
                partial_blocks_in++;
            } else {
                total_blocks_in++;
            }

            if (partial_blocks_in + total_blocks_in <= skip) {
                continue;
            }

            nwritten = write(output_fd, buffer, nread);
            if (nwritten < 0) {
                warnln("Cannot write to the output.");
                break;
            } else if (nwritten == 0) {
                break;
            } else {
                if ((size_t)nwritten < block_size) {
                    partial_blocks_out++;
                } else {
                    total_blocks_out++;
                }

                total_bytes_copied += nwritten;

                if (count > 0 && (partial_blocks_out + total_blocks_out) >= count) {
                    break;
                }
            }
        }
    }

    if (status == Default) {
        warnln("{}+{} blocks in", total_blocks_in, partial_blocks_in);
        warnln("{}+{} blocks out", total_blocks_out, partial_blocks_out);
        warnln("{} bytes copied.", total_bytes_copied);
    }

    free(buffer);

    if (input_fd != 0) {
        close(input_fd);
    }

    if (output_fd != 1) {
        close(output_fd);
    }

    return 0;
}
