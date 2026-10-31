#pragma once

#include <cstdint>

// Addresses resolved from the supplied Black Ops 1 (T5) SP executable dump.
// These are absolute 32-bit virtual addresses, not relative RVAs.
namespace offsets {

extern const uintptr_t Cbuf_AddText;
extern const uintptr_t Cmd_AddCommand;
extern const uintptr_t Dvar_FindVar;
extern const uintptr_t Dvar_LookupHelper;
extern const uintptr_t Dvar_HashBuckets;

} // namespace offsets
