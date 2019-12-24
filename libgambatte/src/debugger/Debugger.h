#pragma once

#include "Buffer.h"
#include "StopReason.h"
#include "Breakpoint.h"

#include <string.h>
#include <pthread.h>
#include <unordered_map>
#include <vector>
#include <condition_variable>
#include <mutex>

namespace gambatte {

class CPU;

namespace debugger {

class GdbStub;

/// Class used to represent the internal debugger state.
class Debugger {
    
public:
    Debugger(CPU* cpu);
    
    struct RegisterLayout {
        enum class RegisterType {
            integer,
            data_ptr,
            code_ptr
        };
        
        std::string name;
        RegisterType type;
        std::function<uint64_t(CPU*)> accessor;
        size_t bitsize = 8;
    };
    
#pragma mark Methods
    
    /// Start the gdb stub in a separate thread.
    void StartGdbStub(std::string address = "0.0.0.0", int port = 55555);
    
    void AddBreakpoint(Breakpoint* bp);
    void RemoveBreakpoint(Breakpoint* bp);
    void RemoveBreakpoints(long address);
    
    void CheckForBreakpoints(long address);
    
    void Halt(StopReason* reason);
    void Unhalt();
        
    void WaitWhileHalted();
    
    void EncodeRegisters(util::Buffer& buffer);
    void EncodeMemory(util::Buffer& buffer, long address, size_t bytes);
    
    void WriteMemory(long address, size_t size, util::Buffer& buffer);
    
#pragma mark Properties
    StopReason* stop_reason = nullptr;
    CPU* cpu = nullptr;
    GdbStub* gdb = nullptr;
    
    bool is_halted = false;
    
    std::unordered_map<long, std::vector<Breakpoint*>> breakpoints;
    
    static std::vector<RegisterLayout> Registers;
    
    std::tuple<long, long> step_range = std::make_tuple(-1, -1);

private:
    pthread_t gdb_thread;
    
    /// Halt Mutex & condition
    std::mutex is_halted_mutex;
    std::condition_variable is_halted_cond;
};

}
}

