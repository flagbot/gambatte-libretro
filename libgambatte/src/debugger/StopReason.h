#pragma once

#include "Buffer.h"

#include <string.h>

namespace gambatte {
namespace debugger {

/// Class used to represent the reason execution just stopped.
class StopReason {
public:
    enum class StopType {
        signal,
        signal_extended,
        process_exited,
        process_terminated,
        output
    };
    
    StopReason(StopType type = StopType::output, int code = 0, std::string additional = "");
    
    /// Class Methods
    static char Letter(StopType type);
    
    /// Member Methods
    void Encode(util::Buffer& buffer);
    std::string Encode();
    
    /// Methods for setting additional correctly.
    StopReason* WithOutput(std::string output);
    
    StopType type;
    int code;
    std::string additional;
    
    static StopReason* BPReason;
};

}
}
