#include <AK/Types.h>
#include "kmalloc.h"
#include "i386.h"
#include "i8253.h"
#include <Kernel/Devices/KeyboardDevice.h>
#include "Process.h"
#include "PIC.h"
#include <Kernel/Devices/IDEDiskDevice.h>
#include <Kernel/Devices/DiskPartition.h>
#include "KSyms.h"
#include <Kernel/Devices/NullDevice.h>
#include <Kernel/Devices/ZeroDevice.h>
#include <Kernel/Devices/FullDevice.h>
#include <Kernel/Devices/RandomDevice.h>
#include <Kernel/FileSystem/Ext2FileSystem.h>
#include <Kernel/FileSystem/VirtualFileSystem.h>
#include <Kernel/VM/MemoryManager.h>
#include <Kernel/FileSystem/ProcFS.h>
#include "RTC.h"
#include <Kernel/TTY/VirtualConsole.h>
#include "Scheduler.h"
#include <Kernel/Devices/PS2MouseDevice.h>
#include <Kernel/TTY/PTYMultiplexer.h>
#include <Kernel/FileSystem/DevPtsFS.h>
#include <Kernel/Devices/BXVGADevice.h>
#include <Kernel/Net/E1000NetworkAdapter.h>
#include <Kernel/Net/NetworkTask.h>
#include <Kernel/Devices/DebugLogDevice.h>
#include <Kernel/Multiboot.h>

//#define STRESS_TEST_SPAWNING

VirtualConsole* tty0;
VirtualConsole* tty1;
VirtualConsole* tty2;
VirtualConsole* tty3;
KeyboardDevice* keyboard;
PS2MouseDevice* ps2mouse;
DebugLogDevice* dev_debuglog;
NullDevice* dev_null;
VFS* vfs;

#ifdef STRESS_TEST_SPAWNING
[[noreturn]] static void spawn_stress()
{
    dword last_sum_alloc = sum_alloc;

    for (unsigned i = 0; i < 10000; ++i) {
        int error;
        Process::create_user_process("/bin/true", (uid_t)100, (gid_t)100, (pid_t)0, error, { }, { }, tty0);
        dbgprintf("malloc stats: alloc:%u free:%u eternal:%u !delta:%u\n", sum_alloc, sum_free, kmalloc_sum_eternal, sum_alloc - last_sum_alloc);
        last_sum_alloc = sum_alloc;
        sleep(60);
    }
    for (;;) {
        asm volatile("hlt");
    }
}
#endif

// TODO: delete this magic number. this block offset corresponds to a
// partition that starts at 32k into an MBR disk. this value is also specified
// in sync.sh, but should ideally be read from the MBR header at startup.
#define PARTITION_OFFSET 62

[[noreturn]] static void init_stage2()
{
    Syscall::initialize();

    auto dev_zero = make<ZeroDevice>();
    auto dev_full = make<FullDevice>();
    auto dev_random = make<RandomDevice>();
    auto dev_ptmx = make<PTYMultiplexer>();
    auto dev_hd0 = IDEDiskDevice::create();
    auto dev_hd0p1 = DiskPartition::create(dev_hd0.copy_ref(), PARTITION_OFFSET);
    auto e2fs = Ext2FS::create(dev_hd0p1.copy_ref());
    e2fs->initialize();

    vfs->mount_root(e2fs.copy_ref());

    dbgprintf("Load ksyms\n");
    load_ksyms();
    dbgprintf("Loaded ksyms\n");

    vfs->mount(ProcFS::the(), "/proc");
    vfs->mount(DevPtsFS::the(), "/dev/pts");

    int error;

    auto* system_server_process = Process::create_user_process("/bin/SystemServer", (uid_t)100, (gid_t)100, (pid_t)0, error, { }, { }, tty0);
    if (error != 0) {
        dbgprintf("error spawning SystemServer: %d\n", error);
        hang();
    }
    system_server_process->set_priority(Process::HighPriority);

#ifdef STRESS_TEST_SPAWNING
    Process::create_kernel_process("spawn_stress", spawn_stress);
#endif

    current->process().sys$exit(0);
    ASSERT_NOT_REACHED();
}

extern "C" {
multiboot_info_t* multiboot_info_ptr;
}

extern "C" [[noreturn]] void init()
{
    kprintf("Kernel command line: '%s'\n", multiboot_info_ptr->cmdline);

    sse_init();

    kmalloc_init();
    init_ksyms();

    vfs = new VFS;
    dev_debuglog = new DebugLogDevice;

    auto console = make<Console>();

    RTC::initialize();
    PIC::initialize();
    gdt_init();
    idt_init();

    keyboard = new KeyboardDevice;
    ps2mouse = new PS2MouseDevice;
    dev_null = new NullDevice;

    VirtualConsole::initialize();
    tty0 = new VirtualConsole(0, VirtualConsole::AdoptCurrentVGABuffer);
    tty1 = new VirtualConsole(1);
    tty2 = new VirtualConsole(2);
    tty3 = new VirtualConsole(3);
    VirtualConsole::switch_to(0);

    kprintf("Starting Serenity Operating System...\n");

    MemoryManager::initialize();
    PIT::initialize();

    new BXVGADevice;

    auto e1000 = E1000NetworkAdapter::autodetect();

    Retained<ProcFS> new_procfs = ProcFS::create();
    new_procfs->initialize();

    auto devptsfs = DevPtsFS::create();
    devptsfs->initialize();

    Process::initialize();
    Thread::initialize();
    Process::create_kernel_process("init_stage2", init_stage2);
    Process::create_kernel_process("syncd", [] {
        for (;;) {
            Syscall::sync();
            current->sleep(1 * TICKS_PER_SECOND);
        }
    });
    Process::create_kernel_process("Finalizer", [] {
        g_finalizer = current;
        current->process().set_priority(Process::LowPriority);
        for (;;) {
            Thread::finalize_dying_threads();
            current->block(Thread::BlockedLurking);
            Scheduler::yield();
        }
    });
    Process::create_kernel_process("NetworkTask", NetworkTask_main);

    Scheduler::pick_next();

    sti();

    // This now becomes the idle process :^)
    for (;;) {
        asm("hlt");
    }
}
