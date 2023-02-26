/*
 * Copyright (c) 2021, Brandon Pruitt <brapru@pm.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/DeprecatedString.h>
#include <AK/HashMap.h>
#include <AK/IPv4Address.h>
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/QuickSort.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/File.h>
#include <LibCore/ProcessStatisticsReader.h>
#include <LibCore/System.h>
#include <LibMain/Main.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

constexpr int max_formatted_address_length = 21;

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    TRY(Core::System::pledge("stdio rpath unix"));

    bool flag_all = false;
    bool flag_list = false;
    bool flag_tcp = false;
    bool flag_udp = false;
    bool flag_numeric = false;
    bool flag_program = false;
    bool flag_wide = false;

    Core::ArgsParser args_parser;
    args_parser.set_general_help("Display network connections");
    TRY(args_parser.add_option(flag_all, "Display both listening and non-listening sockets", "all", 'a'));
    TRY(args_parser.add_option(flag_list, "Display only listening sockets", "list", 'l'));
    TRY(args_parser.add_option(flag_tcp, "Display only TCP network connections", "tcp", 't'));
    TRY(args_parser.add_option(flag_udp, "Display only UDP network connections", "udp", 'u'));
    TRY(args_parser.add_option(flag_numeric, "Display numerical addresses", "numeric", 'n'));
    TRY(args_parser.add_option(flag_program, "Show the PID and name of the program to which each socket belongs", "program", 'p'));
    TRY(args_parser.add_option(flag_wide, "Do not truncate IP addresses by printing out the whole symbolic host", "wide", 'W'));
    TRY(args_parser.parse(arguments));

    TRY(Core::System::unveil("/sys/kernel/net", "r"));
    TRY(Core::System::unveil("/sys/kernel/processes", "r"));
    TRY(Core::System::unveil("/etc/passwd", "r"));
    TRY(Core::System::unveil("/etc/services", "r"));
    if (!flag_numeric)
        TRY(Core::System::unveil("/tmp/portal/lookup", "rw"));

    TRY(Core::System::unveil(nullptr, nullptr));

    bool has_protocol_flag = (flag_tcp || flag_udp);

    uid_t current_uid = getuid();

    HashMap<pid_t, DeprecatedString> programs;

    if (flag_program) {
        auto processes = TRY(Core::ProcessStatisticsReader::get_all());

        for (auto& proc : processes.processes) {
            programs.set(proc.pid, proc.name);
        }
    }

    enum class Alignment {
        Left,
        Right
    };

    struct Column {
        DeprecatedString title;
        Alignment alignment { Alignment::Left };
        int width { 0 };
        DeprecatedString buffer;
    };

    Vector<Column> columns;

    int protocol_column = -1;
    int bytes_in_column = -1;
    int bytes_out_column = -1;
    int local_address_column = -1;
    int peer_address_column = -1;
    int state_column = -1;
    int program_column = -1;

    auto add_column = [&](auto title, auto alignment, auto width) {
        columns.append({ title, alignment, width, {} });
        return columns.size() - 1;
    };

    protocol_column = add_column("Proto", Alignment::Left, 5);
    bytes_in_column = add_column("Bytes-In", Alignment::Right, 9);
    bytes_out_column = add_column("Bytes-Out", Alignment::Right, 9);
    local_address_column = add_column("Local Address", Alignment::Left, 22);
    peer_address_column = add_column("Peer Address", Alignment::Left, 22);
    state_column = add_column("State", Alignment::Left, 11);
    program_column = flag_program ? add_column("PID/Program", Alignment::Left, 11) : -1;

    auto print_column = [](auto& column, auto& string) {
        if (!column.width) {
            out("{}", string);
            return;
        }
        if (column.alignment == Alignment::Right) {
            out("{:>{1}}  "sv, string, column.width);
        } else {
            out("{:<{1}}  "sv, string, column.width);
        }
    };

    auto get_formatted_address = [&](DeprecatedString const& address, DeprecatedString const& port) {
        if (flag_wide)
            return DeprecatedString::formatted("{}:{}", address, port);

        if ((address.length() + port.length()) <= max_formatted_address_length)
            return DeprecatedString::formatted("{}:{}", address, port);

        return DeprecatedString::formatted("{}:{}", address.substring_view(0, max_formatted_address_length - port.length()), port);
    };

    auto get_formatted_program = [&](pid_t pid) {
        if (pid == -1)
            return DeprecatedString("-");

        auto program = programs.get(pid);
        return DeprecatedString::formatted("{}/{}", pid, program.value());
    };

    if (!has_protocol_flag || flag_tcp || flag_udp) {
        if (flag_program && current_uid != 0) {
            outln("(Some processes could not be identified, non-owned process info will not be shown)");
        }

        out("Active Internet connections ");

        if (flag_all) {
            outln("(servers and established)");
        } else {
            if (flag_list)
                outln("(only servers)");
            else
                outln("(without servers)");
        }

        for (auto& column : columns)
            print_column(column, column.title);
        outln();
    }

    if (!has_protocol_flag || flag_tcp) {
        auto file = TRY(Core::File::open("/sys/kernel/net/tcp"sv, Core::File::OpenMode::Read));
        auto file_contents = TRY(file->read_until_eof());
        auto json_or_error = JsonValue::from_string(file_contents);
        if (json_or_error.is_error()) {
            warnln("Error: {}", json_or_error.error());
            return 1;
        }
        auto json = json_or_error.release_value();

        Vector<JsonValue> sorted_regions = json.as_array().values();
        quick_sort(sorted_regions, [](auto& a, auto& b) {
            return a.as_object().get_u32("local_port"sv).value_or(0) < b.as_object().get_u32("local_port"sv).value_or(0);
        });

        for (auto& value : sorted_regions) {
            auto& if_object = value.as_object();

            auto bytes_in = if_object.get_deprecated_string("bytes_in"sv).value_or({});
            auto bytes_out = if_object.get_deprecated_string("bytes_out"sv).value_or({});

            auto peer_address = if_object.get_deprecated_string("peer_address"sv).value_or({});
            if (!flag_numeric) {
                auto from_string = IPv4Address::from_string(peer_address);
                auto addr = from_string.value().to_in_addr_t();
                auto* hostent = gethostbyaddr(&addr, sizeof(in_addr), AF_INET);
                if (hostent != nullptr) {
                    auto host_name = StringView { hostent->h_name, strlen(hostent->h_name) };
                    if (!host_name.is_empty())
                        peer_address = host_name;
                }
            }

            auto peer_port = if_object.get_deprecated_string("peer_port"sv).value_or({});
            if (!flag_numeric) {
                auto service = getservbyport(htons(if_object.get_u32("peer_port"sv).value_or(0)), "tcp");
                if (service != nullptr) {
                    auto s_name = StringView { service->s_name, strlen(service->s_name) };
                    if (!s_name.is_empty())
                        peer_port = s_name;
                }
            }

            auto local_address = if_object.get_deprecated_string("local_address"sv).value_or({});
            if (!flag_numeric) {
                auto from_string = IPv4Address::from_string(local_address);
                auto addr = from_string.value().to_in_addr_t();
                auto* hostent = gethostbyaddr(&addr, sizeof(in_addr), AF_INET);
                if (hostent != nullptr) {
                    auto host_name = StringView { hostent->h_name, strlen(hostent->h_name) };
                    if (!host_name.is_empty())
                        local_address = host_name;
                }
            }

            auto local_port = if_object.get_deprecated_string("local_port"sv).value_or({});
            if (!flag_numeric) {
                auto service = getservbyport(htons(if_object.get_u32("local_port"sv).value_or(0)), "tcp");
                if (service != nullptr) {
                    auto s_name = StringView { service->s_name, strlen(service->s_name) };
                    if (!s_name.is_empty())
                        local_port = s_name;
                }
            }

            auto state = if_object.get_deprecated_string("state"sv).value_or({});
            auto origin_pid = (if_object.has("origin_pid"sv)) ? if_object.get_u32("origin_pid"sv).value_or(0) : -1;

            if (!flag_all && ((state == "Listen" && !flag_list) || (state != "Listen" && flag_list)))
                continue;

            if (protocol_column != -1)
                columns[protocol_column].buffer = "tcp";
            if (bytes_in_column != -1)
                columns[bytes_in_column].buffer = bytes_in;
            if (bytes_out_column != -1)
                columns[bytes_out_column].buffer = bytes_out;
            if (local_address_column != -1)
                columns[local_address_column].buffer = get_formatted_address(local_address, local_port);
            if (peer_address_column != -1)
                columns[peer_address_column].buffer = get_formatted_address(peer_address, peer_port);
            if (state_column != -1)
                columns[state_column].buffer = state;
            if (flag_program && program_column != -1)
                columns[program_column].buffer = get_formatted_program(origin_pid);

            for (auto& column : columns)
                print_column(column, column.buffer);
            outln();
        };
    }

    if (!has_protocol_flag || flag_udp) {
        auto file = TRY(Core::File::open("/sys/kernel/net/udp"sv, Core::File::OpenMode::Read));
        auto file_contents = TRY(file->read_until_eof());
        auto json = TRY(JsonValue::from_string(file_contents));

        Vector<JsonValue> sorted_regions = json.as_array().values();
        quick_sort(sorted_regions, [](auto& a, auto& b) {
            return a.as_object().get_u32("local_port"sv).value_or(0) < b.as_object().get_u32("local_port"sv).value_or(0);
        });

        for (auto& value : sorted_regions) {
            auto& if_object = value.as_object();

            auto local_address = if_object.get_deprecated_string("local_address"sv).value_or({});
            if (!flag_numeric) {
                auto from_string = IPv4Address::from_string(local_address);
                auto addr = from_string.value().to_in_addr_t();
                auto* hostent = gethostbyaddr(&addr, sizeof(in_addr), AF_INET);
                if (hostent != nullptr) {
                    auto host_name = StringView { hostent->h_name, strlen(hostent->h_name) };
                    if (!host_name.is_empty())
                        local_address = host_name;
                }
            }

            auto local_port = if_object.get_deprecated_string("local_port"sv).value_or({});
            if (!flag_numeric) {
                auto service = getservbyport(htons(if_object.get_u32("local_port"sv).value_or(0)), "udp");
                if (service != nullptr) {
                    auto s_name = StringView { service->s_name, strlen(service->s_name) };
                    if (!s_name.is_empty())
                        local_port = s_name;
                }
            }

            auto peer_address = if_object.get_deprecated_string("peer_address"sv).value_or({});
            if (!flag_numeric) {
                auto from_string = IPv4Address::from_string(peer_address);
                auto addr = from_string.value().to_in_addr_t();
                auto* hostent = gethostbyaddr(&addr, sizeof(in_addr), AF_INET);
                if (hostent != nullptr) {
                    auto host_name = StringView { hostent->h_name, strlen(hostent->h_name) };
                    if (!host_name.is_empty())
                        peer_address = host_name;
                }
            }

            auto peer_port = if_object.get_deprecated_string("peer_port"sv).value_or({});
            if (!flag_numeric) {
                auto service = getservbyport(htons(if_object.get_u32("peer_port"sv).value_or(0)), "udp");
                if (service != nullptr) {
                    auto s_name = StringView { service->s_name, strlen(service->s_name) };
                    if (!s_name.is_empty())
                        peer_port = s_name;
                }
            }

            auto origin_pid = (if_object.has("origin_pid"sv)) ? if_object.get_u32("origin_pid"sv).value_or(0) : -1;

            if (protocol_column != -1)
                columns[protocol_column].buffer = "udp";
            if (bytes_in_column != -1)
                columns[bytes_in_column].buffer = "-";
            if (bytes_out_column != -1)
                columns[bytes_out_column].buffer = "-";
            if (local_address_column != -1)
                columns[local_address_column].buffer = get_formatted_address(local_address, local_port);
            if (peer_address_column != -1)
                columns[peer_address_column].buffer = get_formatted_address(peer_address, peer_port);
            if (state_column != -1)
                columns[state_column].buffer = "-";
            if (flag_program && program_column != -1)
                columns[program_column].buffer = get_formatted_program(origin_pid);

            for (auto& column : columns)
                print_column(column, column.buffer);
            outln();
        };
    }

    return 0;
}
