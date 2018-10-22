#include "types.h"
#include "VGA.h"
#include "kmalloc.h"
#include "i386.h"
#include "i8253.h"
#include "Keyboard.h"
#include "Task.h"
#include "IPC.h"
#include "system.h"
#include "Disk.h"
#include "PIC.h"
#include "StdLib.h"
#include "Syscall.h"
#include "CMOS.h"
#include "Userspace.h"
#include "IDEDiskDevice.h"
#include <VirtualFileSystem/NullDevice.h>
#include <VirtualFileSystem/ZeroDevice.h>
#include <VirtualFileSystem/FullDevice.h>
#include <VirtualFileSystem/RandomDevice.h>
#include <VirtualFileSystem/Ext2FileSystem.h>
#include <VirtualFileSystem/VirtualFileSystem.h>
#include <VirtualFileSystem/FileHandle.h>
#include <AK/OwnPtr.h>
#include "MemoryManager.h"
#include <ELFLoader/ELFLoader.h>
#include "Console.h"

#define TEST_VFS
#define TEST_ELF_LOADER
#define TEST_CRASHY_USER_PROCESSES

#if 0
/* Keyboard LED disco task ;^) */

static void led_disco() NORETURN;

static void led_disco()
{
    BYTE b = 0;
    for (;;) {
        sleep(0.5 * TICKS_PER_SECOND);
        Keyboard::unsetLED((Keyboard::LED)b++);
        b &= 7;
        Keyboard::setLED((Keyboard::LED)b);
    }
}
#endif

static void motd_main() NORETURN;
static void motd_main()
{
    kprintf("Hello in motd_main!\n");
    int fd = Userspace::open("/test.asm");
    kprintf("motd: fd=%d\n", fd);
    ASSERT(fd != -1);
    DO_SYSCALL_A3(0x2000, 1, 2, 3);
    kprintf("getuid(): %u\n", Userspace::getuid());
    auto buffer = DataBuffer::createUninitialized(33);
    memset(buffer->data(), 0, buffer->length());
    int nread = Userspace::read(fd, buffer->data(), buffer->length() - 1);
    kprintf("read(): %d\n", nread);
    buffer->data()[nread] = 0;
    kprintf("read(): '%s'\n", buffer->data());
    for (;;) {
        //kill(4, 5);
        sleep(1 * TICKS_PER_SECOND);
    }
}

static void syscall_test_main() NORETURN;
static void syscall_test_main()
{
    kprintf("Hello in syscall_test_main!\n");
    for (;;) {
        Userspace::getuid();
//        Userspace::yield();
        //kprintf("getuid(): %u\n", Userspace::getuid());
        sleep(1 * TICKS_PER_SECOND);
    }
}

static void user_main() NORETURN;
static void user_main()
{
    DO_SYSCALL_A3(0x3000, 2, 3, 4);
    // Crash ourselves!
    char* x = reinterpret_cast<char*>(0xbeefbabe);
    *x = 1;
    HANG;
    for (;;) {
        // nothing?
        Userspace::sleep(1 * TICKS_PER_SECOND);
    }
}

system_t system;

void banner()
{
    kprintf("\n");
    vga_set_attr(0x0a);
    kprintf(" _____         _           _   \n");
    vga_set_attr(0x0b);
    kprintf("|   __|___ ___| |_ ___ ___| |_ \n");
    vga_set_attr(0x0c);
    kprintf("|  |  | -_|  _| . | -_|  _|  _|\n");
    vga_set_attr(0x0d);
    kprintf("|_____|___|_| |___|___|_| |_|  \n");
    vga_set_attr(0x07);
    kprintf("\n");
}

static void init_stage2() NORETURN;
static void init_stage2()
{
    kprintf("init stage2...\n");
    Keyboard::initialize();

    // Anything that registers interrupts goes *after* PIC and IDT for obvious reasons.
    Syscall::initialize();

    VirtualFileSystem::initializeGlobals();

    extern void panel_main();

    new Task(panel_main, "panel", IPC::Handle::PanelTask, Task::Ring0);

    //new Task(led_disco, "led-disco", IPC::Handle::Any, Task::Ring0);

    Disk::initialize();

#ifdef TEST_VFS
    auto vfs = make<VirtualFileSystem>();

    auto dev_zero = make<ZeroDevice>();
    vfs->registerCharacterDevice(1, 3, *dev_zero);

    auto dev_null = make<NullDevice>();
    vfs->registerCharacterDevice(1, 5, *dev_zero);

    auto dev_full = make<FullDevice>();
    vfs->registerCharacterDevice(1, 7, *dev_full);

    auto dev_random = make<RandomDevice>();
    vfs->registerCharacterDevice(1, 8, *dev_random);

    auto dev_hd0 = IDEDiskDevice::create();
    auto e2fs = Ext2FileSystem::create(dev_hd0.copyRef());
    e2fs->initialize();

    vfs->mountRoot(e2fs.copyRef());

    //vfs->listDirectory("/");

    {
        auto motdFile = vfs->open("/motd.txt");
        ASSERT(motdFile);
        auto motdData = motdFile->readEntireFile();

        for (unsigned i = 0; i < motdData.size(); ++i) {
            kprintf("%c", motdData[i]);
        }
    }
#endif

#ifdef TEST_CRASHY_USER_PROCESSES
    new Task(user_main, "user", IPC::Handle::UserTask, Task::Ring3);
#endif

#ifdef TEST_ELF_LOADER
    {
        auto testExecutable = vfs->open("/_hello.o");
        ASSERT(testExecutable);
        auto testExecutableData = testExecutable->readEntireFile();
        ASSERT(testExecutableData);

        ExecSpace space;
        space.loadELF(move(testExecutableData));
        auto* elf_entry = space.symbolPtr("elf_entry");
        ASSERT(elf_entry);

        typedef int (*MainFunctionPtr)(void);
        kprintf("elf_entry: %p\n", elf_entry);
        int rc = reinterpret_cast<MainFunctionPtr>(elf_entry)();
        kprintf("it returned %d\n", rc);
    }
#endif

    //new Task(motd_main, "motd", IPC::Handle::MotdTask, Task::Ring0);
    //new Task(syscall_test_main, "syscall_test", IPC::Handle::MotdTask, Task::Ring3);

    kprintf("init stage2 is done!\n");

    for (;;) {
        asm("hlt");
    }
}

void init()
{
    cli();

    kmalloc_init();
    vga_init();

    auto console = make<Console>();

    PIC::initialize();
    gdt_init();
    idt_init();

    MemoryManager::initialize();

    PIT::initialize();

    memset(&system, 0, sizeof(system));
    WORD base_memory = (CMOS::read(0x16) << 8) | CMOS::read(0x15);
    WORD ext_memory = (CMOS::read(0x18) << 8) | CMOS::read(0x17);

    kprintf("%u kB base memory\n", base_memory);
    kprintf("%u kB extended memory\n", ext_memory);

    Task::initialize();

    auto* init2 = new Task(init_stage2, "init", IPC::Handle::InitTask, Task::Ring0);
    scheduleNewTask();

    sti();

    // This now becomes the idle task :^)
    for (;;) {
        asm("hlt");
    }
}

