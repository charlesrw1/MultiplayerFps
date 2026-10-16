#include "Semantic.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace luasharp {
namespace {
std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t");
    return value.substr(first, last - first + 1);
}

bool IsNumber(std::string_view value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c) || c == '.' || c == '-'; });
}

bool Assignable(const std::string& expected, std::string value) {
    value = Trim(std::move(value));
    if (value.empty() || value == "null" || value == "{}" || value.starts_with("new ")) return true;
    if (expected == "int") return IsNumber(value) && value.find('.') == std::string::npos;
    if (expected == "float") return IsNumber(value);
    if (expected == "string") return value.size() >= 2 && (value.front() == '\"' || value.front() == '\'');
    if (expected == "bool") return value == "true" || value == "false";
    return true;
}
}

Analysis Analyze(std::string_view source) {
    Analysis analysis;
    std::istringstream input{std::string(source)};
    std::string line, currentClass;
    int classDepth = 0;
    const std::regex classPattern(R"(^\s*class\s+(\w+)(?:\s*:\s*(\w+))?\s*\{)");
    const std::regex fieldPattern(R"(^\s*(\w+(?:\[\])?)\s+(\w+)\s*(?:=\s*([^;]+))?;)");
    const std::regex methodPattern(R"(^\s*(\w+)\s+(\w+)\s*\((.*)\)\s*\{)");
    const std::regex constructorPattern(R"(^\s*(\w+)\s*\((.*)\)\s*\{)");
    const std::regex parameterPattern(R"(\s*(\w+(?:\[\])?)\s+(\w+)\s*)");
    const std::regex autoNew(R"(\bauto\s+(\w+)\s*=\s*new\s+(\w+))");

    for (size_t lineNumber{}; std::getline(input, line); ++lineNumber) {
        std::smatch match;
        if (std::regex_search(line, match, classPattern)) {
            currentClass = match[1]; classDepth = 0;
            if (match[2].matched) analysis.baseTypes[currentClass] = match[2];
            analysis.symbols.push_back({currentClass, currentClass, SymbolKind::Class, lineNumber, static_cast<size_t>(match.position(1))});
        } else if (std::regex_search(line, match, methodPattern)) {
            const auto type = match[1].str(), name = match[2].str();
            Symbol method{name, type, SymbolKind::Method, lineNumber, static_cast<size_t>(match.position(2))};
            analysis.symbols.push_back(method);
            if (!currentClass.empty()) analysis.members[currentClass].push_back(method);
            std::stringstream parameters(match[3].str()); std::string parameter;
            while (std::getline(parameters, parameter, ',')) {
                std::smatch parameterMatch;
                if (std::regex_match(parameter, parameterMatch, parameterPattern)) {
                    Symbol p{parameterMatch[2], parameterMatch[1], SymbolKind::Parameter, lineNumber, static_cast<size_t>(match.position(3) + parameterMatch.position(2))};
                    analysis.symbols.push_back(p); analysis.variableTypes[p.name] = p.type;
                }
            }
        } else if (std::regex_search(line, match, constructorPattern) && match[1] == currentClass) {
            Symbol method{currentClass, currentClass, SymbolKind::Method, lineNumber, static_cast<size_t>(match.position(1))};
            analysis.symbols.push_back(method); analysis.members[currentClass].push_back(method);
        } else if (std::regex_search(line, match, fieldPattern)) {
            const auto type = match[1].str(), name = match[2].str(), value = match[3].str();
            const auto kind = currentClass.empty() || classDepth > 1 ? SymbolKind::Variable : SymbolKind::Field;
            Symbol symbol{name, type, kind, lineNumber, static_cast<size_t>(match.position(2))};
            analysis.symbols.push_back(symbol); analysis.variableTypes[name] = type;
            if (!currentClass.empty() && classDepth <= 1) analysis.members[currentClass].push_back(symbol);
            if (!Assignable(type, value)) analysis.diagnostics.push_back({"Cannot assign value to " + type, lineNumber, static_cast<size_t>(match.position(3))});
        }
        if (std::regex_search(line, match, autoNew)) {
            const auto name = match[1].str(), type = match[2].str();
            analysis.variableTypes[name] = type;
            analysis.symbols.push_back({name, type, SymbolKind::Variable, lineNumber, static_cast<size_t>(match.position(1))});
        }
        classDepth += static_cast<int>(std::count(line.begin(), line.end(), '{')) - static_cast<int>(std::count(line.begin(), line.end(), '}'));
        if (!currentClass.empty() && classDepth <= 0) currentClass.clear();
    }
    // Flatten inherited members once all declarations are known. This is intentionally
    // bounded: a malformed cyclic hierarchy cannot hang the editor process.
    for (const auto& [derived, base] : analysis.baseTypes) {
        std::string current = base;
        for (int depth = 0; depth != 32 && !current.empty(); ++depth) {
            const auto inherited = analysis.members.find(current);
            if (inherited != analysis.members.end()) analysis.members[derived].insert(analysis.members[derived].end(), inherited->second.begin(), inherited->second.end());
            const auto parent = analysis.baseTypes.find(current); current = parent == analysis.baseTypes.end() ? "" : parent->second;
        }
    }
    return analysis;
}
} // namespace luasharp
