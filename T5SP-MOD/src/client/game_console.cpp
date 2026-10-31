#include "game_console.hpp"
#include "engine_bridge.hpp"
#include "offsets.hpp"

#include <windows.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace game_console {
namespace {

struct LogLine {
    bool error;
    std::string message;
};

bool open = false;
bool show_output = true;
bool scroll_to_bottom = false;
char input[512]{};
bool request_focus = false;

std::mutex command_mutex;
std::deque<std::string> pending_commands;
std::deque<LogLine> lines;
std::mutex status_log_mutex;
std::deque<std::string> status_logs;

constexpr int max_lines = 1000;
constexpr int toggle_vk = VK_OEM_3;
void add_line(bool error, const std::string& text);

bool valid_cbuf_signature() {
    // Check stable instruction bytes around the dump's Cbuf_AddText entry.
    // Prevents calling a stale hardcoded address in another BO1 executable.
    const auto* code = reinterpret_cast<const unsigned char*>(offsets::Cbuf_AddText);
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(code, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
        return false;
    }
    return code[0] == 0x6A && code[1] == 0x35 && code[2] == 0xE8 &&
        code[7] == 0x8B && code[8] == 0x4C && code[9] == 0x24 &&
        code[10] == 0x0C && code[11] == 0x83 && code[12] == 0xC4;
}

// These exact names were found in the supplied BO1 executable.
constexpr std::array<const char*, 17> bo1_dvars = {
    "cg_draw2D", "cg_drawFPS", "cg_drawGun", "cg_fov",
    "cg_thirdPerson", "cl_maxpackets", "com_maxfps", "developer",
    "developer_script", "g_speed", "m_pitch", "r_aaSamples",
    "r_fullscreen", "r_mode", "r_vsync", "sensitivity", "sv_cheats"
};

void add_line(bool error, const std::string& text) {
    lines.push_back({error, text});
    if (lines.size() > max_lines) {
        lines.pop_front();
    }
    scroll_to_bottom = true;
}

void submit(const std::string& command) {
    if (command.empty()) return;

    add_line(false, "] " + command);

    if (command == "clear") {
        lines.clear();
        return;
    }

    if (command == "help") {
        add_line(false, "Commands: help, clear, status, t5sp_status, BO1 commands");
        add_line(false, "Use TAB for BO1 dvar suggestions.");
        return;
    }

    if (command == "status") {
        add_line(!valid_cbuf_signature(), valid_cbuf_signature()
            ? "Cbuf_AddText signature matched the supplied BO1 dump."
            : "Cbuf_AddText signature mismatch; engine commands disabled.");
        add_line(false, engine_bridge::native_command_registered()
            ? "Cmd_AddCommand: t5sp_status registered" : "Cmd_AddCommand: not registered");
        add_line(false, engine_bridge::dynamic_dvars_available()
            ? "Dvar_FindVar: live dvar autocomplete active" : "Dvar_FindVar: fallback names only");
        return;
    }

    // Keep the input callback off the engine's command-buffer internals.
    std::lock_guard<std::mutex> lock(command_mutex);
    pending_commands.push_back(command + "\n");
}

int input_callback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag != ImGuiInputTextFlags_CallbackCompletion) {
        return 0;
    }

    const std::string typed(data->Buf, data->BufTextLen);
    std::vector<std::string> matches;
    find_matches(typed, matches, false);

    if (matches.size() == 1) {
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, matches.front().c_str());
    } else if (!matches.empty()) {
        add_line(false, "Suggestions:");
        for (size_t i = 0; i < matches.size() && i < 12; ++i) {
            add_line(false, "  " + matches[i]);
        }
    }

    return 0;
}

} // namespace

void initialize() {
    add_line(false, "T5SP-MOD V1.0 | BO1 SP / Zombies");
    add_line(false, "Press ` to toggle the console.");
}

void begin_frame() {
    engine_bridge::initialize_commands_on_game_thread();
    engine_bridge::refresh_dvars_on_game_thread();

    std::deque<std::string> captured;
    {
        std::lock_guard<std::mutex> lock(status_log_mutex);
        captured.swap(status_logs);
    }
    for (const auto& line : captured) {
        add_line(false, line);
    }
}

void enqueue_status_line(const std::string& message) {
    std::lock_guard<std::mutex> lock(status_log_mutex);
    status_logs.push_back(message);
    if (status_logs.size() > 100) status_logs.pop_front();
}

int toggle_key() {
    return toggle_vk;
}

bool is_open() {
    return open;
}

void print(int type, const std::string& data) {
    add_line(type != 0, data);
}

bool console_char_event(int, int key) {
    if (!open) return true;

    if (toggle_vk == VK_OEM_3 && (key == 96 || key == 126)) {
        return false;
    }

    if (key >= 32 && key <= 0xFFFF) {
        ImGui::GetIO().AddInputCharacter(static_cast<unsigned>(key));
    }

    return false;
}

bool console_key_event(int, int key, int down) {
    if (key == toggle_vk) {
        if (down) {
            if (GetKeyState(VK_SHIFT) & 0x8000) {
                show_output = !show_output;
            } else {
                open = !open;
                request_focus = open;
            }
        }
        return false;
    }

    if (open && key == VK_ESCAPE && down) {
        open = false;
        return false;
    }

    return !open;
}

bool match_compare(const std::string& input,
                   const std::string& text,
                   bool exact) {
    auto lower = [](std::string value) {
        for (auto& character : value) {
            character = static_cast<char>(
                std::tolower(static_cast<unsigned char>(character)));
        }
        return value;
    };

    const auto a = lower(input);
    const auto b = lower(text);
    return exact ? a == b : b.find(a) != std::string::npos;
}

void find_matches(std::string typed,
                  std::vector<std::string>& suggestions,
                  bool exact) {
    suggestions.clear();

    std::vector<std::string> live_names;
    engine_bridge::get_dvar_names(live_names);
    if (!live_names.empty()) {
        for (const auto& name : live_names) {
            if (match_compare(typed, name, exact)) suggestions.push_back(name);
            if (suggestions.size() >= 128) break;
        }
    } else {
        for (const char* name : bo1_dvars) {
            if (match_compare(typed, name, exact)) suggestions.emplace_back(name);
        }
    }

    std::sort(suggestions.begin(), suggestions.end());
    suggestions.erase(
        std::unique(suggestions.begin(), suggestions.end()),
        suggestions.end());
}

void draw() {
    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    const ImU32 muted_gray = IM_COL32(132, 136, 141, 210);

    if (!open) {
        ImGui::GetForegroundDrawList()->AddText(
            ImVec2(10, 10), muted_gray, "T5SP-MOD (V1.0) -T5DW");
        return;
    }

    std::vector<std::string> suggestions;
    const bool typing = input[0] != '\0';
    if (typing) find_matches(input, suggestions, false);
    const bool expanded = show_output && (typing || !lines.empty() && scroll_to_bottom);
    const float height = expanded ? 242.0f : 37.0f;

    ImGui::SetNextWindowPos(ImVec2(3, 3), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(screen.x - 6, height), ImGuiCond_Always);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 3));

    if (ImGui::Begin("##t5sp_console", nullptr, flags)) {
        const ImVec2 pos = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        ImDrawList* painter = ImGui::GetWindowDrawList();
        painter->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            IM_COL32(4, 4, 5, 235), 2.0f);
        painter->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            IM_COL32(225, 16, 20, 255), 2.0f, 0, 2.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.11f, 0.11f, 1.0f));
        ImGui::TextUnformatted("T5SP-MOD: 1.0>");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushItemWidth(-9);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

        if (request_focus) {
            ImGui::SetKeyboardFocusHere();
            request_focus = false;
        }

        const bool submitted = ImGui::InputText("##command", input, sizeof(input),
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_CallbackCompletion, input_callback);

        if (submitted) {
            submit(input);
            input[0] = '\0';
            request_focus = true;
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        ImGui::PopItemWidth();

        if (expanded) {
            ImGui::Separator();
            ImGui::BeginChild("##t5sp_output", ImVec2(0, -8), false);

            if (typing && !suggestions.empty()) {
                for (size_t i = 0; i < suggestions.size() && i < 9; ++i) {
                    ImGui::TextColored(ImVec4(0.95f, 0.32f, 0.32f, 1.0f),
                        "%s", suggestions[i].c_str());
                }
            } else {
                for (const auto& line : lines) {
                    ImGui::TextColored(line.error
                        ? ImVec4(1, 0.25f, 0.25f, 1)
                        : ImVec4(0.78f, 0.78f, 0.78f, 1),
                        "%s", line.message.c_str());
                }
            }

            if (scroll_to_bottom) ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            scroll_to_bottom = false;
        }
    }

    ImGui::End();
    ImGui::PopStyleVar(2);

    ImGui::GetForegroundDrawList()->AddText(ImVec2(10, height + 8),
        muted_gray, "T5SP-MOD (V1.0) -T5DW");
}

void process_game_commands() {
    std::deque<std::string> batch;

    {
        std::lock_guard<std::mutex> lock(command_mutex);
        batch.swap(pending_commands);
    }

    // This must run only for the matching 32-bit game version.
    using Cbuf_AddText = void(__cdecl*)(int, const char*);
    if (!valid_cbuf_signature()) return;
    auto execute = reinterpret_cast<Cbuf_AddText>(offsets::Cbuf_AddText);

    for (const auto& command : batch) {
        execute(0, command.c_str());
    }
}

bool engine_command_address_valid() {
    return valid_cbuf_signature();
}

bool queue_game_command(const std::string& command) {
    if (command.empty() || command.size() > 1023 || !valid_cbuf_signature()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(command_mutex);
    pending_commands.push_back(command + "\n");
    return true;
}

} // namespace game_console
