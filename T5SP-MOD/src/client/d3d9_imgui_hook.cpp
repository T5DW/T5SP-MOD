#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <imgui.h>
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>
#include <MinHook.h>

#include "d3d9_imgui_hook.hpp"
#include "game_console.hpp"

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

namespace {

using EndSceneFn = HRESULT(WINAPI*)(IDirect3DDevice9*);
using ResetFn = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

EndSceneFn original_end_scene = nullptr;
ResetFn original_reset = nullptr;
HWND game_window = nullptr;
WNDPROC original_wndproc = nullptr;
bool imgui_ready = false;

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (imgui_ready && game_console::is_open()) {
        ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam);

        // Block game inputs while the overlay is open.
        if ((message >= WM_KEYFIRST && message <= WM_KEYLAST) ||
            (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST)) {
            if (message == WM_KEYDOWN && wparam == VK_OEM_3) {
                game_console::console_key_event(0, VK_OEM_3, 1);
            }
            return 0;
        }
    }

    if (message == WM_KEYDOWN && wparam == VK_OEM_3) {
        game_console::console_key_event(0, VK_OEM_3, 1);
        return 0;
    }

    return CallWindowProcA(original_wndproc, window, message, wparam, lparam);
}

HRESULT WINAPI HookedReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) {
    if (imgui_ready) ImGui_ImplDX9_InvalidateDeviceObjects();
    const HRESULT result = original_reset(device, params);
    if (imgui_ready && SUCCEEDED(result)) ImGui_ImplDX9_CreateDeviceObjects();
    return result;
}

HRESULT WINAPI HookedEndScene(IDirect3DDevice9* device) {
    if (!imgui_ready) {
        D3DDEVICE_CREATION_PARAMETERS creation{};
        if (SUCCEEDED(device->GetCreationParameters(&creation))) {
            game_window = creation.hFocusWindow;
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGui::StyleColorsDark();
            ImGui_ImplWin32_Init(game_window);
            ImGui_ImplDX9_Init(device);
            game_console::initialize();
            original_wndproc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrA(game_window, GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(&WindowProc)));
            imgui_ready = true;
        }
    }

    if (imgui_ready) {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        game_console::begin_frame();
        game_console::draw();
        ImGui::EndFrame();
        ImGui::Render();

        DWORD color_write = 0;
        device->GetRenderState(D3DRS_COLORWRITEENABLE, &color_write);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xFFFFFFFF);
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        device->SetRenderState(D3DRS_COLORWRITEENABLE, color_write);

        // Cbuf_AddText manages its own command-buffer lock; issue queued
        // commands after ImGui callbacks, not within WindowProc.
        game_console::process_game_commands();
    }

    return original_end_scene(device);
}

} // namespace

bool install_d3d9_imgui_hook() {
    HMODULE d3d9 = GetModuleHandleA("d3d9.dll");
    if (!d3d9) return false;

    using Direct3DCreate9Fn = IDirect3D9*(WINAPI*)(UINT);
    auto create = reinterpret_cast<Direct3DCreate9Fn>(
        GetProcAddress(d3d9, "Direct3DCreate9"));
    if (!create) return false;

    IDirect3D9* d3d = create(D3D_SDK_VERSION);
    if (!d3d) return false;

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "T5SPMOD_D3D9_HOOK_WINDOW";
    RegisterClassA(&wc);
    HWND dummy = CreateWindowA(wc.lpszClassName, "", WS_OVERLAPPED,
        0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
    if (!dummy) { d3d->Release(); return false; }

    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = dummy;

    IDirect3DDevice9* device = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
        dummy, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    if (FAILED(hr)) hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF,
        dummy, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);

    bool success = false;
    const MH_STATUS init_status = MH_Initialize();
    if (SUCCEEDED(hr) && device &&
        (init_status == MH_OK || init_status == MH_ERROR_ALREADY_INITIALIZED)) {
        void** table = *reinterpret_cast<void***>(device);
        const bool end_ok = MH_CreateHook(table[42], reinterpret_cast<LPVOID>(&HookedEndScene),
            reinterpret_cast<void**>(&original_end_scene)) == MH_OK;
        const bool reset_ok = MH_CreateHook(table[16], reinterpret_cast<LPVOID>(&HookedReset),
            reinterpret_cast<void**>(&original_reset)) == MH_OK;
        success = end_ok && reset_ok && MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
    }

    if (device) device->Release();
    d3d->Release();
    DestroyWindow(dummy);
    return success;
}
