#pragma once
#include <filesystem>

// Writes the generated LuaSharp declarations for the tools (the LSP and LuaSharp.exe), which find them through
// luasharp.json. The engine itself compiles against the declarations in the binary.
//   <scripts_dir>/.luasharp/engine.ls  only when its text changed
//   <scripts_dir>/luasharp.json        only when it is missing
// Returns false on an I/O error (logged).
bool write_luasharp_declarations(const std::filesystem::path& scripts_dir);
