/*
 * Copyright (c) 2021, Pankaj R <pankydev8@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/NonnullRefPtrVector.h>
#include <AK/OwnPtr.h>
#include <AK/RefCounted.h>
#include <AK/RefPtr.h>
#include <AK/Types.h>
#include <Kernel/Bus/PCI/Device.h>
#include <Kernel/Interrupts/IRQHandler.h>
#include <Kernel/Locking/Spinlock.h>
#include <Kernel/Memory/MemoryManager.h>
#include <Kernel/Memory/TypedMapping.h>
#include <Kernel/Storage/NVMe/NVMeDefinitions.h>

namespace Kernel {

struct DoorbellRegister {
    u32 sq_tail;
    u32 cq_head;
};

class AsyncBlockDeviceRequest;
class NVMeQueue : public IRQHandler
    , public RefCounted<NVMeQueue> {
public:
    static ErrorOr<NonnullRefPtr<NVMeQueue>> try_create(u16 qid, u8 irq, u32 q_depth, OwnPtr<Memory::Region> cq_dma_region, NonnullRefPtrVector<Memory::PhysicalPage> cq_dma_page, OwnPtr<Memory::Region> sq_dma_region, NonnullRefPtrVector<Memory::PhysicalPage> sq_dma_page, Memory::TypedMapping<volatile DoorbellRegister> db_regs);
    ErrorOr<void> create();
    explicit NVMeQueue(u16 qid, u8 irq, u32 q_depth, OwnPtr<Memory::Region> cq_dma_region, NonnullRefPtrVector<Memory::PhysicalPage> cq_dma_page, OwnPtr<Memory::Region> sq_dma_region, NonnullRefPtrVector<Memory::PhysicalPage> sq_dma_page, Memory::TypedMapping<volatile DoorbellRegister> db_regs);
    bool is_admin_queue() { return m_admin_queue; };
    bool handle_irq(const RegisterState&) override;
    void submit_sqe(struct NVMeSubmission&);
    u16 submit_sync_sqe(struct NVMeSubmission&);
    void read(AsyncBlockDeviceRequest& request, u16 nsid, u64 index, u32 count);
    void write(AsyncBlockDeviceRequest& request, u16 nsid, u64 index, u32 count);
    void enable_interrupts() { enable_irq(); };
    void disable_interrupts() { disable_irq(); };

private:
    bool cqe_available();
    void update_cqe_head();
    void complete_current_request(u16 status);
    void update_cq_doorbell()
    {
        m_db_regs->cq_head = m_cq_head;
    }

    void update_sq_doorbell()
    {
        m_db_regs->sq_tail = m_sq_tail;
    }

private:
    u16 m_qid {};
    u8 m_cq_valid_phase { 1 };
    u16 m_sq_tail {};
    u16 m_prev_sq_tail {};
    u16 m_cq_head {};
    bool m_admin_queue { false };
    u8 m_irq {};
    u32 m_qdepth {};
    Spinlock m_cq_lock { LockRank::Interrupts };
    Spinlock m_sq_lock { LockRank::Interrupts };
    OwnPtr<Memory::Region> m_cq_dma_region;
    NonnullRefPtrVector<Memory::PhysicalPage> m_cq_dma_page;
    Span<NVMeSubmission> m_sqe_array;
    OwnPtr<Memory::Region> m_sq_dma_region;
    NonnullRefPtrVector<Memory::PhysicalPage> m_sq_dma_page;
    Span<NVMeCompletion> m_cqe_array;
    OwnPtr<Memory::Region> m_rw_dma_region;
    Memory::TypedMapping<volatile DoorbellRegister> m_db_regs;
    RefPtr<Memory::PhysicalPage> m_rw_dma_page;
    Spinlock m_request_lock;
    RefPtr<AsyncBlockDeviceRequest> m_current_request;
};
}
