#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "d3d9_imgui_hook.hpp"
#include "game_console.hpp"
#include "engine_bridge.hpp"
#include "offsets.hpp"

static HMODULE g_realD3D9 = nullptr;
static INIT_ONCE g_init = INIT_ONCE_STATIC_INIT;
static bool g_imgui_hooked = false;

static BOOL CALLBACK LoadRealD3D9(PINIT_ONCE, PVOID, PVOID*) {
    char system[MAX_PATH]{};
    if (!GetSystemDirectoryA(system, MAX_PATH)) return FALSE;
    std::string path = std::string(system) + "\\d3d9.dll";
    g_realD3D9 = LoadLibraryA(path.c_str());
    return g_realD3D9 != nullptr;
}

static FARPROC Real(const char* name) {
    InitOnceExecuteOnce(&g_init, LoadRealD3D9, nullptr, nullptr);
    if (!g_realD3D9) return nullptr;
    return GetProcAddress(g_realD3D9, name);
}

static void Log(const char* message) {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    std::printf("[%02u:%02u:%02u] [T5SP-MOD] %s\n",
        now.wHour, now.wMinute, now.wSecond, message);
    std::fflush(stdout);
}

static DWORD WINAPI ConsoleThread(void*) {
    AllocConsole();
    SetConsoleTitleA("T5SP-MOD | Black Ops 1");
    FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    freopen_s(&stream, "CONIN$", "r", stdin);
    Log("d3d9.dll proxy loaded (x86)");
    Log(g_realD3D9 ? "System d3d9.dll loaded" : "ERROR: system d3d9.dll unavailable");
    char address_text[96]{};
    std::snprintf(address_text, sizeof(address_text),
                  "BO1 Cbuf_AddText: 0x%08lX (dump-specific)",
                  static_cast<unsigned long>(offsets::Cbuf_AddText));
    Log(address_text);
    Log("Dev console only. Menu and fastfile modification disabled.");
    Log(g_imgui_hooked ? "ImGui hooks installed" : "ERROR: ImGui hooks failed");
    Log("Type help, status, an engine command, or exit (does not close game).");

    char input[1024];
    while (true) {
        std::printf("T5SP> ");
        std::fflush(stdout);
        if (!std::fgets(input, sizeof(input), stdin)) break;
        input[std::strcspn(input, "\r\n")] = '\0';
        if (!std::strcmp(input, "exit")) break;
        if (!std::strcmp(input, "help")) {
            Log("Commands: help | status | exit | BO1 engine command");
        } else if (!std::strcmp(input, "status")) {
            Log(g_realD3D9 ? "D3D9 forwarding: ready" : "D3D9 forwarding: unavailable");
            Log(address_text);
            Log(game_console::engine_command_address_valid()
                ? "Engine signature verified" : "Engine signature mismatch: commands disabled");
            Log(engine_bridge::native_command_registered()
                ? "t5sp_status: registered" : "t5sp_status: not registered");
            Log(engine_bridge::dynamic_dvars_available()
                ? "Dvar autocomplete: live registry" : "Dvar autocomplete: static fallback");
        } else if (*input) {
            Log(game_console::queue_game_command(input)
                ? "Game command queued" : "Game command rejected (wrong EXE or too long)");
        }
    }
    Log("Console input stopped. Game remains running.");
    return 0;
}

extern "C" __declspec(dllexport) IDirect3D9* WINAPI Direct3DCreate9(UINT sdk) {
    using Fn = IDirect3D9* (WINAPI*)(UINT);
    auto fn = reinterpret_cast<Fn>(Real("Direct3DCreate9"));
    return fn ? fn(sdk) : nullptr;
}

extern "C" __declspec(dllexport) HRESULT WINAPI Direct3DCreate9Ex(UINT sdk, IDirect3D9Ex** out) {
    using Fn = HRESULT (WINAPI*)(UINT, IDirect3D9Ex**);
    auto fn = reinterpret_cast<Fn>(Real("Direct3DCreate9Ex"));
    return fn ? fn(sdk, out) : E_FAIL;
}

#define FORWARD_VOID(name, args, callargs) \
extern "C" __declspec(dllexport) void WINAPI name args { \
    using Fn = void (WINAPI*) args; \
    auto fn = reinterpret_cast<Fn>(Real(#name)); \
    if (fn) fn callargs; \
}
#define FORWARD_INT(name, args, callargs) \
extern "C" __declspec(dllexport) int WINAPI name args { \
    using Fn = int (WINAPI*) args; \
    auto fn = reinterpret_cast<Fn>(Real(#name)); \
    return fn ? fn callargs : 0; \
}

FORWARD_INT(D3DPERF_BeginEvent, (D3DCOLOR c, LPCWSTR n), (c, n))
FORWARD_INT(D3DPERF_EndEvent, (), ())
FORWARD_VOID(D3DPERF_SetMarker, (D3DCOLOR c, LPCWSTR n), (c, n))
FORWARD_VOID(D3DPERF_SetRegion, (D3DCOLOR c, LPCWSTR n), (c, n))
FORWARD_VOID(D3DPERF_SetOptions, (DWORD o), (o))
extern "C" __declspec(dllexport) DWORD WINAPI D3DPERF_GetStatus() {
    using Fn = DWORD (WINAPI*)();
    auto fn = reinterpret_cast<Fn>(Real("D3DPERF_GetStatus"));
    return fn ? fn() : 0;
}

extern "C" __declspec(dllexport) BOOL WINAPI D3DPERF_QueryRepeatFrame() {
    using Fn = BOOL (WINAPI*)();
    auto fn = reinterpret_cast<Fn>(Real("D3DPERF_QueryRepeatFrame"));
    return fn ? fn() : FALSE;
}

extern "C" __declspec(dllexport) void WINAPI DebugSetMute() {
    using Fn = void (WINAPI*)();
    auto fn = reinterpret_cast<Fn>(Real("DebugSetMute"));
    if (fn) fn();
}

static DWORD WINAPI StartThread(void*) {
    Real("Direct3DCreate9");
    g_imgui_hooked = install_d3d9_imgui_hook();
    ConsoleThread(nullptr);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, StartThread, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
