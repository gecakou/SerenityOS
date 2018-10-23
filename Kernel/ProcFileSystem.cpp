#include "ProcFileSystem.h"
#include "Task.h"

RetainPtr<ProcFileSystem> ProcFileSystem::create()
{
    return adopt(*new ProcFileSystem);
}

ProcFileSystem::ProcFileSystem()
{
}

ProcFileSystem::~ProcFileSystem()
{
}

bool ProcFileSystem::initialize()
{
    SyntheticFileSystem::initialize();
    addFile(createGeneratedFile("summary", [] {
        cli();
        auto tasks = Task::allTasks();
        char* buffer;
        auto stringImpl = StringImpl::createUninitialized(tasks.size() * 64, buffer);
        memset(buffer, 0, stringImpl->length());
        char* ptr = buffer;
        ptr += ksprintf(ptr, "PID    OWNER      STATE  NAME\n");
        for (auto* task : tasks) {
            ptr += ksprintf(ptr, "%w   %w:%w  %b     %s\n",
                task->pid(),
                task->uid(),
                task->gid(),
                task->state(),
                task->name().characters());
        }
        *ptr = '\0';
        sti();
        return ByteBuffer::copy((byte*)buffer, ptr - buffer);
    }));
    return true;
}

const char* ProcFileSystem::className() const
{
    return "procfs";
}
