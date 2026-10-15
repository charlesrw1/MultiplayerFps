#pragma once

#include "Compiler.h"

#include <cstddef>
#include <string>
#include <vector>

namespace luasharp {

struct LspPosition { size_t line; size_t character; };
struct VirtualDocument {
    std::string sourceUri;
    std::string virtualUri;
    std::string source;
    CompilationResult compilation;
};

VirtualDocument CreateVirtualDocument(std::string sourceUri, std::string source);
LspPosition ToGeneratedPosition(const VirtualDocument& document, LspPosition sourcePosition);
LspPosition ToSourcePosition(const VirtualDocument& document, LspPosition generatedPosition);

} // namespace luasharp
