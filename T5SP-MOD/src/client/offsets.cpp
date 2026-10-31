#include "offsets.hpp"

namespace offsets {

// Verified command-buffer function in the supplied BO1 dump.
const uintptr_t Cbuf_AddText = 0x0049B930;

// Reverse-engineered candidates; guarded by runtime instruction checks.
const uintptr_t Cmd_AddCommand = 0x00661400;
const uintptr_t Dvar_FindVar = 0x005AE810;
const uintptr_t Dvar_LookupHelper = 0x00862280;

// Dvar lookup table (data address, not a callable function).
const uintptr_t Dvar_HashBuckets = 0x02620BF0;

} // namespace offsets
