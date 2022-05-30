/*
 * Copyright (c) 2021, Nico Weber <thakis@chromium.org>
 * Copyright (c) 2021, Marcin Undak <mcinek@gmail.com>
 * Copyright (c) 2021, Jesse Buhagiar <jooster669@gmail.com>
 * Copyright (c) 2022, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Format.h>
#include <AK/Types.h>

#include <Kernel/Arch/Processor.h>
#include <Kernel/Arch/aarch64/BootPPMParser.h>
#include <Kernel/Arch/aarch64/CPU.h>
#include <Kernel/Arch/aarch64/RPi/Framebuffer.h>
#include <Kernel/Arch/aarch64/RPi/Mailbox.h>
#include <Kernel/Arch/aarch64/RPi/Timer.h>
#include <Kernel/Arch/aarch64/RPi/UART.h>
#include <Kernel/Arch/aarch64/Registers.h>
#include <Kernel/KSyms.h>
#include <Kernel/Panic.h>

struct TrapFrame {
    u64 x[31];     // Saved general purpose registers
    u64 spsr_el1;  // Save Processor Status Register, EL1
    u64 elr_el1;   // Exception Link Reigster, EL1
    u64 tpidr_el1; // EL0 thread ID
    u64 sp_el0;    // EL0 stack pointer
};

extern "C" void exception_common(TrapFrame const* const trap_frame);
extern "C" void exception_common(TrapFrame const* const trap_frame)
{
    constexpr bool print_stack_frame = true;

    if constexpr (print_stack_frame) {
        dbgln("Exception Generated by processor!");

        for (auto reg = 0; reg < 31; reg++) {
            dbgln("x{}: {:x}", reg, trap_frame->x[reg]);
        }

        // Special registers
        dbgln("spsr_el1: {:x}", trap_frame->spsr_el1);
        dbgln("elr_el1: {:x}", trap_frame->elr_el1);
        dbgln("tpidr_el1: {:x}", trap_frame->tpidr_el1);
        dbgln("sp_el0: {:x}", trap_frame->sp_el0);

        auto esr_el1 = Kernel::Aarch64::ESR_EL1::read();
        dbgln("esr_el1: EC({:#b}) IL({:#b}) ISS({:#b}) ISS2({:#b})", esr_el1.EC, esr_el1.IL, esr_el1.ISS, esr_el1.ISS2);
    }

    Kernel::Processor::halt();
}

typedef void (*ctor_func_t)();
extern ctor_func_t start_heap_ctors[];
extern ctor_func_t end_heap_ctors[];
extern ctor_func_t start_ctors[];
extern ctor_func_t end_ctors[];

// FIXME: Share this with the Intel Prekernel.
extern size_t __stack_chk_guard;
size_t __stack_chk_guard;
extern "C" [[noreturn]] void __stack_chk_fail();

void __stack_chk_fail()
{
    Kernel::Processor::halt();
}

namespace Kernel {

static void draw_logo();
static u32 query_firmware_version();

extern "C" [[noreturn]] void halt();
extern "C" [[noreturn]] void init();

ALWAYS_INLINE static aarch64Processor& bootstrap_processor()
{
    alignas(aarch64Processor) static u8 bootstrap_processor_storage[sizeof(aarch64Processor)];
    return (aarch64Processor&)bootstrap_processor_storage;
}

extern "C" [[noreturn]] void init()
{
    dbgln("Welcome to Serenity OS!");
    dbgln("Imagine this being your ideal operating system.");
    dbgln("Observed deviations from that ideal are shortcomings of your imagination.");
    dbgln();

    new (&bootstrap_processor()) aarch64Processor();
    bootstrap_processor().initialize(0);

    // We call the constructors of kmalloc.cpp separately, because other constructors in the Kernel
    // might rely on being able to call new/kmalloc in the constructor. We do have to run the
    // kmalloc constructors, because kmalloc_init relies on that.
    for (ctor_func_t* ctor = start_heap_ctors; ctor < end_heap_ctors; ctor++)
        (*ctor)();
    kmalloc_init();

    for (ctor_func_t* ctor = start_ctors; ctor < end_ctors; ctor++)
        (*ctor)();

    load_kernel_symbol_table();

    auto firmware_version = query_firmware_version();
    dbgln("Firmware version: {}", firmware_version);

    dbgln("Initialize MMU");
    init_prekernel_page_tables();

    auto& framebuffer = Framebuffer::the();
    if (framebuffer.initialized()) {
        draw_logo();
    }

    dbgln("Enter loop");

    auto& timer = Timer::the();
    u64 start_musec = 0;
    for (;;) {
        u64 now_musec;
        while ((now_musec = timer.microseconds_since_boot()) - start_musec < 1'000'000)
            ;
        start_musec = now_musec;
        dbgln("Timer: {}", now_musec);
    }
}

class QueryFirmwareVersionMboxMessage : Mailbox::Message {
public:
    u32 version;

    QueryFirmwareVersionMboxMessage()
        : Mailbox::Message(0x0000'0001, 4)
    {
        version = 0;
    }
};

static u32 query_firmware_version()
{
    struct __attribute__((aligned(16))) {
        Mailbox::MessageHeader header;
        QueryFirmwareVersionMboxMessage query_firmware_version;
        Mailbox::MessageTail tail;
    } message_queue;

    if (!Mailbox::the().send_queue(&message_queue, sizeof(message_queue))) {
        return 0xffff'ffff;
    }

    return message_queue.query_firmware_version.version;
}

extern "C" const u32 serenity_boot_logo_start;
extern "C" const u32 serenity_boot_logo_size;

static void draw_logo()
{
    BootPPMParser logo_parser(reinterpret_cast<u8 const*>(&serenity_boot_logo_start), serenity_boot_logo_size);
    if (!logo_parser.parse()) {
        dbgln("Failed to parse boot logo.");
        return;
    }

    dbgln("Boot logo size: {} ({} x {})", serenity_boot_logo_size, logo_parser.image.width, logo_parser.image.height);

    auto& framebuffer = Framebuffer::the();
    auto fb_ptr = framebuffer.gpu_buffer();
    auto image_left = (framebuffer.width() - logo_parser.image.width) / 2;
    auto image_right = image_left + logo_parser.image.width;
    auto image_top = (framebuffer.height() - logo_parser.image.height) / 2;
    auto image_bottom = image_top + logo_parser.image.height;
    auto logo_pixels = logo_parser.image.pixel_data;

    for (u32 y = 0; y < framebuffer.height(); y++) {
        for (u32 x = 0; x < framebuffer.width(); x++) {
            if (x >= image_left && x < image_right && y >= image_top && y < image_bottom) {
                switch (framebuffer.pixel_order()) {
                case Framebuffer::PixelOrder::RGB:
                    fb_ptr[0] = logo_pixels[0];
                    fb_ptr[1] = logo_pixels[1];
                    fb_ptr[2] = logo_pixels[2];
                    break;
                case Framebuffer::PixelOrder::BGR:
                    fb_ptr[0] = logo_pixels[2];
                    fb_ptr[1] = logo_pixels[1];
                    fb_ptr[2] = logo_pixels[0];
                    break;
                default:
                    dbgln("Unsupported pixel format");
                    VERIFY_NOT_REACHED();
                }

                logo_pixels += 3;
            } else {
                fb_ptr[0] = 0xBD;
                fb_ptr[1] = 0xBD;
                fb_ptr[2] = 0xBD;
            }

            fb_ptr[3] = 0xFF;
            fb_ptr += 4;
        }
        fb_ptr += framebuffer.pitch() - framebuffer.width() * 4;
    }
}

}
