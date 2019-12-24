#include "StopReason.h"
#include "GdbConnection.h"

namespace gambatte {
namespace debugger {

StopReason* StopReason::BPReason = new StopReason(StopReason::StopType::signal_extended, 5, "swbreak:;thread:p1.1;core:1;");

StopReason::StopReason(StopType type, int code, std::string additional) : type(type), code(code), additional(additional)
{
    
}

char StopReason::Letter(StopType type)
{
    switch (type) {
        case StopType::signal:
            return 'S';
        case StopType::signal_extended:
            return 'T';
        case StopType::process_exited:
            return 'W';
        case StopType::process_terminated:
            return 'X';
        case StopType::output:
            return 'O';
    }
    
    return 'U';
}

void StopReason::Encode(util::Buffer &buffer)
{
    buffer.Write(StopReason::Letter(type));
    if (type != StopType::output) {
        // We have a code to write.
        GdbConnection::Encode(code, 1, buffer);
    }
    buffer.Write(additional);
}

std::string StopReason::Encode()
{
    util::Buffer ret;
    Encode(ret);
    return ret.GetString();
}

StopReason* StopReason::WithOutput(std::string output)
{
    if (type != StopType::output) {
        LOG(ERROR) << "Trying to set output for non output stop type";
        return this;
    }
    
    additional = output;
    
    return this;
}

}
}
