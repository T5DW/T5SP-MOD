#pragma once

#include <string>
#include <vector>

namespace game_console {

void initialize();
void begin_frame();
void draw();
void print(int type, const std::string& data);

int toggle_key();
bool is_open();
bool console_char_event(int local_client, int key);
bool console_key_event(int local_client, int key, int down);

bool match_compare(const std::string& input,
                   const std::string& text,
                   bool exact);

void find_matches(std::string input,
                  std::vector<std::string>& suggestions,
                  bool exact);

// Invoke on a safe game-thread update after the game initializes.
void process_game_commands();
bool queue_game_command(const std::string& command);
bool engine_command_address_valid();
void enqueue_status_line(const std::string& message);

} // namespace game_console
