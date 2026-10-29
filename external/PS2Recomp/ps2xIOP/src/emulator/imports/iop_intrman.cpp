#include "iop_intrman.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "../core/iop_cpu.h"
#include "../core/iop_memory.h"
#include "../services/iop_rpc.h"

namespace ps2x::iop::detail
{
    IopIntrman::IopIntrman(IopMemory &memory) noexcept
        : m_memory(memory)
    {
    }

    void IopIntrman::reset()
    {
        m_handlers.clear();
        m_enabled.clear();
    }

    namespace // DBZIW-DEBUG (issue 020): last interrupt enable/disable calls, printed by the overdue-DMA alarm
    {
        struct IntrEvent
        {
            uint16_t ordinal;
            uint32_t a0, ra, before, after, result;
        };
        std::array<IntrEvent, 64> g_intrEvents{};
        size_t g_intrEventCount = 0u;
        std::array<IntrEvent, 64> g_intrChanges{}; // only calls that changed the state
        size_t g_intrChangeCount = 0u;
    }

    void dumpIntrHistory()
    {
        const size_t count = std::min<size_t>(g_intrEventCount, g_intrEvents.size());
        for (size_t i = g_intrEventCount - count; i < g_intrEventCount; ++i)
        {
            const IntrEvent &e = g_intrEvents[i % g_intrEvents.size()];
            std::fprintf(stderr, "[intr-history] #%zu ordinal=%u a0=0x%x ra=0x%x intr %u->%u v0=0x%x\n", i, e.ordinal, e.a0,
                         e.ra, e.before, e.after, e.result);
        }
        const size_t changes = std::min<size_t>(g_intrChangeCount, g_intrChanges.size());
        for (size_t i = g_intrChangeCount - changes; i < g_intrChangeCount; ++i)
        {
            const IntrEvent &e = g_intrChanges[i % g_intrChanges.size()];
            std::fprintf(stderr, "[intr-change] #%zu ordinal=%u a0=0x%x ra=0x%x intr %u->%u v0=0x%x\n", i, e.ordinal, e.a0,
                         e.ra, e.before, e.after, e.result);
        }
    }

    bool IopIntrman::dispatchImport(uint16_t ordinal, IopCpuState &cpu, IopGuestExecutor &executor)
    {
        const uint32_t before = m_memory.interruptControl();
        const bool handled = dispatchImportImpl(ordinal, cpu, executor);
        if (ordinal == 8u || ordinal == 9u || ordinal == 17u || ordinal == 18u)
        {
            const IntrEvent event{ordinal, cpu.gpr[4], cpu.gpr[31], before, m_memory.interruptControl(), cpu.gpr[2]};
            g_intrEvents[g_intrEventCount++ % g_intrEvents.size()] = event;
            if (event.before != event.after)
                g_intrChanges[g_intrChangeCount++ % g_intrChanges.size()] = event;
        }
        return handled;
    }

    bool IopIntrman::dispatchImportImpl(uint16_t ordinal, IopCpuState &cpu, IopGuestExecutor &executor)
    {
        const uint32_t a0 = cpu.gpr[4];
        const uint32_t a1 = cpu.gpr[5];
        const uint32_t a2 = cpu.gpr[6];
        const uint32_t a3 = cpu.gpr[7];
        const auto setV0 = [&](uint32_t value)
        {
            cpu.gpr[2] = value;
        };

        switch (ordinal)
        {
        case 3:
            setV0(0u);
            return true;
        case 4: // RegisterIntrHandler
            m_handlers[static_cast<int>(a0)] = {a2, a3, cpu.gpr[28]};
            setV0(0u);
            return true;
        case 5: // ReleaseIntrHandler
            m_handlers.erase(static_cast<int>(a0));
            setV0(0u);
            return true;
        case 6: // EnableIntr
            m_enabled[static_cast<int>(a0)] = true;
            if (a0 < 32u)
                m_memory.setInterruptMask(m_memory.interruptMask() | (1u << a0));
            setV0(0u);
            return true;
        case 7: // DisableIntr
            if (a1 != 0u)
                m_memory.write32(a1, a0);
            if (a0 < 32u)
                m_memory.setInterruptMask(m_memory.interruptMask() & ~(1u << a0));
            m_enabled[static_cast<int>(a0)] = false;
            setV0(0u);
            return true;
        case 8: // CpuDisableIntr(void): no state pointer, unlike CpuSuspendIntr (#17)
        {
            const uint32_t prev = m_memory.interruptControl();
            m_memory.setInterruptControl(0u);
            setV0(prev != 0u ? 0u : static_cast<uint32_t>(-104));
            return true;
        }
        case 9: // CpuEnableIntr
            m_memory.setInterruptControl(1u);
            setV0(0u);
            return true;
        case 14:
            setV0(a0 != 0u
                      ? executor.executeGuestFunctionWithBudget(a0, a1, a2, a3, 0u, cpu.gpr[28], 100000u)
                      : 0u);
            return true;
        case 15:
        case 16:
        case 23:
        case 24:
        case 25:
        case 28:
        case 30:
            setV0(0u);
            return true;
        case 17:
        {
            const uint32_t prev = m_memory.interruptControl();
            if (a0 != 0u)
                m_memory.write32(a0, prev);
            m_memory.setInterruptControl(0u);
            setV0(prev != 0u ? 0u : static_cast<uint32_t>(-104));
            return true;
        }
        case 18:
            m_memory.setInterruptControl(a0 != 0u ? 1u : 0u);
            setV0(0u);
            return true;
        default:
            return false;
        }
    }

    bool IopIntrman::dispatchInterrupt(int irq, IopGuestExecutor &executor) const
    {
        const auto enabled = m_enabled.find(irq);
        if (enabled == m_enabled.end() || !enabled->second)
            return false;
        const auto handler = m_handlers.find(irq);
        if (handler == m_handlers.end() || handler->second.function == 0u)
            return false;

        (void)executor.executeGuestFunctionWithBudget(handler->second.function,
                                                      handler->second.argument,
                                                      0u,
                                                      0u,
                                                      0u,
                                                      handler->second.gp,
                                                      100000u);
        return true;
    }
}
