/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2023, Linus Groh <linusg@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/DeprecatedString.h>
#include <AK/LexicalPath.h>
#include <AK/Platform.h>
#include <AK/String.h>
#include <AK/StringBuilder.h>
#include <LibCore/SessionManagement.h>
#include <LibCore/StandardPaths.h>
#include <stdlib.h>

#if !defined(AK_OS_WINDOWS)
#    include <pwd.h>
#    include <unistd.h>
#endif

namespace Core {

DeprecatedString StandardPaths::home_directory()
{
#if defined(AK_OS_WINDOWS)
    auto* home_env = getenv("USERPROFILE");
    DeprecatedString path { home_env, strlen(home_env) };
#else
    if (auto* home_env = getenv("HOME"))
        return LexicalPath::canonicalized_path(home_env);

    auto* pwd = getpwuid(getuid());
    DeprecatedString path = pwd ? pwd->pw_dir : "/";
    endpwent();
#endif
    return LexicalPath::canonicalized_path(path);
}

DeprecatedString StandardPaths::desktop_directory()
{
    StringBuilder builder;
    builder.append(home_directory());
    builder.append("/Desktop"sv);
    return LexicalPath::canonicalized_path(builder.to_deprecated_string());
}

DeprecatedString StandardPaths::documents_directory()
{
    StringBuilder builder;
    builder.append(home_directory());
    builder.append("/Documents"sv);
    return LexicalPath::canonicalized_path(builder.to_deprecated_string());
}

DeprecatedString StandardPaths::downloads_directory()
{
    StringBuilder builder;
    builder.append(home_directory());
    builder.append("/Downloads"sv);
    return LexicalPath::canonicalized_path(builder.to_deprecated_string());
}

DeprecatedString StandardPaths::config_directory()
{
    if (auto* config_directory = getenv("XDG_CONFIG_HOME"))
        return LexicalPath::canonicalized_path(config_directory);

    StringBuilder builder;
    builder.append(home_directory());
#if defined(AK_OS_MACOS)
    builder.append("/Library/Preferences"sv);
#else
    builder.append("/.config"sv);
#endif
    return LexicalPath::canonicalized_path(builder.to_deprecated_string());
}

DeprecatedString StandardPaths::data_directory()
{
    if (auto* data_directory = getenv("XDG_DATA_HOME"))
        return LexicalPath::canonicalized_path(data_directory);

    StringBuilder builder;
    builder.append(home_directory());
#if defined(AK_OS_SERENITY)
    builder.append("/.data"sv);
#elif defined(AK_OS_MACOS)
    builder.append("/Library/Application Support"sv);
#else
    builder.append("/.local/share"sv);
#endif

    return LexicalPath::canonicalized_path(builder.to_deprecated_string());
}

ErrorOr<DeprecatedString> StandardPaths::runtime_directory()
{
    if (auto* data_directory = getenv("XDG_RUNTIME_DIR"))
        return LexicalPath::canonicalized_path(data_directory);

    StringBuilder builder;

#if defined(AK_OS_SERENITY)
    auto sid = TRY(Core::SessionManagement::root_session_id());
    builder.appendff("/tmp/session/{}", sid);
#elif defined(AK_OS_MACOS)
    builder.append(home_directory());
    builder.append("/Library/Application Support"sv);
#else
    auto uid = getuid();
    builder.appendff("/run/user/{}", uid);
#endif

    return LexicalPath::canonicalized_path(builder.to_deprecated_string());
}

DeprecatedString StandardPaths::tempfile_directory()
{
#if defined(AK_OS_WINDOWS)
    auto* temp_env = getenv("TEMP");
    return { temp_env, strlen(temp_env) };
#else
    return "/tmp";
#endif
}

ErrorOr<Vector<String>> StandardPaths::font_directories()
{
    return Vector { {
#if defined(AK_OS_SERENITY)
        TRY(String::from_utf8("/res/fonts"sv)),
#elif defined(AK_OS_MACOS)
        TRY(String::from_utf8("/System/Library/Fonts"sv)),
        TRY(String::from_utf8("/Library/Fonts"sv)),
        TRY(String::formatted("{}/Library/Fonts"sv, home_directory())),
#else
        TRY(String::from_utf8("/usr/share/fonts"sv)),
        TRY(String::from_utf8("/usr/local/share/fonts"sv)),
        TRY(String::formatted("{}/.local/share/fonts"sv, home_directory())),
#endif
    } };
}

}
