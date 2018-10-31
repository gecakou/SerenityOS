#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <AK/String.h>

struct passwd_with_strings : public passwd {
    char name_buffer[256];
    char passwd_buffer[256];
    char gecos_buffer[256];
    char dir_buffer[256];
    char shell_buffer[256];
};

static FILE* __pwdb_stream = nullptr;
static unsigned __pwdb_line_number = 0;
static struct passwd_with_strings* __pwdb_entry = nullptr;

void setpwent()
{
    __pwdb_line_number = 0;
    if (__pwdb_stream) {
        rewind(__pwdb_stream);
    } else {
        __pwdb_stream = fopen("/etc/passwd", "r");
        __pwdb_entry = (struct passwd_with_strings*)mmap(nullptr, getpagesize());
    }
}

void endpwent()
{
    __pwdb_line_number = 0;
    if (__pwdb_stream) {
        fclose(__pwdb_stream);
        __pwdb_stream = nullptr;
    }
    if (__pwdb_entry) {
        munmap(__pwdb_entry, getpagesize());
        __pwdb_entry = nullptr;
    }
}

struct passwd* getpwuid(uid_t uid)
{
    setpwent();
    while (auto* pw = getpwent()) {
        if (pw->pw_uid == uid)
            return pw;
    }
    return nullptr;
}

struct passwd* getpwnam(const char* name)
{
    setpwent();
    while (auto* pw = getpwent()) {
        if (!strcmp(pw->pw_name, name))
            return pw;
    }
    return nullptr;
}

struct passwd* getpwent()
{
    if (!__pwdb_stream)
        setpwent();

    if (feof(__pwdb_stream))
        return nullptr;

next_entry:
    char buffer[1024];
    ++__pwdb_line_number;
    char* s = fgets(buffer, sizeof(buffer), __pwdb_stream);
    if (!s)
        return nullptr;
    if (feof(__pwdb_stream))
        return nullptr;
    String line(s);
    auto parts = line.split(':');
    if (parts.size() != 7) {
        fprintf(stderr, "getpwent(): Malformed entry on line %u\n", __pwdb_line_number);
        goto next_entry;
    }
    auto& e_name = parts[0];
    auto& e_passwd = parts[1];
    auto& e_uid_string = parts[2];
    auto& e_gid_string = parts[3];
    auto& e_gecos = parts[4];
    auto& e_dir = parts[5];
    auto& e_shell = parts[6];
    bool ok;
    uid_t e_uid = e_uid_string.toUInt(ok);
    if (!ok) {
        fprintf(stderr, "getpwent(): Malformed UID on line %u\n", __pwdb_line_number);
        goto next_entry;
    }
    gid_t e_gid = e_gid_string.toUInt(ok);
    if (!ok) {
        fprintf(stderr, "getpwent(): Malformed GID on line %u\n", __pwdb_line_number);
        goto next_entry;
    }
    __pwdb_entry->pw_uid = e_uid;
    __pwdb_entry->pw_gid = e_gid;
    __pwdb_entry->pw_name = __pwdb_entry->name_buffer;
    __pwdb_entry->pw_passwd = __pwdb_entry->passwd_buffer;
    __pwdb_entry->pw_gecos = __pwdb_entry->gecos_buffer;
    __pwdb_entry->pw_dir = __pwdb_entry->dir_buffer;
    __pwdb_entry->pw_shell = __pwdb_entry->shell_buffer;
    strncpy(__pwdb_entry->name_buffer, e_name.characters(), e_name.length());
    strncpy(__pwdb_entry->passwd_buffer, e_passwd.characters(), e_passwd.length());
    strncpy(__pwdb_entry->gecos_buffer, e_gecos.characters(), e_gecos.length());
    strncpy(__pwdb_entry->dir_buffer, e_dir.characters(), e_dir.length());
    strncpy(__pwdb_entry->shell_buffer, e_shell.characters(), e_shell.length());
    return __pwdb_entry;
}
