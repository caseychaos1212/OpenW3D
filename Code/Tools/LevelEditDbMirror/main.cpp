#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>

namespace {

std::string ReadAll(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return {};
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool Contains(const std::string &text, const std::string &needle)
{
    return text.find(needle) != std::string::npos;
}

uint64_t Fnv1a64(const std::string &text)
{
    constexpr uint64_t kOffset = 1469598103934665603ull;
    constexpr uint64_t kPrime = 1099511628211ull;

    uint64_t hash = kOffset;
    for (unsigned char ch : text) {
        hash ^= static_cast<uint64_t>(ch);
        hash *= kPrime;
    }

    return hash;
}

void PrintUsage()
{
    std::cout << "Usage:\n"
              << "  leveledit_dbmirror verify --ddb <path-to-ddb> --json <path-to-ddb.json>\n";
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    const std::string command = argv[1];
    if (command != "verify") {
        PrintUsage();
        return 1;
    }

    std::string ddb_path;
    std::string json_path;

    for (int index = 2; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--ddb" && index + 1 < argc) {
            ddb_path = argv[++index];
        } else if (arg == "--json" && index + 1 < argc) {
            json_path = argv[++index];
        } else {
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
            PrintUsage();
            return 1;
        }
    }

    if (ddb_path.empty() || json_path.empty()) {
        std::cerr << "Missing required --ddb/--json arguments.\n";
        PrintUsage();
        return 1;
    }

    const std::string ddb_data = ReadAll(ddb_path);
    if (ddb_data.empty()) {
        std::cerr << "Unable to read DDB file: " << ddb_path << "\n";
        return 1;
    }

    const std::string json_data = ReadAll(json_path);
    if (json_data.empty()) {
        std::cerr << "Unable to read JSON mirror file: " << json_path << "\n";
        return 1;
    }

    if (!Contains(json_data, "\"schemaVersion\": 1")) {
        std::cerr << "JSON mirror missing expected schemaVersion marker.\n";
        return 1;
    }

    if (!Contains(json_data, "\"definitions\"")) {
        std::cerr << "JSON mirror missing definitions section.\n";
        return 1;
    }

    if (!Contains(json_data, "\"presets\"")) {
        std::cerr << "JSON mirror missing presets section.\n";
        return 1;
    }

    const uint64_t hash = Fnv1a64(json_data);

    std::cout << "verify: ok\n";
    std::cout << "  ddb: " << ddb_path << "\n";
    std::cout << "  json: " << json_path << "\n";
    std::cout << "  json_fnv1a64: 0x" << std::hex << hash << std::dec << "\n";
    std::cout << "Run this command after repeated exports and compare json_fnv1a64 to confirm determinism.\n";

    return 0;
}
