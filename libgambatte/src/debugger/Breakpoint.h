#pragma once

namespace gambatte {
namespace debugger {

/// Class used to represent the internal debugger state.
class Breakpoint {
    
public:
    Breakpoint(long address, long uses = -1);
    
    long address;
    long uses;
    bool enabled = true;

private:

};

}
}


