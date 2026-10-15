#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace luasharp {
struct SourceMapEntry {
    size_t generatedLine;
    size_t sourceOffset;
};

struct CompilationResult {
    std::string lua;
    std::vector<SourceMapEntry> sourceMap;
};

std::string Compile(std::string_view source);
CompilationResult CompileWithSourceMap(std::string_view source);
}
