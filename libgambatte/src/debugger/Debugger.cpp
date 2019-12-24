#include "Debugger.h"
#include <vector>
#include "GdbConnection.h"
#include "GdbStub.h"
#include "cpu.h"


namespace gambatte {
namespace debugger {

std::vector<Debugger::RegisterLayout> Debugger::Registers = {
    {"a", RegisterLayout::RegisterType::integer, [](CPU* cpu){ return cpu->a_; }},
    {"b", RegisterLayout::RegisterType::integer, [](CPU* cpu){ return cpu->b; }},
    {"c", RegisterLayout::RegisterType::integer, [](CPU* cpu){ return cpu->c; }},
    {"d", RegisterLayout::RegisterType::integer, [](CPU* cpu){ return cpu->d; }},
    {"e", RegisterLayout::RegisterType::integer, [](CPU* cpu){ return cpu->e; }},
    {"sp", RegisterLayout::RegisterType::data_ptr, [](CPU* cpu){ return cpu->sp; }, 32}, // regnum 5
    {"pc", RegisterLayout::RegisterType::code_ptr, [](CPU* cpu){ return cpu->correct_pc; }, 32}, // regnum 6
    {"h", RegisterLayout::RegisterType::integer, [](CPU* cpu){ return cpu->h; }},
    {"l", RegisterLayout::RegisterType::integer, [](CPU* cpu){ return cpu->l; }}
};

Debugger::Debugger(CPU* cpu) : cpu(cpu)
{
    
}

static void* GdbRun(void* ctx)
{
    GdbStub* stub = (GdbStub*)ctx;
    stub->Run();
    LOG(WARNING) << "GDB stub has exited the run method!";
    return NULL;
}

void Debugger::StartGdbStub(std::string address, int port)
{
    if (gdb != nullptr) {
        LOG(ERROR) << "GDB stub is already running!";
        return;
    }
    
    gdb = new GdbStub(this, address, port);
    pthread_create(&gdb_thread, NULL, GdbRun, gdb);
}

void Debugger::AddBreakpoint(Breakpoint* bp)
{
    breakpoints[bp->address].push_back(bp);
}

void Debugger::RemoveBreakpoint(Breakpoint* bp)
{
    auto bps = breakpoints[bp->address];
    for (auto iter = bps.begin(); iter != bps.end(); iter++) {
        if (*iter == bp) {
            bps.erase(iter);
            break;
        }
    }
}

void Debugger::RemoveBreakpoints(long address)
{
    breakpoints[address].clear();
}

void Debugger::CheckForBreakpoints(long address)
{
    auto bps = breakpoints[address];
    bool suitable_bp = false;
    std::vector<Breakpoint*> to_remove;
    if (bps.size() > 0) {
        for (Breakpoint* bp : bps) {
            if (bp->enabled) {
                suitable_bp = true;
                if (bp->uses > 1) bp->uses--;
                else if (bp->uses == 1) to_remove.push_back(bp);
            }
        }
    }
    for (Breakpoint* bp : to_remove) {
        RemoveBreakpoint(bp);
    }
    if (std::get<0>(step_range) != -1) {
        if (!(std::get<0>(step_range) <= address && address <= std::get<1>(step_range))) {
            // Outside of step_range
            suitable_bp = true;
            step_range = std::make_tuple(-1, -1);
        }
    }
    if (suitable_bp) {
        Halt(new StopReason(StopReason::StopType::signal_extended, 5, "swbreak:;thread:p1.1;core:1;")); // 5: SIGTRAP
    }
}

void Debugger::Halt(StopReason* reason)
{
    is_halted = true;
    stop_reason = reason;
    if (gdb != nullptr) gdb->NotifyHalted(reason);
}

void Debugger::Unhalt()
{
    is_halted = false;
    stop_reason = nullptr;
    is_halted_cond.notify_all();
}

void Debugger::WaitWhileHalted()
{
    if (is_halted) {
        std::unique_lock<std::mutex> lk(is_halted_mutex);
        is_halted_cond.wait(lk, [this]{ return !this->is_halted; });
    }
}

void Debugger::EncodeRegisters(util::Buffer &buffer)
{
    for (Debugger::RegisterLayout layout : Debugger::Registers) {
        GdbConnection::Encode(layout.accessor(cpu), layout.bitsize / 8, buffer, false);
    }
}

void Debugger::EncodeMemory(util::Buffer &buffer, long address, size_t bytes)
{
    // Outside of gameboy memory!
    if (address > 0xffff) {
        return;
    }
    for (int i = 0; i < bytes; i++) {
        uint8_t byte = cpu->mem_.read(address+i, cpu->cycleCounter_);
        GdbConnection::Encode(byte, 1, buffer);
    }
}

void Debugger::WriteMemory(long address, size_t size, util::Buffer& buffer)
{
    // Outside of gameboy memory!
    if (address > 0xffff) {
        return;
    }
    std::vector<uint8_t> bytes;
    GdbConnection::Decode(bytes, buffer);
    for (int i = 0; i < size; i++) {
        uint8_t byte = 0;
        if (i < bytes.size()) byte = bytes.at(i);
        cpu->mem_.write(address+i, byte, cpu->cycleCounter_);
    }
}


}
}
