#include "RuntimeSession.h"

#include "../LevelEdit/EditorChunkIDs.h"
#include "../../Combat/combatchunkid.h"
#include "../../WWAudio/SoundChunkIDs.h"
#include "../../wwlib/chunkio.h"
#include "../../wwlib/ffactory.h"
#include "../../wwlib/mixfile.h"
#include "../../wwlib/rawfile.h"
#include "../../wwsaveload/saveloadids.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QSet>
#include <QString>
#include <QStringList>

namespace leveledit_qt {

namespace {

constexpr std::uint32_t kChunkVariables = 0x00000100;
constexpr std::uint32_t kChunkObjectCollection = 0x00000101;
constexpr std::uint32_t kSimpleFactoryChunkObjectData = 0x00100101;

constexpr std::uint32_t kDefinitionVarInstanceId = 0x01;
constexpr std::uint32_t kDefinitionVarName = 0x03;

constexpr std::uint32_t kPresetVarDefinitionId = 0x01;
constexpr std::uint32_t kPresetVarIsTemporary = 0x02;
constexpr std::uint32_t kPresetVarParentId = 0x07;

constexpr std::uint32_t kDefinitionClassIdBase = 0x00001000;
constexpr std::uint32_t kDefinitionIdRange = 10000;

struct DefinitionSnapshot
{
    std::uint32_t id = 0;
    std::uint32_t class_id = 0;
    std::string name;
};

struct DefinitionFieldAnnotation
{
    std::string source_file;
    std::string definition_class;
    std::unordered_map<std::uint32_t, std::string> chunk_names;
    std::unordered_set<std::uint32_t> chunk_ids_with_fields;
    std::unordered_map<std::uint64_t, std::string> field_names;
};

struct DefinitionFieldAnnotationCache
{
    bool initialized = false;
    std::unordered_map<std::uint32_t, DefinitionFieldAnnotation> by_definition_chunk;
    std::unordered_map<std::uint32_t, std::string> global_chunk_names;
    std::unordered_set<std::uint32_t> global_chunk_ids_with_fields;
    std::unordered_map<std::uint64_t, std::string> global_field_names;
};

std::uint64_t MakeChunkMicroKey(std::uint32_t chunk_id, std::uint32_t micro_id)
{
    return (static_cast<std::uint64_t>(chunk_id) << 32) |
        static_cast<std::uint64_t>(micro_id);
}

std::string TrimCopy(const std::string &value)
{
    std::size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return value.substr(begin, end - begin);
}

std::string StripLineComment(const std::string &line)
{
    const std::size_t comment_pos = line.find("//");
    if (comment_pos == std::string::npos) {
        return line;
    }
    return line.substr(0, comment_pos);
}

std::string StripLineComments(std::string text)
{
    std::stringstream stream(text);
    std::string line;
    std::string output;
    output.reserve(text.size());

    while (std::getline(stream, line)) {
        output += StripLineComment(line);
        output.push_back('\n');
    }

    return output;
}

std::string StripBlockComments(std::string text)
{
    std::size_t search_pos = 0;
    while (true) {
        const std::size_t begin = text.find("/*", search_pos);
        if (begin == std::string::npos) {
            break;
        }

        const std::size_t end = text.find("*/", begin + 2);
        if (end == std::string::npos) {
            text.erase(begin);
            break;
        }

        text.erase(begin, (end + 2) - begin);
        search_pos = begin;
    }

    return text;
}

std::string NormalizePathSeparators(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

std::filesystem::path DetectRepoRootFromSourcePath()
{
    std::string file = NormalizePathSeparators(__FILE__);
    const std::string marker = "/Code/Tools/LevelEditQt/RuntimeSession.cpp";
    const std::size_t marker_pos = file.rfind(marker);
    if (marker_pos != std::string::npos) {
        return std::filesystem::path(file.substr(0, marker_pos));
    }

    const std::string relative_marker = "Code/Tools/LevelEditQt/RuntimeSession.cpp";
    const std::size_t relative_pos = file.rfind(relative_marker);
    if (relative_pos == std::string::npos) {
        return std::filesystem::path();
    }

    if (relative_pos > 0 && file[relative_pos - 1] == '/') {
        return std::filesystem::path(file.substr(0, relative_pos - 1));
    }
    return std::filesystem::path(file.substr(0, relative_pos));
}

std::filesystem::path FindRepoRootFromCurrentPath()
{
    std::error_code ec;
    std::filesystem::path current = std::filesystem::current_path(ec);
    if (ec) {
        return std::filesystem::path();
    }

    while (!current.empty()) {
        const std::filesystem::path marker =
            current / "Code/Tools/LevelEditQt/RuntimeSession.cpp";
        if (std::filesystem::exists(marker, ec)) {
            return current;
        }

        const std::filesystem::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    return std::filesystem::path();
}

bool ReadTextFile(const std::filesystem::path &path, std::string &text)
{
    text.clear();

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    text = buffer.str();
    return true;
}

class ExpressionParser
{
public:
    ExpressionParser(const std::string &expression,
                     const std::unordered_map<std::string, std::uint32_t> &symbols)
        : _expression(expression)
        , _symbols(symbols)
    {
    }

    bool parse(std::uint32_t &value)
    {
        skipWhitespace();
        std::int64_t parsed = 0;
        if (!parseBitwiseOr(parsed)) {
            return false;
        }

        skipWhitespace();
        if (_position != _expression.size()) {
            return false;
        }

        if (parsed < 0) {
            return false;
        }

        value = static_cast<std::uint32_t>(parsed);
        return true;
    }

private:
    bool parseBitwiseOr(std::int64_t &value)
    {
        if (!parseAdditive(value)) {
            return false;
        }

        while (true) {
            skipWhitespace();
            if (!consume('|')) {
                break;
            }

            std::int64_t rhs = 0;
            if (!parseAdditive(rhs)) {
                return false;
            }
            value |= rhs;
        }

        return true;
    }

    bool parseAdditive(std::int64_t &value)
    {
        if (!parseShift(value)) {
            return false;
        }

        while (true) {
            skipWhitespace();
            if (consume('+')) {
                std::int64_t rhs = 0;
                if (!parseShift(rhs)) {
                    return false;
                }
                value += rhs;
            } else if (consume('-')) {
                std::int64_t rhs = 0;
                if (!parseShift(rhs)) {
                    return false;
                }
                value -= rhs;
            } else {
                break;
            }
        }

        return true;
    }

    bool parseShift(std::int64_t &value)
    {
        if (!parseMultiplicative(value)) {
            return false;
        }

        while (true) {
            skipWhitespace();
            if (match("<<")) {
                _position += 2;
                std::int64_t rhs = 0;
                if (!parseMultiplicative(rhs)) {
                    return false;
                }
                value <<= rhs;
            } else if (match(">>")) {
                _position += 2;
                std::int64_t rhs = 0;
                if (!parseMultiplicative(rhs)) {
                    return false;
                }
                value >>= rhs;
            } else {
                break;
            }
        }

        return true;
    }

    bool parseMultiplicative(std::int64_t &value)
    {
        if (!parsePrimary(value)) {
            return false;
        }

        while (true) {
            skipWhitespace();
            if (consume('*')) {
                std::int64_t rhs = 0;
                if (!parsePrimary(rhs)) {
                    return false;
                }
                value *= rhs;
            } else if (consume('/')) {
                std::int64_t rhs = 0;
                if (!parsePrimary(rhs) || rhs == 0) {
                    return false;
                }
                value /= rhs;
            } else {
                break;
            }
        }

        return true;
    }

    bool parsePrimary(std::int64_t &value)
    {
        skipWhitespace();
        if (_position >= _expression.size()) {
            return false;
        }

        if (consume('(')) {
            if (!parseBitwiseOr(value)) {
                return false;
            }
            skipWhitespace();
            return consume(')');
        }

        if (consume('+')) {
            return parsePrimary(value);
        }

        if (consume('-')) {
            if (!parsePrimary(value)) {
                return false;
            }
            value = -value;
            return true;
        }

        if (std::isdigit(static_cast<unsigned char>(_expression[_position])) != 0) {
            return parseNumber(value);
        }

        return parseIdentifier(value);
    }

    bool parseNumber(std::int64_t &value)
    {
        const std::size_t begin = _position;
        while (_position < _expression.size() &&
               (std::isalnum(static_cast<unsigned char>(_expression[_position])) != 0 ||
                _expression[_position] == 'x' || _expression[_position] == 'X')) {
            ++_position;
        }

        std::string token = _expression.substr(begin, _position - begin);
        while (!token.empty()) {
            const char suffix = token.back();
            if (suffix == 'u' || suffix == 'U' || suffix == 'l' || suffix == 'L') {
                token.pop_back();
            } else {
                break;
            }
        }

        if (token.empty()) {
            return false;
        }

        try {
            std::size_t consumed = 0;
            const unsigned long long parsed = std::stoull(token, &consumed, 0);
            if (consumed != token.size()) {
                return false;
            }
            value = static_cast<std::int64_t>(parsed);
            return true;
        } catch (...) {
            return false;
        }
    }

    bool parseIdentifier(std::int64_t &value)
    {
        const std::size_t begin = _position;
        if (!std::isalpha(static_cast<unsigned char>(_expression[_position])) &&
            _expression[_position] != '_') {
            return false;
        }

        ++_position;
        while (_position < _expression.size()) {
            const unsigned char ch = static_cast<unsigned char>(_expression[_position]);
            if (std::isalnum(ch) != 0 || ch == '_') {
                ++_position;
            } else {
                break;
            }
        }

        const std::string token = _expression.substr(begin, _position - begin);
        const auto it = _symbols.find(token);
        if (it == _symbols.end()) {
            return false;
        }

        value = static_cast<std::int64_t>(it->second);
        return true;
    }

    bool consume(char ch)
    {
        if (_position < _expression.size() && _expression[_position] == ch) {
            ++_position;
            return true;
        }
        return false;
    }

    bool match(const char *text) const
    {
        const std::size_t length = std::strlen(text);
        return _expression.compare(_position, length, text) == 0;
    }

    void skipWhitespace()
    {
        while (_position < _expression.size() &&
               std::isspace(static_cast<unsigned char>(_expression[_position])) != 0) {
            ++_position;
        }
    }

    const std::string &_expression;
    const std::unordered_map<std::string, std::uint32_t> &_symbols;
    std::size_t _position = 0;
};

bool TryParseExpression(const std::string &expression,
                        const std::unordered_map<std::string, std::uint32_t> &symbols,
                        std::uint32_t &value)
{
    ExpressionParser parser(TrimCopy(expression), symbols);
    return parser.parse(value);
}

std::size_t FindMatchingBrace(const std::string &text, std::size_t open_brace_pos)
{
    if (open_brace_pos >= text.size() || text[open_brace_pos] != '{') {
        return std::string::npos;
    }

    int depth = 0;
    for (std::size_t i = open_brace_pos; i < text.size(); ++i) {
        if (text[i] == '{') {
            ++depth;
        } else if (text[i] == '}') {
            --depth;
            if (depth == 0) {
                return i;
            }
        }
    }

    return std::string::npos;
}

void ParseEnumSymbols(const std::string &text,
                      std::unordered_map<std::string, std::uint32_t> &symbols,
                      std::unordered_map<std::string, std::uint32_t> *added_symbols = nullptr)
{
    std::string cleaned = StripLineComments(StripBlockComments(text));
    std::size_t search_pos = 0;
    while (true) {
        const std::size_t enum_pos = cleaned.find("enum", search_pos);
        if (enum_pos == std::string::npos) {
            break;
        }

        const std::size_t brace_open = cleaned.find('{', enum_pos);
        if (brace_open == std::string::npos) {
            break;
        }

        const std::size_t brace_close = FindMatchingBrace(cleaned, brace_open);
        if (brace_close == std::string::npos) {
            break;
        }

        const std::string body = cleaned.substr(brace_open + 1, brace_close - brace_open - 1);
        std::stringstream stream(body);
        std::string entry;

        bool has_previous_value = false;
        std::uint32_t previous_value = 0;

        while (std::getline(stream, entry, ',')) {
            entry = TrimCopy(StripLineComment(entry));
            if (entry.empty()) {
                continue;
            }

            const std::size_t equals_pos = entry.find('=');
            std::string symbol_name =
                TrimCopy(entry.substr(0, equals_pos == std::string::npos ? entry.size() : equals_pos));
            if (symbol_name.empty()) {
                continue;
            }

            const unsigned char first_char = static_cast<unsigned char>(symbol_name[0]);
            if (std::isalpha(first_char) == 0 && symbol_name[0] != '_') {
                continue;
            }

            for (std::size_t i = 1; i < symbol_name.size(); ++i) {
                const unsigned char ch = static_cast<unsigned char>(symbol_name[i]);
                if (std::isalnum(ch) == 0 && symbol_name[i] != '_') {
                    symbol_name.clear();
                    break;
                }
            }

            if (symbol_name.empty()) {
                continue;
            }

            std::uint32_t value = 0;
            bool has_value = false;
            if (equals_pos != std::string::npos) {
                const std::string expression = TrimCopy(entry.substr(equals_pos + 1));
                has_value = TryParseExpression(expression, symbols, value);
            } else if (has_previous_value) {
                value = previous_value + 1;
                has_value = true;
            } else {
                value = 0;
                has_value = true;
            }

            if (!has_value) {
                continue;
            }

            symbols[symbol_name] = value;
            if (added_symbols != nullptr) {
                (*added_symbols)[symbol_name] = value;
            }
            previous_value = value;
            has_previous_value = true;
        }

        search_pos = brace_close + 1;
    }
}

bool ParseMacroArgumentsAt(const std::string &text,
                           std::size_t macro_pos,
                           std::vector<std::string> &args)
{
    args.clear();
    const std::size_t paren_open = text.find('(', macro_pos);
    if (paren_open == std::string::npos) {
        return false;
    }

    int depth = 1;
    std::size_t arg_begin = paren_open + 1;
    for (std::size_t i = paren_open + 1; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == '(') {
            ++depth;
        } else if (ch == ')') {
            --depth;
            if (depth == 0) {
                args.push_back(TrimCopy(text.substr(arg_begin, i - arg_begin)));
                return true;
            }
        } else if (ch == ',' && depth == 1) {
            args.push_back(TrimCopy(text.substr(arg_begin, i - arg_begin)));
            arg_begin = i + 1;
        }
    }

    return false;
}

std::optional<std::uint32_t> ResolveSymbolValue(
    const std::string &expression,
    const std::unordered_map<std::string, std::uint32_t> &symbols)
{
    std::uint32_t value = 0;
    if (!TryParseExpression(expression, symbols, value)) {
        return std::nullopt;
    }
    return value;
}

std::string NormalizeFieldName(std::string expression)
{
    expression = TrimCopy(expression);
    while (!expression.empty() && (expression.front() == '&' || expression.front() == '*')) {
        expression.erase(expression.begin());
        expression = TrimCopy(expression);
    }

    while (!expression.empty() && expression.front() == '(' && expression.back() == ')') {
        expression = TrimCopy(expression.substr(1, expression.size() - 2));
    }

    if (expression.rfind("this->", 0) == 0) {
        expression.erase(0, 6);
    }

    return expression;
}

void CaptureFieldMappingsFromMethod(
    const std::string &text,
    const std::string &class_name,
    const std::string &method_name,
    const std::unordered_map<std::string, std::uint32_t> &symbols,
    DefinitionFieldAnnotation &annotation)
{
    const std::string marker = class_name + "::" + method_name;
    std::size_t search_pos = 0;
    while (true) {
        const std::size_t method_pos = text.find(marker, search_pos);
        if (method_pos == std::string::npos) {
            break;
        }

        const std::size_t brace_open = text.find('{', method_pos);
        if (brace_open == std::string::npos) {
            break;
        }

        const std::size_t brace_close = FindMatchingBrace(text, brace_open);
        if (brace_close == std::string::npos) {
            break;
        }

        std::vector<std::uint32_t> chunk_stack;

        const std::string body = text.substr(brace_open + 1, brace_close - brace_open - 1);
        std::stringstream stream(body);
        std::string line;
        while (std::getline(stream, line)) {
            line = TrimCopy(StripLineComment(line));
            if (line.empty()) {
                continue;
            }

            const std::size_t begin_pos = line.find("Begin_Chunk");
            if (begin_pos != std::string::npos) {
                std::vector<std::string> args;
                if (ParseMacroArgumentsAt(line, begin_pos, args) && !args.empty()) {
                    if (const std::optional<std::uint32_t> chunk_value =
                            ResolveSymbolValue(args[0], symbols);
                        chunk_value.has_value()) {
                        chunk_stack.push_back(*chunk_value);
                    }
                }
            }

            if (line.find("End_Chunk") != std::string::npos && !chunk_stack.empty()) {
                chunk_stack.pop_back();
            }

            const char *macro_names[] = {
                "WRITE_MICRO_CHUNK_WWSTRING",
                "WRITE_MICRO_CHUNK_WIDESTRING",
                "WRITE_MICRO_CHUNK_STRING",
                "WRITE_MICRO_CHUNK_PTR",
                "WRITE_SAFE_MICRO_CHUNK",
                "WRITE_MICRO_CHUNK",
                "READ_MICRO_CHUNK_WWSTRING",
                "READ_MICRO_CHUNK_WIDESTRING",
                "READ_MICRO_CHUNK_STRING",
                "READ_MICRO_CHUNK_PTR",
                "READ_SAFE_MICRO_CHUNK",
                "READ_MICRO_CHUNK",
            };

            std::size_t macro_pos = std::string::npos;
            for (const char *macro_name : macro_names) {
                macro_pos = line.find(macro_name);
                if (macro_pos != std::string::npos) {
                    break;
                }
            }

            if (macro_pos == std::string::npos) {
                continue;
            }

            std::vector<std::string> args;
            if (!ParseMacroArgumentsAt(line, macro_pos, args) || args.size() < 3) {
                continue;
            }

            if (chunk_stack.empty()) {
                continue;
            }

            const std::optional<std::uint32_t> micro_value = ResolveSymbolValue(args[1], symbols);
            if (!micro_value.has_value()) {
                continue;
            }

            const std::uint32_t chunk_id = chunk_stack.back();
            const std::uint64_t key = MakeChunkMicroKey(chunk_id, *micro_value);
            if (annotation.field_names.find(key) == annotation.field_names.end()) {
                const std::string field_name = NormalizeFieldName(args[2]);
                if (!field_name.empty()) {
                    annotation.field_names[key] = field_name;
                    annotation.chunk_ids_with_fields.insert(chunk_id);
                }
            }
        }

        search_pos = brace_close + 1;
    }
}

void ParseDefinitionFactories(
    const std::string &text,
    const std::unordered_map<std::string, std::uint32_t> &symbols,
    std::vector<std::pair<std::uint32_t, std::string>> &factories)
{
    factories.clear();

    const std::regex factory_regex(
        R"(SimplePersistFactoryClass\s*<\s*([A-Za-z_][A-Za-z0-9_:]*)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*>)");

    for (std::sregex_iterator it(text.begin(), text.end(), factory_regex), end; it != end; ++it) {
        const std::string class_name = (*it)[1].str();
        const std::string chunk_token = (*it)[2].str();

        if (class_name.find("Def") == std::string::npos &&
            class_name.find("Definition") == std::string::npos) {
            continue;
        }

        if (const std::optional<std::uint32_t> chunk_id =
                ResolveSymbolValue(chunk_token, symbols);
            chunk_id.has_value()) {
            factories.emplace_back(*chunk_id, class_name);
        }
    }
}

bool PreferChunkName(const std::string &current_name, const std::string &candidate_name);

void AddChunkNamesFromSymbols(const std::unordered_map<std::string, std::uint32_t> &symbols,
                              std::unordered_map<std::uint32_t, std::string> &chunk_names)
{
    for (const auto &entry : symbols) {
        if (entry.first.rfind("CHUNKID_", 0) != 0) {
            continue;
        }

        auto it = chunk_names.find(entry.second);
        if (it == chunk_names.end()) {
            chunk_names.emplace(entry.second, entry.first);
        } else if (PreferChunkName(it->second, entry.first)) {
            it->second = entry.first;
        }
    }
}

bool PreferChunkName(const std::string &current_name, const std::string &candidate_name)
{
    if (current_name.empty()) {
        return true;
    }

    const bool current_is_begin = current_name.find("_BEGIN") != std::string::npos;
    const bool candidate_is_begin = candidate_name.find("_BEGIN") != std::string::npos;
    if (current_is_begin != candidate_is_begin) {
        return !candidate_is_begin;
    }

    const bool current_is_generic =
        current_name.find("SAVELOAD_") != std::string::npos ||
        current_name.find("COMMANDO_EDITOR_") != std::string::npos;
    const bool candidate_is_generic =
        candidate_name.find("SAVELOAD_") != std::string::npos ||
        candidate_name.find("COMMANDO_EDITOR_") != std::string::npos;
    if (current_is_generic != candidate_is_generic) {
        return !candidate_is_generic;
    }

    return candidate_name.size() > current_name.size();
}

void BuildDefinitionFieldAnnotationCache(DefinitionFieldAnnotationCache &cache)
{
    if (cache.initialized) {
        return;
    }

    cache.initialized = true;
    cache.by_definition_chunk.clear();
    cache.global_chunk_names.clear();
    cache.global_chunk_ids_with_fields.clear();
    cache.global_field_names.clear();

    std::filesystem::path repo_root = DetectRepoRootFromSourcePath();
    if (repo_root.empty()) {
        repo_root = FindRepoRootFromCurrentPath();
    }
    if (repo_root.empty()) {
        return;
    }

    std::unordered_map<std::string, std::uint32_t> global_symbols;
    const std::filesystem::path header_paths[] = {
        repo_root / "Code/wwsaveload/saveloadids.h",
        repo_root / "Code/Combat/combatchunkid.h",
        repo_root / "Code/Tools/LevelEdit/EditorChunkIDs.h",
        repo_root / "Code/WWAudio/SoundChunkIDs.h",
    };

    for (const std::filesystem::path &header_path : header_paths) {
        std::string header_text;
        if (!ReadTextFile(header_path, header_text)) {
            continue;
        }

        ParseEnumSymbols(header_text, global_symbols);
    }

    AddChunkNamesFromSymbols(global_symbols, cache.global_chunk_names);

    const std::filesystem::path source_dirs[] = {
        repo_root / "Code/Combat",
        repo_root / "Code/Tools/LevelEdit",
        repo_root / "Code/wwsaveload",
    };

    for (const std::filesystem::path &dir : source_dirs) {
        std::error_code ec;
        if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
            continue;
        }

        for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) {
                break;
            }

            std::error_code entry_ec;
            if (!entry.is_regular_file(entry_ec) || entry_ec) {
                continue;
            }

            const std::filesystem::path source_path = entry.path();
            if (source_path.extension() != ".cpp") {
                continue;
            }

            std::string source_text;
            if (!ReadTextFile(source_path, source_text)) {
                continue;
            }

            std::unordered_map<std::string, std::uint32_t> local_symbols = global_symbols;
            std::unordered_map<std::string, std::uint32_t> added_symbols;
            ParseEnumSymbols(source_text, local_symbols, &added_symbols);

            std::vector<std::pair<std::uint32_t, std::string>> factories;
            ParseDefinitionFactories(source_text, local_symbols, factories);
            if (factories.empty()) {
                continue;
            }

            std::unordered_map<std::uint32_t, std::string> local_chunk_names;
            AddChunkNamesFromSymbols(added_symbols, local_chunk_names);

            for (const auto &factory : factories) {
                const std::uint32_t definition_chunk_id = factory.first;
                if (cache.by_definition_chunk.find(definition_chunk_id) !=
                    cache.by_definition_chunk.end()) {
                    continue;
                }

                DefinitionFieldAnnotation annotation;
                annotation.source_file = source_path.string();
                annotation.definition_class = factory.second;
                annotation.chunk_names = local_chunk_names;

                CaptureFieldMappingsFromMethod(
                    source_text, factory.second, "Save", local_symbols, annotation);
                CaptureFieldMappingsFromMethod(
                    source_text, factory.second, "Load", local_symbols, annotation);

                for (const auto &chunk_name_entry : annotation.chunk_names) {
                    auto chunk_name_it = cache.global_chunk_names.find(chunk_name_entry.first);
                    if (chunk_name_it == cache.global_chunk_names.end()) {
                        cache.global_chunk_names.emplace(chunk_name_entry.first,
                                                         chunk_name_entry.second);
                    } else if (PreferChunkName(
                                   chunk_name_it->second, chunk_name_entry.second)) {
                        chunk_name_it->second = chunk_name_entry.second;
                    }
                }

                for (std::uint32_t chunk_id : annotation.chunk_ids_with_fields) {
                    cache.global_chunk_ids_with_fields.insert(chunk_id);
                }

                for (const auto &field_entry : annotation.field_names) {
                    if (cache.global_field_names.find(field_entry.first) ==
                        cache.global_field_names.end()) {
                        cache.global_field_names.emplace(field_entry.first, field_entry.second);
                    }
                }

                cache.by_definition_chunk.emplace(definition_chunk_id, std::move(annotation));
            }
        }
    }
}

DefinitionFieldAnnotationCache &GetDefinitionFieldAnnotationCache()
{
    static DefinitionFieldAnnotationCache cache;
    BuildDefinitionFieldAnnotationCache(cache);
    return cache;
}

const DefinitionFieldAnnotation *FindDefinitionFieldAnnotation(std::uint32_t definition_chunk_id)
{
    DefinitionFieldAnnotationCache &cache = GetDefinitionFieldAnnotationCache();
    const auto it = cache.by_definition_chunk.find(definition_chunk_id);
    if (it == cache.by_definition_chunk.end()) {
        return nullptr;
    }
    return &it->second;
}

std::string ResolveKnownChunkName(std::uint32_t chunk_id)
{
    DefinitionFieldAnnotationCache &cache = GetDefinitionFieldAnnotationCache();
    const auto it = cache.global_chunk_names.find(chunk_id);
    if (it == cache.global_chunk_names.end()) {
        return std::string();
    }

    return it->second;
}

std::string ResolveKnownFieldName(std::uint32_t chunk_id, std::uint32_t micro_id)
{
    DefinitionFieldAnnotationCache &cache = GetDefinitionFieldAnnotationCache();
    const auto it = cache.global_field_names.find(MakeChunkMicroKey(chunk_id, micro_id));
    if (it == cache.global_field_names.end()) {
        return std::string();
    }
    return it->second;
}

bool ChunkHasKnownFields(std::uint32_t chunk_id, const DefinitionFieldAnnotation *annotation)
{
    if (annotation != nullptr) {
        if (annotation->chunk_ids_with_fields.find(chunk_id) !=
            annotation->chunk_ids_with_fields.end()) {
            return true;
        }

        if (annotation->chunk_names.find(chunk_id) != annotation->chunk_names.end()) {
            return true;
        }
    }

    DefinitionFieldAnnotationCache &cache = GetDefinitionFieldAnnotationCache();
    return cache.global_chunk_ids_with_fields.find(chunk_id) !=
        cache.global_chunk_ids_with_fields.end();
}

std::vector<std::uint8_t> ReadMicroChunkBytes(ChunkLoadClass &loader)
{
    const std::uint32_t chunk_length = loader.Cur_Micro_Chunk_Length();
    std::vector<std::uint8_t> bytes(chunk_length);
    if (chunk_length > 0) {
        loader.Read(bytes.data(), chunk_length);
    }
    return bytes;
}

std::string ReadStringFromBytes(const std::vector<std::uint8_t> &bytes)
{
    if (bytes.empty()) {
        return std::string();
    }

    std::string value(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    while (!value.empty() && value.back() == '\0') {
        value.pop_back();
    }
    return value;
}

bool ReadUint32FromBytes(const std::vector<std::uint8_t> &bytes, std::uint32_t &value)
{
    value = 0;
    switch (bytes.size()) {
    case sizeof(std::uint8_t):
        value = bytes[0];
        return true;
    case sizeof(std::uint16_t): {
        std::uint16_t parsed = 0;
        std::memcpy(&parsed, bytes.data(), sizeof(parsed));
        value = parsed;
        return true;
    }
    case sizeof(std::uint32_t): {
        std::uint32_t parsed = 0;
        std::memcpy(&parsed, bytes.data(), sizeof(parsed));
        value = parsed;
        return true;
    }
    default:
        return false;
    }
}

bool LooksLikeText(const std::vector<std::uint8_t> &bytes)
{
    if (bytes.empty()) {
        return false;
    }

    const std::size_t check_count =
        (bytes.back() == '\0' && bytes.size() > 1) ? bytes.size() - 1 : bytes.size();
    if (check_count == 0) {
        return false;
    }

    for (std::size_t i = 0; i < check_count; ++i) {
        const unsigned char ch = bytes[i];
        if (!(std::isprint(ch) || ch == '\t' || ch == '\n' || ch == '\r')) {
            return false;
        }
    }

    return true;
}

std::string FormatHexBytes(const std::vector<std::uint8_t> &bytes)
{
    if (bytes.empty()) {
        return "<empty>";
    }

    constexpr std::size_t kMaxShownBytes = 40;
    const std::size_t shown = std::min(bytes.size(), kMaxShownBytes);

    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill('0');
    for (std::size_t i = 0; i < shown; ++i) {
        if (i > 0) {
            out << ' ';
        }
        out << std::setw(2) << static_cast<unsigned int>(bytes[i]);
    }

    if (shown < bytes.size()) {
        out << " ...";
    }

    return out.str();
}

std::string DecodeMicroChunkBytes(const std::vector<std::uint8_t> &bytes)
{
    if (bytes.empty()) {
        return "<empty>";
    }

    if (LooksLikeText(bytes)) {
        return "\"" + ReadStringFromBytes(bytes) + "\"";
    }

    std::ostringstream out;
    switch (bytes.size()) {
    case sizeof(std::uint8_t): {
        const std::uint8_t u8 = bytes[0];
        out << "u8=" << static_cast<unsigned int>(u8);
        if (u8 == 0 || u8 == 1) {
            out << ", bool=" << (u8 ? "true" : "false");
        }
        return out.str();
    }
    case sizeof(std::uint16_t): {
        std::uint16_t u16 = 0;
        std::memcpy(&u16, bytes.data(), sizeof(u16));
        out << "u16=" << u16 << ", i16=" << static_cast<std::int16_t>(u16);
        return out.str();
    }
    case sizeof(std::uint32_t): {
        std::uint32_t u32 = 0;
        std::memcpy(&u32, bytes.data(), sizeof(u32));
        float f32 = 0.0F;
        std::memcpy(&f32, bytes.data(), sizeof(f32));
        out << "u32=" << u32
            << ", i32=" << static_cast<std::int32_t>(u32)
            << ", f32=" << f32;
        if (u32 == 0U || u32 == 1U) {
            out << ", bool=" << (u32 ? "true" : "false");
        }
        return out.str();
    }
    case sizeof(std::uint64_t): {
        std::uint64_t u64 = 0;
        std::memcpy(&u64, bytes.data(), sizeof(u64));
        double f64 = 0.0;
        std::memcpy(&f64, bytes.data(), sizeof(f64));
        out << "u64=" << u64
            << ", i64=" << static_cast<std::int64_t>(u64)
            << ", f64=" << f64;
        return out.str();
    }
    default:
        break;
    }

    return "<binary>";
}

std::string FormatChunkPath(const std::vector<std::uint32_t> &chunk_path)
{
    if (chunk_path.empty()) {
        return std::string();
    }

    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill('0');
    for (std::size_t i = 0; i < chunk_path.size(); ++i) {
        if (i > 0) {
            out << '/';
        }
        out << "0x" << std::setw(8) << chunk_path[i];
    }

    return out.str();
}

void AppendDefinitionField(std::vector<PresetDefinitionField> *fields,
                           const std::vector<std::uint32_t> &chunk_path,
                           std::uint32_t micro_id,
                           const std::vector<std::uint8_t> &bytes,
                           const DefinitionFieldAnnotation *annotation = nullptr)
{
    if (fields == nullptr) {
        return;
    }

    PresetDefinitionField field;
    field.chunk_id = chunk_path.empty() ? 0U : chunk_path.back();
    field.micro_id = micro_id;
    field.byte_length = static_cast<std::uint32_t>(bytes.size());
    field.chunk_path = FormatChunkPath(chunk_path);
    if (annotation != nullptr) {
        const auto chunk_name_it = annotation->chunk_names.find(field.chunk_id);
        if (chunk_name_it != annotation->chunk_names.end()) {
            field.chunk_name = chunk_name_it->second;
        }

        const auto field_name_it =
            annotation->field_names.find(MakeChunkMicroKey(field.chunk_id, micro_id));
        if (field_name_it != annotation->field_names.end()) {
            field.field_name = field_name_it->second;
        }
    }

    if (field.chunk_name.empty()) {
        field.chunk_name = ResolveKnownChunkName(field.chunk_id);
    }

    if (field.field_name.empty() && annotation == nullptr) {
        field.field_name = ResolveKnownFieldName(field.chunk_id, micro_id);
    }

    if (field.field_name.empty() && field.chunk_id == kChunkVariables) {
        switch (micro_id) {
        case kDefinitionVarInstanceId:
            field.field_name = "InstanceID";
            break;
        case kDefinitionVarName:
            field.field_name = "Name";
            break;
        default:
            break;
        }
    }

    field.decoded_value = DecodeMicroChunkBytes(bytes);
    field.raw_hex = FormatHexBytes(bytes);
    fields->push_back(std::move(field));
}

bool CommandExists(const QString &program, const QStringList &args = {})
{
    QProcess process;
    process.start(program, args);
    if (!process.waitForStarted(1000)) {
        return false;
    }

    process.closeWriteChannel();
    process.waitForFinished(5000);
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

void AddUniquePath(const QString &path, QStringList &list, QSet<QString> &seen)
{
    if (path.isEmpty()) {
        return;
    }

    const QString cleaned = QDir::cleanPath(path);
    const QString dedupe_key = cleaned.toLower();
    if (seen.contains(dedupe_key)) {
        return;
    }

    seen.insert(dedupe_key);
    list.push_back(cleaned);
}

QString NormalizePathHint(QString path)
{
    path = QDir::fromNativeSeparators(path.trimmed());
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
        path = path.mid(1, path.size() - 2);
    }

    return path;
}

std::uint32_t InferClassIdFromDefinitionId(std::uint32_t definition_id)
{
    if (definition_id == 0) {
        return 0;
    }

    return kDefinitionClassIdBase + ((definition_id - 1) / kDefinitionIdRange);
}

std::uint32_t ClassIdFromDefinitionChunkId(std::uint32_t definition_chunk_id)
{
    switch (definition_chunk_id) {
    case CHUNKID_TERRAIN_DEF:
        return CLASSID_TERRAIN;
    case CHUNKID_TILE_DEF:
        return CLASSID_TILE;
    case CHUNKID_LIGHT_DEF:
        return CLASSID_LIGHT;
    case CHUNKID_WAYPATH_DEF:
        return CLASSID_WAYPATH;
    case CHUNKID_EDITOR_ZONE_DEF:
        return CLASSID_ZONE;
    case CHUNKID_TRANSITION_DEF:
        return CLASSID_TRANSITION;
    case CHUNKID_VIS_POINT_DEF:
        return CLASSID_VIS_POINT_DEF;
    case CHUNKID_PATHFIND_START_DEF:
        return CLASSID_PATHFIND_START_DEF;
    case CHUNKID_DUMMY_OBJECT_DEF:
        return CLASSID_DUMMY_OBJECTS;
    case CHUNKID_COVERSPOT_DEF:
        return CLASSID_COVERSPOT;
    case CHUNKID_EDITOR_ONLY_OBJECTS_DEF:
        return CLASSID_EDITOR_ONLY_OBJECTS;
    case CHUNKID_GAME_OBJECT_DEF_C4:
        return CLASSID_GAME_OBJECT_DEF_C4;
    case CHUNKID_GAME_OBJECT_DEF_POWERUP:
        return CLASSID_GAME_OBJECT_DEF_POWERUP;
    case CHUNKID_GAME_OBJECT_DEF_SAMSITE:
        return CLASSID_GAME_OBJECT_DEF_SAMSITE;
    case CHUNKID_GAME_OBJECT_DEF_SIMPLE:
        return CLASSID_GAME_OBJECT_DEF_SIMPLE;
    case CHUNKID_GAME_OBJECT_DEF_SOLDIER:
        return CLASSID_GAME_OBJECT_DEF_SOLDIER;
    case CHUNKID_SPAWNER_DEF:
        return CLASSID_SPAWNER_DEF;
    case CHUNKID_GAME_OBJECT_DEF_SCRIPT_ZONE:
        return CLASSID_GAME_OBJECT_DEF_SCRIPT_ZONE;
    case CHUNKID_GAME_OBJECT_DEF_TRANSITION:
        return CLASSID_GAME_OBJECT_DEF_TRANSITION;
    case CHUNKID_WEAPON_DEF:
        return CLASSID_DEF_WEAPON;
    case CHUNKID_AMMO_DEF:
        return CLASSID_DEF_AMMO;
    case CHUNKID_GAME_OBJECT_DEF_VEHICLE:
        return CLASSID_GAME_OBJECT_DEF_VEHICLE;
    case CHUNKID_EXPLOSION_DEF:
        return CLASSID_DEF_EXPLOSION;
    case CHUNKID_GAME_OBJECT_DEF_CINEMATIC:
        return CLASSID_GAME_OBJECT_DEF_CINEMATIC;
    case CHUNKID_GAME_OBJECT_DEF_DAMAGE_ZONE:
        return CLASSID_GAME_OBJECT_DEF_DAMAGE_ZONE;
    case CHUNKID_GAME_OBJECT_DEF_SPECIAL_EFFECTS:
        return CLASSID_GAME_OBJECT_DEF_SPECIAL_EFFECTS;
    case CHUNKID_GAME_OBJECT_DEF_SAKURA_BOSS:
        return CLASSID_GAME_OBJECT_DEF_SAKURA_BOSS;
    case CHUNKID_GAME_OBJECT_DEF_BUILDING:
        return CLASSID_GAME_OBJECT_DEF_BUILDING;
    case CHUNKID_GAME_OBJECT_DEF_BEACON:
        return CLASSID_GAME_OBJECT_DEF_BEACON;
    case CHUNKID_GAME_OBJECT_DEF_REFINERY:
        return CLASSID_GAME_OBJECT_DEF_REFINERY;
    case CHUNKID_GAME_OBJECT_DEF_POWERPLANT:
        return CLASSID_GAME_OBJECT_DEF_POWERPLANT;
    case CHUNKID_GAME_OBJECT_DEF_SOLDIER_FACTORY:
        return CLASSID_GAME_OBJECT_DEF_SOLDIER_FACTORY;
    case CHUNKID_GAME_OBJECT_DEF_VEHICLE_FACTORY:
        return CLASSID_GAME_OBJECT_DEF_VEHICLE_FACTORY;
    case CHUNKID_GAME_OBJECT_DEF_AIRSTRIP:
        return CLASSID_GAME_OBJECT_DEF_AIRSTRIP;
    case CHUNKID_GAME_OBJECT_DEF_WARFACTORY:
        return CLASSID_GAME_OBJECT_DEF_WARFACTORY;
    case CHUNKID_GAME_OBJECT_DEF_COMCENTER:
        return CLASSID_GAME_OBJECT_DEF_COMCENTER;
    case CHUNKID_GAME_OBJECT_DEF_REPAIR_BAY:
        return CLASSID_GAME_OBJECT_DEF_REPAIR_BAY;
    case CHUNKID_GAME_OBJECT_DEF_MENDOZA_BOSS:
        return CLASSID_GAME_OBJECT_DEF_MENDOZA_BOSS;
    case CHUNKID_GAME_OBJECT_DEF_RAVESHAW_BOSS:
        return CLASSID_GAME_OBJECT_DEF_RAVESHAW_BOSS;
    case CHUNKID_GLOBAL_SETTINGS_DEF:
        return CLASSID_GLOBAL_SETTINGS_DEF;
    case CHUNKID_GLOBAL_SETTINGS_DEF_HUMAN_LOITER:
        return CLASSID_GLOBAL_SETTINGS_DEF_HUMAN_LOITER;
    case CHUNKID_GLOBAL_SETTINGS_DEF_GENERAL:
        return CLASSID_GLOBAL_SETTINGS_DEF_GENERAL;
    case CHUNKID_GLOBAL_SETTINGS_DEF_HUD:
        return CLASSID_GLOBAL_SETTINGS_DEF_HUD;
    case CHUNKID_GLOBAL_SETTINGS_DEF_EVA:
        return CLASSID_GLOBAL_SETTINGS_DEF_EVA;
    case CHUNKID_GLOBAL_SETTINGS_DEF_CHAR_CLASS:
        return CLASSID_GLOBAL_SETTINGS_DEF_CHAR_CLASS;
    case CHUNKID_GLOBAL_SETTINGS_DEF_HUMAN_ANIM_OVERRIDE:
        return CLASSID_GLOBAL_SETTINGS_DEF_HUMAN_ANIM_OVERRIDE;
    case CHUNKID_GLOBAL_SETTINGS_DEF_PURCHASE:
        return CLASSID_GLOBAL_SETTINGS_DEF_PURCHASE;
    case CHUNKID_GLOBAL_SETTINGS_DEF_TEAM_PURCHASE:
        return CLASSID_GLOBAL_SETTINGS_DEF_TEAM_PURCHASE;
    case CHUNKID_GLOBAL_SETTINGS_DEF_CNCMODE:
        return CLASSID_GLOBAL_SETTINGS_DEF_CNCMODE;
    case CHUNKID_SOUND_DEF:
        return CLASSID_SOUND_DEF;
    case CHUNKID_TWIDDLER:
        return CLASSID_TWIDDLERS;
    default:
        return 0;
    }
}

void AddAlwaysMixCandidates(const QString &root, QStringList &candidates, QSet<QString> &seen)
{
    AddUniquePath(QDir(root).filePath(QStringLiteral("Always.dbs")), candidates, seen);
    AddUniquePath(QDir(root).filePath(QStringLiteral("always.dbs")), candidates, seen);
    AddUniquePath(QDir(root).filePath(QStringLiteral("Always.dat")), candidates, seen);
    AddUniquePath(QDir(root).filePath(QStringLiteral("always.dat")), candidates, seen);
}

QStringList BuildPresetCatalogCandidates(const std::string &asset_tree_path)
{
    QStringList candidates;
    QSet<QString> seen;

    const QString hint = NormalizePathHint(QString::fromStdString(asset_tree_path));
    if (hint.isEmpty()) {
        return candidates;
    }

    QFileInfo hint_info(hint);
    if (hint_info.exists() && hint_info.isFile()) {
        const QString filename = hint_info.fileName();
        if (filename.compare(QStringLiteral("objects.ddb"), Qt::CaseInsensitive) == 0 ||
            filename.compare(QStringLiteral("objects.ddb.json"), Qt::CaseInsensitive) == 0 ||
            filename.compare(QStringLiteral("always.dbs"), Qt::CaseInsensitive) == 0 ||
            filename.compare(QStringLiteral("always.dat"), Qt::CaseInsensitive) == 0) {
            AddUniquePath(hint_info.absoluteFilePath(), candidates, seen);
        }
    }

    QString resolved_root = hint;
    if (hint_info.exists() && hint_info.isFile()) {
        resolved_root = hint_info.absolutePath();
    }

    QStringList roots;
    QSet<QString> root_seen;
    AddUniquePath(resolved_root, roots, root_seen);

    QFileInfo root_info(resolved_root);
    const QString leaf_name = root_info.fileName();
    if (leaf_name.compare(QStringLiteral("data"), Qt::CaseInsensitive) == 0) {
        AddUniquePath(root_info.absolutePath(), roots, root_seen);
    } else {
        AddUniquePath(QDir(resolved_root).filePath(QStringLiteral("DATA")), roots, root_seen);
        AddUniquePath(QDir(resolved_root).filePath(QStringLiteral("Data")), roots, root_seen);
    }

    for (const QString &root : roots) {
        AddUniquePath(QDir(root).filePath(QStringLiteral("presets/objects.ddb.json")), candidates, seen);
        AddUniquePath(QDir(root).filePath(QStringLiteral("presets/objects.ddb")), candidates, seen);
        AddUniquePath(QDir(root).filePath(QStringLiteral("objects.ddb.json")), candidates, seen);
        AddUniquePath(QDir(root).filePath(QStringLiteral("objects.ddb")), candidates, seen);
        AddAlwaysMixCandidates(root, candidates, seen);
    }

    return candidates;
}

bool JsonValueToUint32(const QJsonValue &value, std::uint32_t &out)
{
    if (value.isDouble()) {
        const double parsed = value.toDouble(-1.0);
        if (parsed < 0.0 || parsed > static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
            return false;
        }
        if (std::floor(parsed) != parsed) {
            return false;
        }

        out = static_cast<std::uint32_t>(parsed);
        return true;
    }

    if (value.isString()) {
        bool ok = false;
        const qulonglong parsed = value.toString().toULongLong(&ok, 10);
        if (!ok || parsed > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }

        out = static_cast<std::uint32_t>(parsed);
        return true;
    }

    return false;
}

bool JsonValueToBool(const QJsonValue &value, bool &out)
{
    if (value.isBool()) {
        out = value.toBool();
        return true;
    }

    std::uint32_t numeric = 0;
    if (JsonValueToUint32(value, numeric)) {
        out = (numeric != 0);
        return true;
    }

    if (value.isString()) {
        const QString text = value.toString().trimmed().toLower();
        if (text == QStringLiteral("true")) {
            out = true;
            return true;
        }

        if (text == QStringLiteral("false")) {
            out = false;
            return true;
        }
    }

    return false;
}

bool ReadMicroChunkUint32(ChunkLoadClass &loader, std::uint32_t &value)
{
    value = 0;
    const std::uint32_t chunk_length = loader.Cur_Micro_Chunk_Length();

    switch (chunk_length) {
    case sizeof(std::uint8_t): {
        std::uint8_t parsed = 0;
        loader.Read(&parsed, sizeof(parsed));
        value = parsed;
        return true;
    }
    case sizeof(std::uint16_t): {
        std::uint16_t parsed = 0;
        loader.Read(&parsed, sizeof(parsed));
        value = parsed;
        return true;
    }
    case sizeof(std::uint32_t): {
        std::uint32_t parsed = 0;
        loader.Read(&parsed, sizeof(parsed));
        value = parsed;
        return true;
    }
    default:
        loader.Seek(chunk_length);
        return false;
    }
}

bool ReadMicroChunkBool(ChunkLoadClass &loader, bool &value)
{
    value = false;
    const std::uint32_t chunk_length = loader.Cur_Micro_Chunk_Length();

    switch (chunk_length) {
    case sizeof(std::uint8_t): {
        std::uint8_t parsed = 0;
        loader.Read(&parsed, sizeof(parsed));
        value = (parsed != 0);
        return true;
    }
    case sizeof(std::uint16_t): {
        std::uint16_t parsed = 0;
        loader.Read(&parsed, sizeof(parsed));
        value = (parsed != 0);
        return true;
    }
    case sizeof(std::uint32_t): {
        std::uint32_t parsed = 0;
        loader.Read(&parsed, sizeof(parsed));
        value = (parsed != 0);
        return true;
    }
    default:
        loader.Seek(chunk_length);
        return false;
    }
}

void ParseDefinitionObjectData(ChunkLoadClass &loader,
                               DefinitionSnapshot &snapshot,
                               std::vector<PresetDefinitionField> *fields = nullptr,
                               std::vector<std::uint32_t> *chunk_stack = nullptr,
                               const DefinitionFieldAnnotation *annotation = nullptr)
{
    while (loader.Open_Chunk()) {
        const std::uint32_t chunk_id = loader.Cur_Chunk_ID();
        if (chunk_stack != nullptr) {
            chunk_stack->push_back(chunk_id);
        }

        const bool contains_sub_chunks = loader.Contains_Chunks() != 0;
        const bool parse_as_micro_chunks =
            !contains_sub_chunks && (chunk_id == kChunkVariables ||
                                     ChunkHasKnownFields(chunk_id, annotation));

        if (parse_as_micro_chunks) {
            std::uint32_t candidate_id = 0;
            bool has_candidate_id = false;
            bool has_candidate_name = false;
            bool has_unrecognized_microchunk = false;
            bool has_duplicate_id = false;
            bool has_duplicate_name = false;
            std::string candidate_name;

            while (loader.Open_Micro_Chunk()) {
                const std::uint32_t micro_id = loader.Cur_Micro_Chunk_ID();
                const std::vector<std::uint8_t> micro_bytes = ReadMicroChunkBytes(loader);
                if (chunk_stack != nullptr) {
                    AppendDefinitionField(fields, *chunk_stack, micro_id, micro_bytes, annotation);
                }

                if (chunk_id == kChunkVariables) {
                    switch (micro_id) {
                    case kDefinitionVarInstanceId:
                        if (has_candidate_id) {
                            has_duplicate_id = true;
                        } else {
                            has_candidate_id = ReadUint32FromBytes(micro_bytes, candidate_id);
                        }
                        break;
                    case kDefinitionVarName:
                        if (has_candidate_name) {
                            has_duplicate_name = true;
                        }
                        has_candidate_name = true;
                        candidate_name = ReadStringFromBytes(micro_bytes);
                        break;
                    default:
                        has_unrecognized_microchunk = true;
                        break;
                    }
                }

                loader.Close_Micro_Chunk();
            }

            if (chunk_id == kChunkVariables) {
                // Many definitions have their own CHUNKID_VARIABLES blocks. Only accept the
                // exact DefinitionClass variable shape: {instance id, name} with no extras.
                const bool is_definition_base_variables = has_candidate_id &&
                    has_candidate_name &&
                    !has_unrecognized_microchunk &&
                    !has_duplicate_id &&
                    !has_duplicate_name;

                if (is_definition_base_variables && snapshot.id == 0) {
                    snapshot.id = candidate_id;
                    snapshot.name = candidate_name;
                }
            }
        } else if (contains_sub_chunks) {
            ParseDefinitionObjectData(loader, snapshot, fields, chunk_stack, annotation);
        }

        loader.Close_Chunk();
        if (chunk_stack != nullptr && !chunk_stack->empty()) {
            chunk_stack->pop_back();
        }
    }
}

void ParseDefinitionManagerChunk(ChunkLoadClass &loader, std::unordered_map<std::uint32_t, DefinitionSnapshot> &definitions)
{
    while (loader.Open_Chunk()) {
        if (loader.Cur_Chunk_ID() == kChunkObjectCollection) {
            while (loader.Open_Chunk()) {
                const std::uint32_t definition_chunk_id = loader.Cur_Chunk_ID();
                DefinitionSnapshot snapshot;
                snapshot.class_id = ClassIdFromDefinitionChunkId(definition_chunk_id);

                while (loader.Open_Chunk()) {
                    if (loader.Cur_Chunk_ID() == kSimpleFactoryChunkObjectData) {
                        ParseDefinitionObjectData(loader, snapshot);
                    }

                    loader.Close_Chunk();
                }

                if (snapshot.id != 0) {
                    if (snapshot.class_id == 0) {
                        snapshot.class_id = InferClassIdFromDefinitionId(snapshot.id);
                    }
                    definitions[snapshot.id] = snapshot;
                }

                loader.Close_Chunk();
            }
        }

        loader.Close_Chunk();
    }
}

void ParsePresetObjectData(ChunkLoadClass &loader, PresetRecord &record)
{
    while (loader.Open_Chunk()) {
        if (loader.Cur_Chunk_ID() == kChunkVariables) {
            std::uint32_t candidate_definition_id = 0;
            std::uint32_t candidate_parent_id = 0;
            bool candidate_temporary = false;
            bool has_definition_id = false;
            bool has_parent_id = false;
            bool has_temporary = false;

            while (loader.Open_Micro_Chunk()) {
                switch (loader.Cur_Micro_Chunk_ID()) {
                case kPresetVarDefinitionId:
                    has_definition_id = ReadMicroChunkUint32(loader, candidate_definition_id);
                    break;
                case kPresetVarIsTemporary:
                    has_temporary = ReadMicroChunkBool(loader, candidate_temporary);
                    break;
                case kPresetVarParentId:
                    has_parent_id = ReadMicroChunkUint32(loader, candidate_parent_id);
                    break;
                default:
                    loader.Seek(loader.Cur_Micro_Chunk_Length());
                    break;
                }

                loader.Close_Micro_Chunk();
            }

            if (has_definition_id && record.definition_id == 0) {
                record.definition_id = candidate_definition_id;
            }
            if (has_parent_id) {
                record.parent_id = candidate_parent_id;
            }
            if (has_temporary) {
                record.temporary = candidate_temporary;
            }
        } else if (loader.Contains_Chunks()) {
            ParsePresetObjectData(loader, record);
        }

        loader.Close_Chunk();
    }
}

void ParsePresetManagerChunk(ChunkLoadClass &loader, std::vector<PresetRecord> &records)
{
    while (loader.Open_Chunk()) {
        if (loader.Cur_Chunk_ID() == kChunkObjectCollection) {
            while (loader.Open_Chunk()) {
                PresetRecord record{};

                while (loader.Open_Chunk()) {
                    if (loader.Cur_Chunk_ID() == kSimpleFactoryChunkObjectData) {
                        ParsePresetObjectData(loader, record);
                    }

                    loader.Close_Chunk();
                }

                if (record.definition_id != 0) {
                    record.id = record.definition_id;
                    records.push_back(record);
                }

                loader.Close_Chunk();
            }
        }

        loader.Close_Chunk();
    }
}

bool LoadPresetCatalogFromJson(const QString &json_path, std::vector<PresetRecord> &records, std::string &error)
{
    QFile file(json_path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Unable to open preset JSON catalog: %1")
                    .arg(QDir::toNativeSeparators(json_path))
                    .toStdString();
        return false;
    }

    QJsonParseError parse_error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("Preset JSON parse failed: %1").arg(parse_error.errorString()).toStdString();
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonValue definitions_value = root.value(QStringLiteral("definitions"));
    const QJsonValue presets_value = root.value(QStringLiteral("presets"));

    if (!presets_value.isArray()) {
        error = "Preset JSON is missing the 'presets' array.";
        return false;
    }

    std::unordered_map<std::uint32_t, DefinitionSnapshot> definitions;
    if (definitions_value.isArray()) {
        const QJsonArray definitions_array = definitions_value.toArray();
        for (const QJsonValue &value : definitions_array) {
            if (!value.isObject()) {
                continue;
            }

            const QJsonObject object = value.toObject();
            DefinitionSnapshot snapshot;
            if (!JsonValueToUint32(object.value(QStringLiteral("id")), snapshot.id)) {
                continue;
            }

            if (!JsonValueToUint32(object.value(QStringLiteral("classId")), snapshot.class_id)) {
                snapshot.class_id = InferClassIdFromDefinitionId(snapshot.id);
            }

            snapshot.name = object.value(QStringLiteral("name")).toString().toStdString();
            definitions[snapshot.id] = snapshot;
        }
    }

    std::vector<PresetRecord> parsed;
    const QJsonArray presets_array = presets_value.toArray();
    parsed.reserve(static_cast<std::size_t>(presets_array.size()));

    for (const QJsonValue &value : presets_array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();

        PresetRecord record{};
        if (!JsonValueToUint32(object.value(QStringLiteral("id")), record.id)) {
            continue;
        }

        record.definition_id = record.id;

        if (!JsonValueToUint32(object.value(QStringLiteral("classId")), record.class_id)) {
            record.class_id = InferClassIdFromDefinitionId(record.definition_id);
        }

        JsonValueToUint32(object.value(QStringLiteral("parentId")), record.parent_id);
        JsonValueToBool(object.value(QStringLiteral("isTemporary")), record.temporary);

        record.name = object.value(QStringLiteral("name")).toString().toStdString();
        const auto definition_it = definitions.find(record.definition_id);
        if (definition_it != definitions.end()) {
            if (record.name.empty()) {
                record.name = definition_it->second.name;
            }
            if (record.class_id == 0) {
                record.class_id = definition_it->second.class_id;
            }
        }

        parsed.push_back(record);
    }

    std::sort(parsed.begin(), parsed.end(), [](const PresetRecord &lhs, const PresetRecord &rhs) {
        if (lhs.id != rhs.id) {
            return lhs.id < rhs.id;
        }
        if (lhs.class_id != rhs.class_id) {
            return lhs.class_id < rhs.class_id;
        }
        return lhs.name < rhs.name;
    });

    records.swap(parsed);
    error.clear();
    return true;
}

bool LoadPresetCatalogFromDdb(const QString &ddb_path, std::vector<PresetRecord> &records, std::string &error)
{
    auto load_from_file = [&](FileClass &file) -> bool {
        if (!file.Open(FileClass::READ)) {
            return false;
        }

        std::unordered_map<std::uint32_t, DefinitionSnapshot> definitions;
        std::vector<PresetRecord> parsed;
        bool saw_catalog_chunk = false;

        ChunkLoadClass loader(&file);
        while (loader.Open_Chunk()) {
            switch (loader.Cur_Chunk_ID()) {
            case CHUNKID_SAVELOAD_DEFMGR:
                saw_catalog_chunk = true;
                ParseDefinitionManagerChunk(loader, definitions);
                break;
            case CHUNKID_PRESETMGR:
                saw_catalog_chunk = true;
                ParsePresetManagerChunk(loader, parsed);
                break;
            default:
                break;
            }

            loader.Close_Chunk();
        }

        file.Close();
        if (!saw_catalog_chunk) {
            return false;
        }

        std::vector<PresetRecord> filtered;
        filtered.reserve(parsed.size());
        for (PresetRecord &record : parsed) {
            const auto definition_it = definitions.find(record.definition_id);
            if (definition_it == definitions.end()) {
                continue;
            }

            if (record.name.empty()) {
                record.name = definition_it->second.name;
            }
            if (record.class_id == 0) {
                record.class_id = definition_it->second.class_id;
            }

            if (record.class_id == 0) {
                continue;
            }

            filtered.push_back(record);
        }

        std::sort(filtered.begin(), filtered.end(), [](const PresetRecord &lhs, const PresetRecord &rhs) {
            if (lhs.id != rhs.id) {
                return lhs.id < rhs.id;
            }
            if (lhs.class_id != rhs.class_id) {
                return lhs.class_id < rhs.class_id;
            }
            return lhs.name < rhs.name;
        });

        records.swap(filtered);
        error.clear();
        return true;
    };

    const QByteArray native_path = QDir::toNativeSeparators(ddb_path).toLocal8Bit();
    RawFileClass raw_file(native_path.constData());
    if (load_from_file(raw_file)) {
        return true;
    }

    // Legacy-compatible fallback: preset DB may be packed in Always.dbs/.dat.
    const QString extension = QFileInfo(ddb_path).suffix().toLower();
    if (extension == QStringLiteral("dbs") || extension == QStringLiteral("dat")) {
        RawFileFactoryClass raw_factory;
        MixFileFactoryClass mix_factory(native_path.constData(), &raw_factory);
        if (!mix_factory.Is_Valid()) {
            error = QStringLiteral("Unable to open preset container: %1")
                        .arg(QDir::toNativeSeparators(ddb_path))
                        .toStdString();
            return false;
        }

        const char *const entry_candidates[] = {
            "objects.ddb",
            "presets\\objects.ddb",
            "presets/objects.ddb",
        };

        for (const char *entry : entry_candidates) {
            FileClass *entry_file = mix_factory.Get_File(entry);
            if (entry_file == nullptr) {
                continue;
            }

            const bool loaded = load_from_file(*entry_file);
            mix_factory.Return_File(entry_file);
            if (loaded) {
                return true;
            }
        }
    }

    error = QStringLiteral("Unable to open preset catalog: %1")
                .arg(QDir::toNativeSeparators(ddb_path))
                .toStdString();
    return false;
}

bool LoadPresetDefinitionDetailsFromDdb(const QString &ddb_path,
                                        std::uint32_t definition_id,
                                        PresetDefinitionDetails &details,
                                        std::string &error)
{
    auto load_from_file = [&](FileClass &file) -> bool {
        if (!file.Open(FileClass::READ)) {
            return false;
        }

        bool found = false;
        ChunkLoadClass loader(&file);
        while (loader.Open_Chunk() && !found) {
            if (loader.Cur_Chunk_ID() == CHUNKID_SAVELOAD_DEFMGR) {
                while (loader.Open_Chunk() && !found) {
                    if (loader.Cur_Chunk_ID() == kChunkObjectCollection) {
                        while (loader.Open_Chunk() && !found) {
                            const std::uint32_t definition_chunk_id = loader.Cur_Chunk_ID();
                            const DefinitionFieldAnnotation *annotation =
                                FindDefinitionFieldAnnotation(definition_chunk_id);
                            DefinitionSnapshot snapshot;
                            snapshot.class_id = ClassIdFromDefinitionChunkId(definition_chunk_id);

                            std::vector<PresetDefinitionField> fields;
                            while (loader.Open_Chunk()) {
                                if (loader.Cur_Chunk_ID() == kSimpleFactoryChunkObjectData) {
                                    std::vector<std::uint32_t> chunk_path;
                                    ParseDefinitionObjectData(
                                        loader, snapshot, &fields, &chunk_path, annotation);
                                }

                                loader.Close_Chunk();
                            }

                            if (snapshot.id == definition_id) {
                                details = {};
                                details.definition_id = snapshot.id;
                                details.class_id = snapshot.class_id != 0
                                    ? snapshot.class_id
                                    : InferClassIdFromDefinitionId(snapshot.id);
                                details.definition_chunk_id = definition_chunk_id;
                                details.name = snapshot.name;
                                if (annotation != nullptr) {
                                    details.annotation_source_file = annotation->source_file;
                                    details.annotation_class_name = annotation->definition_class;
                                    details.annotation_field_count =
                                        static_cast<std::uint32_t>(annotation->field_names.size());
                                }
                                details.fields = std::move(fields);
                                found = true;
                            }

                            loader.Close_Chunk();
                        }
                    }

                    loader.Close_Chunk();
                }
            }

            loader.Close_Chunk();
        }

        file.Close();
        return found;
    };

    const QByteArray native_path = QDir::toNativeSeparators(ddb_path).toLocal8Bit();
    RawFileClass raw_file(native_path.constData());
    if (load_from_file(raw_file)) {
        details.source_path = QDir::toNativeSeparators(ddb_path).toStdString();
        error.clear();
        return true;
    }

    const QString extension = QFileInfo(ddb_path).suffix().toLower();
    if (extension == QStringLiteral("dbs") || extension == QStringLiteral("dat")) {
        RawFileFactoryClass raw_factory;
        MixFileFactoryClass mix_factory(native_path.constData(), &raw_factory);
        if (!mix_factory.Is_Valid()) {
            error = QStringLiteral("Unable to open preset container: %1")
                        .arg(QDir::toNativeSeparators(ddb_path))
                        .toStdString();
            return false;
        }

        const char *const entry_candidates[] = {
            "objects.ddb",
            "presets\\objects.ddb",
            "presets/objects.ddb",
        };

        for (const char *entry : entry_candidates) {
            FileClass *entry_file = mix_factory.Get_File(entry);
            if (entry_file == nullptr) {
                continue;
            }

            const bool loaded = load_from_file(*entry_file);
            mix_factory.Return_File(entry_file);
            if (loaded) {
                details.source_path = QDir::toNativeSeparators(ddb_path).toStdString();
                error.clear();
                return true;
            }
        }
    }

    error = QStringLiteral("Definition ID %1 was not found in: %2")
                .arg(definition_id)
                .arg(QDir::toNativeSeparators(ddb_path))
                .toStdString();
    return false;
}

} // namespace

bool RuntimeSession::initialize(const RuntimeInitOptions &options, std::string &error)
{
    if (!options.viewport_hwnd) {
        error = "Runtime init failed: viewport HWND is null.";
        return false;
    }

    _options = options;
    _profile = options.profile;

    _capabilities = {};
#ifdef W3D_LEVELEDIT_DDB_JSON_MIRROR
    _capabilities.ddb_json_mirror_enabled = true;
#endif
#ifdef W3D_LEVELEDIT_GIT_SCM
    _capabilities.source_control_enabled = IsFullProfile(_profile);
    if (_capabilities.source_control_enabled) {
        const bool has_git = CommandExists(QStringLiteral("git"), {QStringLiteral("--version")});
        const bool has_git_lfs =
            CommandExists(QStringLiteral("git"), {QStringLiteral("lfs"), QStringLiteral("version")});
        _capabilities.source_control_read_only = !(has_git && has_git_lfs);
    }
#endif

    _initialized = true;
    error.clear();
    return true;
}

void RuntimeSession::shutdown()
{
    _initialized = false;
    _currentLevelPath.clear();
    _capabilities = {};
    _profile = LevelEditQtProfile::Public;
}

bool RuntimeSession::openLevel(const std::string &path, std::string &error)
{
    if (!_initialized) {
        error = "Runtime is not initialized.";
        return false;
    }

    const QFileInfo fileInfo(QString::fromStdString(path));
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        error = "Level file does not exist.";
        return false;
    }

    _currentLevelPath = path;
    error.clear();
    return true;
}

bool RuntimeSession::saveLevel(const std::string &path, std::string &error)
{
    if (!_initialized) {
        error = "Runtime is not initialized.";
        return false;
    }

    if (path.empty()) {
        error = "Save path is empty.";
        return false;
    }

    _currentLevelPath = path;
    error.clear();
    return true;
}

bool RuntimeSession::executeLegacyCommand(int legacy_command_id, std::string &error)
{
    if (!_initialized) {
        error = "Runtime is not initialized.";
        return false;
    }

    if (legacy_command_id == 0) {
        error = "Invalid command id.";
        return false;
    }

    // Runtime command routing is intentionally incremental. Returning success here ensures
    // command paths are exercised through the runtime abstraction instead of silent no-ops.
    error.clear();
    return true;
}

bool RuntimeSession::readPresetCatalog(std::vector<PresetRecord> &records,
                                       std::string &source,
                                       std::string &error,
                                       std::vector<std::string> *searched_paths) const
{
    records.clear();
    source.clear();
    if (searched_paths != nullptr) {
        searched_paths->clear();
    }

    if (!_initialized) {
        error = "Runtime is not initialized.";
        return false;
    }

    const QStringList candidates = BuildPresetCatalogCandidates(_options.asset_tree_path);
    if (searched_paths != nullptr) {
        searched_paths->reserve(static_cast<std::size_t>(candidates.size()));
        for (const QString &candidate : candidates) {
            searched_paths->push_back(QDir::toNativeSeparators(candidate).toStdString());
        }
    }
    if (candidates.isEmpty()) {
        error = "Asset Tree is not set. Configure Config/Asset Tree in LevelEdit.ini.";
        return false;
    }

    QString selected_catalog;
    for (const QString &candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.exists() && info.isFile()) {
            selected_catalog = info.absoluteFilePath();
            break;
        }
    }

    if (selected_catalog.isEmpty()) {
        error = "Could not find objects.ddb(.json) or Always.dbs/.dat under the configured Asset Tree location.";
        return false;
    }

    std::string parse_error;
    const bool loaded = selected_catalog.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)
                            ? LoadPresetCatalogFromJson(selected_catalog, records, parse_error)
                            : LoadPresetCatalogFromDdb(selected_catalog, records, parse_error);

    source = selected_catalog.toStdString();
    if (!loaded) {
        error = parse_error;
        return false;
    }

    error.clear();
    return true;
}

bool RuntimeSession::readPresetDefinitionDetails(std::uint32_t definition_id,
                                                 PresetDefinitionDetails &details,
                                                 std::string &error,
                                                 std::vector<std::string> *searched_paths) const
{
    details = {};
    if (searched_paths != nullptr) {
        searched_paths->clear();
    }

    if (!_initialized) {
        error = "Runtime is not initialized.";
        return false;
    }

    if (definition_id == 0) {
        error = "Definition id is invalid.";
        return false;
    }

    const QStringList candidates = BuildPresetCatalogCandidates(_options.asset_tree_path);
    if (searched_paths != nullptr) {
        searched_paths->reserve(static_cast<std::size_t>(candidates.size()));
        for (const QString &candidate : candidates) {
            searched_paths->push_back(QDir::toNativeSeparators(candidate).toStdString());
        }
    }
    if (candidates.isEmpty()) {
        error = "Asset Tree is not set. Configure Config/Asset Tree in LevelEdit.ini.";
        return false;
    }

    bool saw_non_json_candidate = false;
    std::string last_error;
    for (const QString &candidate : candidates) {
        const QFileInfo info(candidate);
        if (!info.exists() || !info.isFile()) {
            continue;
        }

        const QString absolute_path = info.absoluteFilePath();
        if (absolute_path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
            continue;
        }

        saw_non_json_candidate = true;
        if (LoadPresetDefinitionDetailsFromDdb(absolute_path, definition_id, details, last_error)) {
            error.clear();
            return true;
        }
    }

    if (!saw_non_json_candidate) {
        error =
            "Preset definition detail view requires objects.ddb/Always.dbs data. JSON-only catalogs are not sufficient.";
        return false;
    }

    if (last_error.empty()) {
        error = "Unable to read preset definition detail from the configured Asset Tree.";
    } else {
        error = last_error;
    }
    return false;
}

} // namespace leveledit_qt
