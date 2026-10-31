#pragma once

#include <string>
#include <vector>

namespace engine_bridge {

// The addresses in this bridge are specific to the supplied BO1 SP dump.
void initialize_commands_on_game_thread();
void refresh_dvars_on_game_thread();
void get_dvar_names(std::vector<std::string>& names);
bool native_command_registered();
bool dynamic_dvars_available();

} // namespace engine_bridge
