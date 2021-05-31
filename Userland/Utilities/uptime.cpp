/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Format.h>
#include <stdio.h>
#include <unistd.h>

int main(int, char**)
{
    if (pledge("stdio rpath", nullptr) < 0) {
        perror("pledge");
        return 1;
    }

    FILE* fp = fopen("/proc/uptime", "r");
    if (!fp) {
        perror("fopen(/proc/uptime)");
        return 1;
    }

    if (pledge("stdio", nullptr) < 0) {
        perror("pledge");
        return 1;
    }

    char buffer[BUFSIZ];
    auto* p = fgets(buffer, sizeof(buffer), fp);
    if (!p) {
        perror("fgets");
        return 1;
    }

    unsigned seconds;
    sscanf(buffer, "%u", &seconds);

    out("Up ");

    if (seconds / 86400 > 0) {
        out("{} day{}, ", seconds / 86400, (seconds / 86400) == 1 ? "" : "s");
        seconds %= 86400;
    }

    if (seconds / 3600 > 0) {
        out("{} hour{}, ", seconds / 3600, (seconds / 3600) == 1 ? "" : "s");
        seconds %= 3600;
    }

    if (seconds / 60 > 0) {
        out("{} minute{}, ", seconds / 60, (seconds / 60) == 1 ? "" : "s");
        seconds %= 60;
    }

    out("{} second{}", seconds, seconds == 1 ? "" : "s");
    outln();

    fclose(fp);
    return 0;
}
