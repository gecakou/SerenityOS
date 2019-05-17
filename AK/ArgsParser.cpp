#include "ArgsParser.h"
#include "StringBuilder.h"

#include <stdio.h>

namespace AK {

bool ArgsParserResult::is_present(const String& arg_name) const
{
    return m_args.contains(arg_name);
}

String ArgsParserResult::get(const String& arg_name) const
{
    return m_args.get(arg_name);
}

const Vector<String>& ArgsParserResult::get_single_values() const
{
    return m_single_values;
}

ArgsParser::Arg::Arg(const String& name, const String& description, bool required)
    : name(name), description(description), required(required)
{}

ArgsParser::Arg::Arg(const String& name, const String& value_name, const String& description, bool required)
    : name(name), description(description), value_name(value_name), required(required)
{}

ArgsParser::ArgsParser(const String& program_name, const String& prefix)
    : m_program_name(program_name), m_prefix(prefix)
{}

ArgsParserResult ArgsParser::parse(const int argc, const char** argv) 
{
    ArgsParserResult res;

    // We should have at least one parameter
    if (argc < 2)
        return {};

    // We parse the first parameter at the index 1
    if (parse_next_param(1, argv, argc - 1, res) != 0)
        return {};

    if (!check_required_args(res))
        return {};

    return res;
}

int ArgsParser::parse_next_param(const int index, const char** argv, const int params_left, ArgsParserResult& res) 
{
    if (params_left == 0)
        return 0;

    String param = argv[index];

    // We check if the prefix is found at the beginning of the param name
    if (is_param_valid(param)) {
        auto prefix_length = m_prefix.length();
        String param_name = param.substring(prefix_length, param.length() - prefix_length);

        auto arg = m_args.find(param_name);
        if (arg == m_args.end()) {
            printf("Unknown arg \"");
            if (!param_name.is_null())
                printf("%s", param_name.characters());
            printf("\"\n");
            return -1;
        }

        // If this parameter must be followed by a value, we look for it
        if (!arg->value.value_name.is_null()) {
            if (params_left < 1) {
                printf("Missing value for argument %s\n", arg->value.name.characters());
                return -1;
            }

            String next = String(argv[index + 1]);

            if (is_param_valid(next)) {
                printf("Missing value for argument %s\n", arg->value.name.characters());
                return -1;
            }

            res.m_args.set(arg->value.name, next);
            return parse_next_param(index + 2, argv, params_left - 2, res);
        }

        // Single argument, not followed by a value
        res.m_args.set(arg->value.name, "");
        return parse_next_param(index + 1, argv, params_left - 1, res);
    }

    // Else, it's a value alone, a file name parameter for example
    res.m_single_values.append(param);
    return parse_next_param(index + 1, argv, params_left - 1, res);
}

bool ArgsParser::is_param_valid(const String& param_name)
{
    return param_name.substring(0, m_prefix.length()) == m_prefix;
}

bool ArgsParser::check_required_args(const ArgsParserResult& res)
{
    for (auto& it : m_args) {
        if (it.value.required) {
            if (!res.is_present(it.value.name))
                return false;
        }
    }
    return true;
}

void ArgsParser::add_arg(const String& name, const String& description, bool required)
{
    m_args.set(name, Arg(name, description, required));
}

void ArgsParser::add_arg(const String& name, const String& value_name, const String& description, bool required)
{
    m_args.set(name, Arg(name, value_name, description, required));
}

String ArgsParser::get_usage() const
{
    StringBuilder sb;

    sb.append("usage : ");
    sb.append(m_program_name);
    sb.append(" ");

    for (auto& it : m_args) {
        if (it.value.required)
            sb.append("<");
        else
            sb.append("[");

        sb.append(m_prefix);
        sb.append(it.value.name);

        if (!it.value.value_name.is_null()) {
            sb.append(" ");
            sb.append(it.value.value_name);
        }

        if (it.value.required)
            sb.append("> ");
        else
            sb.append("] ");
    }

    sb.append("\n");

    for (auto& it : m_args) {
        sb.append("    ");
        sb.append(m_prefix);
        sb.append(it.value.name);

        if (!it.value.value_name.is_null()) {
            sb.append(" ");
            sb.append(it.value.value_name);
        }

        sb.append(" : ");
        sb.append(it.value.description);
        sb.append("\n");
    }

    return sb.to_string();
}

void ArgsParser::print_usage() const
{
    printf("%s\n", get_usage().characters());
}

}
