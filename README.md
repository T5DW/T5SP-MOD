T5SP-MOD

A lightweight developer-console experiment for Call of Duty: Black Ops (2010) — PC Single Player / Zombies.

Created by T5DW.

T5SP-MOD is a 32-bit Direct3D 9 (d3d9.dll) proxy that adds a black-and-red ImGui developer console, a separate Windows CMD window for mod diagnostics, and experimental integration with the Black Ops 1 command and dvar systems.

[!WARNING]
Experimental / untested in-game. The DLL has been cross-compiled and its exports inspected, but the engine hooks have not been validated in a running Black Ops process. Addresses were researched against one supplied 2011 executable dump. Use the matching original executable, keep backups, and expect incompatibilities with other builds.

Features

ImGui developer console with black background, red outline, and command input.

CMD diagnostics window for proxy status and commands you enter. Engine log capture is deliberately not included.

Cbuf_AddText integration for queuing supported BO1 engine commands.

Cmd_AddCommand integration for an experimental native t5sp_status command.

Dvar_FindVar integration and live dvar autocomplete, with a built-in fallback list.

Tab completion, on-screen suggestions, and local help, status, and clear commands.

Watermark: T5SP-MOD (V1.0) -T5DW.

Centralized offsets in offsets.cpp and offsets.hpp.

Not included

This is a console-focused project. It does not include menu decompiling/recompiling, custom fastfile loading, game authentication bypasses, or an offline-services emulator. It also does not hook Com_Printf.

Requirements

Windows and a legitimate PC copy of Call of Duty: Black Ops.

The 32-bit Black Ops SP executable matching the researched offsets.

A Direct3D 9-capable game environment.

For building: a C++17 compiler and xmake, plus the included Dear ImGui and MinHook source trees under third_party/.

Installation

Back up any existing d3d9.dll in your Black Ops installation directory.

Download a release build or compile the project yourself.

Place d3d9.dll beside BlackOps.exe.

Launch Black Ops Single Player or Zombies.

Press backtick (`) to open or close the ImGui console.

If your game crashes, first remove the proxy DLL to return to your previous setup. Do not install multiple D3D9 proxy DLLs under the same filename.

Console commands

Command

Purpose

help

Show T5SP-MOD console help.

status

Show the command-address and experimental bridge status.

clear

Clear the ImGui console output.

t5sp_status

Experimental command registered through BO1's command system; displays the detected dvar count.

Any supported BO1 engine command

Queued via Cbuf_AddText, subject to the game's normal restrictions.

Use Tab in the ImGui input field for dvar autocomplete. The CMD window accepts help, status, and exit; other input is queued for BO1 where supported.

Building from source

Clone the repository with its required third-party source trees, then run in the project directory:

xmake f -a x86 -m release
xmake

The expected output is a 32-bit d3d9.dll (typically under build/windows/x86/release/).

Project source overview:

File

Role

d3d9_proxy.cpp

System D3D9 forwarding and CMD window.

d3d9_imgui_hook.cpp

D3D9 device hooks and ImGui rendering.

game_console.cpp

Command input, output UI, and autocomplete.

engine_bridge.cpp

Experimental BO1 native command and dvar integration.

offsets.cpp / offsets.hpp

Researched addresses, referenced by name.

xmake.lua

Build definition.

d3d9.def

D3D9 export names.

Researched BO1 addresses

All addresses below refer to one specific BO1 SP executable dump, not to every release:

Symbol

Address

Notes

Cbuf_AddText

0x0049B930

Located and disassembled.

Cmd_AddCommand

0x00661400

Reverse-engineered candidate; live-game behavior unverified.

Dvar_FindVar

0x005AE810

Reverse-engineered candidate; live-game behavior unverified.

Dvar_LookupHelper

0x00862280

Hash lookup routine candidate.

Dvar_HashBuckets

0x02620BF0

Dvar registry data address; not a function.

The code performs short instruction-signature checks before using these addresses. This reduces accidental use with mismatched executables but does not guarantee safety. Do not blindly reuse them across BO1 versions.

Contributing

Reverse-engineering notes, issue reports, safer version detection, and fixes are welcome.

When reporting a bug, include:

Your BO1 executable version and whether it is the original Steam release.

What you typed or did immediately before the issue.

Relevant T5SP-MOD CMD output and reproducible steps.

Do not upload copyrighted game executables or proprietary fastfiles to public issues or pull requests.

Credits

T5DW — project creator.

Dear ImGui — graphical developer console UI.

MinHook — function hooking library.

Activision / Treyarch — original Call of Duty: Black Ops game.

This is an independent community experiment and is not affiliated with or endorsed by Activision or Treyarch.
