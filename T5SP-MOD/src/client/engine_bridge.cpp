#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine_bridge.hpp"
#include "game_console.hpp"
#include "offsets.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace engine_bridge {
namespace {

using CmdCallback = void(__cdecl*)();
using AddCommandFn = void(__cdecl*)(const char*, CmdCallback, void*);
using FindDvarFn = void*(__cdecl*)(const char*);

bool command_registered = false;
bool command_attempted = false;
bool dvar_supported = false;
std::array<uint32_t, 6> command_node{}; // BO1 cmd_function_t, 0x18 bytes.
char command_name[] = "t5sp_status";
std::mutex dvar_mutex;
std::vector<std::string> dvar_names;
DWORD last_refresh = 0;

bool readable(const void* ptr, size_t bytes) {
    if (!ptr || !bytes) return false;
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(ptr, &region, sizeof(region)) || region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    const auto first = reinterpret_cast<uintptr_t>(ptr);
    const auto base = reinterpret_cast<uintptr_t>(region.BaseAddress);
    return first >= base && bytes <= region.RegionSize - (first - base);
}

bool signature(uintptr_t address, std::initializer_list<unsigned char> bytes) {
    if (!readable(reinterpret_cast<const void*>(address), bytes.size())) return false;
    const auto* ptr = reinterpret_cast<const unsigned char*>(address);
    return std::equal(bytes.begin(), bytes.end(), ptr);
}

bool compatible() {
    return game_console::engine_command_address_valid();
}

bool safe_name(const char* ptr, std::string& result) {
    if (!readable(ptr, 1)) return false;
    MEMORY_BASIC_INFORMATION region{};
    VirtualQuery(ptr, &region, sizeof(region));
    const auto limit = reinterpret_cast<uintptr_t>(region.BaseAddress) + region.RegionSize;
    result.clear();
    for (size_t i = 0; i < 96 && reinterpret_cast<uintptr_t>(ptr + i) < limit; ++i) {
        const unsigned char c = static_cast<unsigned char>(ptr[i]);
        if (!c) return !result.empty();
        if (c < 33 || c > 126) return false;
        result.push_back(static_cast<char>(c));
    }
    return false;
}

void __cdecl status_command() {
    if (!compatible()) return;
    size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(dvar_mutex);
        count = dvar_names.size();
    }
    const std::string line = "T5SP-MOD V1.0 | dvars=" + std::to_string(count);
    game_console::enqueue_status_line(line);
    std::printf("[T5SP-MOD] %s\n", line.c_str());
    std::fflush(stdout);
}

} // namespace

void initialize_commands_on_game_thread() {
    if (command_attempted) return;
    command_attempted = true;
    if (!compatible() || !signature(offsets::Cmd_AddCommand,
                                   {0x56, 0x8B, 0x74, 0x24, 0x08, 0x56, 0xE8})) return;
    auto add = reinterpret_cast<AddCommandFn>(offsets::Cmd_AddCommand);
    add(command_name, &status_command, command_node.data());
    command_registered = true;
}

void refresh_dvars_on_game_thread() {
    const DWORD now = GetTickCount();
    if (now - last_refresh < 2000) return;
    last_refresh = now;

    if (!compatible() ||
        !signature(offsets::Dvar_FindVar, {0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0}) ||
        !signature(offsets::Dvar_LookupHelper, {0xB8, 0xE0, 0xCB, 0x61, 0x02}) ||
        !readable(reinterpret_cast<const void*>(offsets::Dvar_HashBuckets), 1024 * 4)) {
        return;
    }

    auto find = reinterpret_cast<FindDvarFn>(offsets::Dvar_FindVar);
    auto* buckets = reinterpret_cast<const uintptr_t*>(offsets::Dvar_HashBuckets);
    std::vector<std::string> discovered;
    discovered.reserve(4096);

    for (size_t bucket = 0; bucket < 1024 && discovered.size() < 32000; ++bucket) {
        uintptr_t current = buckets[bucket];
        for (size_t chain = 0; current && chain < 512; ++chain) {
            auto* entry = reinterpret_cast<const unsigned char*>(current);
            if (!readable(entry, 0x6C)) break;

            const char* name = *reinterpret_cast<const char* const*>(entry);
            std::string value;
            if (safe_name(name, value) && find(value.c_str()) == entry) {
                discovered.push_back(std::move(value));
            }
            const uintptr_t next = *reinterpret_cast<const uintptr_t*>(entry + 0x68);
            if (next == current) break;
            current = next;
        }
    }

    std::sort(discovered.begin(), discovered.end());
    discovered.erase(std::unique(discovered.begin(), discovered.end()), discovered.end());
    {
        std::lock_guard<std::mutex> lock(dvar_mutex);
        dvar_names.swap(discovered);
        dvar_supported = !dvar_names.empty();
    }
}

void get_dvar_names(std::vector<std::string>& names) {
    std::lock_guard<std::mutex> lock(dvar_mutex);
    names = dvar_names;
}

bool native_command_registered() { return command_registered; }
bool dynamic_dvars_available() { return dvar_supported; }

} // namespace engine_bridge
