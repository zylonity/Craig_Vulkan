#include "memory_allocator.h"

// Valve Corporation, Apache-2.0 — https://github.com/ValveSoftware/steam-audio/tree/v4.8.1
// standalone adaptation of context.cpp: DSP uses the default allocator, not the simulator's global context
namespace ipl {
    Memory& gMemory() {
        static Memory memory;
        return memory;
    }
}
