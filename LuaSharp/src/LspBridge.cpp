#include "LspBridge.h"

#include <algorithm>
#include <functional>

namespace luasharp {
namespace {
size_t OffsetAt(const std::string& text, LspPosition position) {
    size_t offset = 0;
    for (size_t line = 0; line < position.line && offset < text.size(); ++line) {
        const auto newline = text.find('\n', offset);
        offset = newline == std::string::npos ? text.size() : newline + 1;
    }
    return std::min(offset + position.character, text.size());
}

LspPosition PositionAt(const std::string& text, size_t offset) {
    offset = std::min(offset, text.size());
    LspPosition position{};
    for (size_t index = 0; index < offset; ++index) {
        if (text[index] == '\n') { ++position.line; position.character = 0; }
        else ++position.character;
    }
    return position;
}
}

VirtualDocument CreateVirtualDocument(std::string sourceUri, std::string source) {
    const auto hash = std::hash<std::string>{}(sourceUri);
    auto compilation = CompileWithSourceMap(source);
    return {std::move(sourceUri), "luasharp-generated://" + std::to_string(hash) + ".lua", std::move(source), std::move(compilation)};
}

LspPosition ToGeneratedPosition(const VirtualDocument& document, LspPosition sourcePosition) {
    const auto sourceOffset = OffsetAt(document.source, sourcePosition);
    const auto found = std::upper_bound(document.compilation.sourceMap.begin(), document.compilation.sourceMap.end(), sourceOffset,
        [](size_t offset, const SourceMapEntry& entry) { return offset < entry.sourceOffset; });
    if (found == document.compilation.sourceMap.begin()) return {};
    return {std::prev(found)->generatedLine - 1, 0};
}

LspPosition ToSourcePosition(const VirtualDocument& document, LspPosition generatedPosition) {
    const auto line = generatedPosition.line + 1;
    const auto found = std::find_if(document.compilation.sourceMap.begin(), document.compilation.sourceMap.end(),
        [line](const SourceMapEntry& entry) { return entry.generatedLine == line; });
    return found == document.compilation.sourceMap.end() ? LspPosition{} : PositionAt(document.source, found->sourceOffset);
}

} // namespace luasharp
