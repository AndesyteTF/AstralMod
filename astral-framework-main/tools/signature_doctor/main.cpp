#include <capstone/capstone.h>
#include <capstone/x86.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

struct pattern_byte {
    std::uint8_t value{};
    bool wildcard{};
};

struct signature_entry {
    std::string id;
    std::string module;
    std::string text;
    std::vector<pattern_byte> pattern;
    std::size_t line{};
};

struct section_view {
    std::string name;
    std::uint32_t rva{};
    std::size_t file_offset{};
    std::size_t size{};
    bool executable{};
};

struct pe_image {
    fs::path path;
    std::vector<std::uint8_t> bytes;
    std::vector<section_view> sections;
};

struct match {
    std::size_t file_offset{};
    std::uint32_t rva{};
};

static std::optional<std::vector<std::uint8_t>> read_file(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        return std::nullopt;
    stream.seekg(0, std::ios::end);
    const auto length = stream.tellg();
    if (length < 0)
        return std::nullopt;
    stream.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty())
        stream.read(reinterpret_cast<char*>(bytes.data()), length);
    if (!stream && !bytes.empty())
        return std::nullopt;
    return bytes;
}

template <typename T>
static std::optional<T> read_scalar(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
        return std::nullopt;
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

static std::optional<pe_image> load_pe(const fs::path& path) {
    auto file = read_file(path);
    if (!file || file->size() < 0x100 || (*file)[0] != 'M' || (*file)[1] != 'Z')
        return std::nullopt;
    const auto pe_offset = read_scalar<std::uint32_t>(*file, 0x3c);
    if (!pe_offset || *pe_offset + 24 > file->size() ||
        std::string_view(reinterpret_cast<const char*>(file->data() + *pe_offset), 4) != std::string_view("PE\0\0", 4))
        return std::nullopt;

    const auto section_count = read_scalar<std::uint16_t>(*file, *pe_offset + 6);
    const auto optional_size = read_scalar<std::uint16_t>(*file, *pe_offset + 20);
    if (!section_count || !optional_size)
        return std::nullopt;
    const std::size_t table = *pe_offset + 24 + *optional_size;
    if (table > file->size() || static_cast<std::size_t>(*section_count) * 40 > file->size() - table)
        return std::nullopt;

    pe_image image{path, std::move(*file), {}};
    for (std::size_t i = 0; i < *section_count; ++i) {
        const std::size_t at = table + i * 40;
        char name_buffer[9]{};
        std::memcpy(name_buffer, image.bytes.data() + at, 8);
        const auto virtual_size = read_scalar<std::uint32_t>(image.bytes, at + 8).value_or(0);
        const auto virtual_address = read_scalar<std::uint32_t>(image.bytes, at + 12).value_or(0);
        const auto raw_size = read_scalar<std::uint32_t>(image.bytes, at + 16).value_or(0);
        const auto raw_offset = read_scalar<std::uint32_t>(image.bytes, at + 20).value_or(0);
        const auto characteristics = read_scalar<std::uint32_t>(image.bytes, at + 36).value_or(0);
        if (raw_offset >= image.bytes.size())
            continue;
        const std::size_t available = image.bytes.size() - raw_offset;
        const std::size_t size = std::min<std::size_t>(raw_size ? raw_size : virtual_size, available);
        image.sections.push_back({name_buffer, virtual_address, raw_offset, size,
                                  (characteristics & 0x20000000u) != 0});
    }
    return image;
}

static std::optional<std::vector<pattern_byte>> parse_pattern(const std::string& text) {
    std::istringstream input(text);
    std::string token;
    std::vector<pattern_byte> result;
    while (input >> token) {
        if (token == "?" || token == "??") {
            result.push_back({0, true});
            continue;
        }
        if (token.size() != 2 || !std::isxdigit(static_cast<unsigned char>(token[0])) ||
            !std::isxdigit(static_cast<unsigned char>(token[1])))
            return std::nullopt;
        result.push_back({static_cast<std::uint8_t>(std::stoul(token, nullptr, 16)), false});
    }
    if (result.empty())
        return std::nullopt;
    return result;
}

static std::vector<signature_entry> parse_tables(const fs::path& path) {
    std::ifstream input(path);
    std::vector<signature_entry> entries;
    if (!input)
        return entries;

    const std::regex id_re(R"re(HASH_RT\(\s*"([^"]+)"\s*\))re");
    const std::regex module_re(R"re(api_module::([a-zA-Z0-9_]+))re");
    const std::regex string_re(R"re("([0-9A-Fa-f? ]+)")re");
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        std::smatch id_match, module_match;
        if (!std::regex_search(line, id_match, id_re) || !std::regex_search(line, module_match, module_re))
            continue;
        for (std::sregex_iterator it(line.begin(), line.end(), string_re), end; it != end; ++it) {
            const std::string pattern_text = (*it)[1].str();
            auto pattern = parse_pattern(pattern_text);
            if (!pattern || pattern->size() < 3)
                continue;
            entries.push_back({id_match[1].str(), module_match[1].str(), pattern_text,
                               std::move(*pattern), line_number});
            break;
        }
    }
    return entries;
}

static std::vector<match> scan(const pe_image& image, const std::vector<pattern_byte>& pattern,
                               bool executable_only = false, std::size_t limit = 64) {
    std::vector<match> matches;
    for (const auto& section : image.sections) {
        if ((executable_only && !section.executable) || pattern.size() > section.size)
            continue;
        const auto* data = image.bytes.data() + section.file_offset;
        for (std::size_t i = 0; i <= section.size - pattern.size(); ++i) {
            bool equal = true;
            for (std::size_t j = 0; j < pattern.size(); ++j) {
                if (!pattern[j].wildcard && data[i + j] != pattern[j].value) {
                    equal = false;
                    break;
                }
            }
            if (equal) {
                matches.push_back({section.file_offset + i,
                                   static_cast<std::uint32_t>(section.rva + i)});
                if (matches.size() >= limit)
                    return matches;
            }
        }
    }
    return matches;
}

static const std::unordered_map<std::string, std::string> module_files{
    {"client", "client.dll"}, {"engine", "engine.dll"},
    {"studiorender", "studiorender.dll"}, {"vgui2", "vgui2.dll"},
    {"vguimatsurface", "vguimatsurface.dll"}, {"tier0", "tier0.dll"},
    {"materialsystem", "materialsystem.dll"}, {"vstdlib", "vstdlib.dll"},
    {"vphysics", "vphysics.dll"}, {"shaderapidx9", "shaderapidx9.dll"},
    {"steamclient64", "steamclient64.dll"}, {"gameoverlay", "gameoverlayrenderer64.dll"},
};

static std::optional<fs::path> find_module(const fs::path& root, const std::string& module) {
    const auto known = module_files.find(module);
    if (known == module_files.end())
        return std::nullopt;
    const fs::path direct = root / known->second;
    if (fs::exists(direct))
        return direct;
    const fs::path game_bin = root / "bin" / "x64" / known->second;
    if (fs::exists(game_bin))
        return game_bin;
    const fs::path tf_bin = root / "tf" / "bin" / "x64" / known->second;
    if (fs::exists(tf_bin))
        return tf_bin;
    return std::nullopt;
}

struct decoded_instruction {
    unsigned int id{};
    std::vector<std::uint8_t> bytes;
    std::uint8_t disp_offset{}, disp_size{}, imm_offset{}, imm_size{};
};

static std::vector<decoded_instruction> decode(csh handle, const pe_image& image, std::size_t offset,
                                                std::size_t wanted) {
    std::vector<decoded_instruction> result;
    if (offset >= image.bytes.size())
        return result;
    const std::uint8_t* code = image.bytes.data() + offset;
    std::size_t remaining = std::min<std::size_t>(image.bytes.size() - offset, 256);
    std::uint64_t address = 0;
    cs_insn* instruction = cs_malloc(handle);
    if (!instruction)
        return result;
    while (result.size() < wanted && cs_disasm_iter(handle, &code, &remaining, &address, instruction)) {
        decoded_instruction decoded;
        decoded.id = instruction->id;
        decoded.bytes.assign(instruction->bytes, instruction->bytes + instruction->size);
        if (instruction->detail) {
            const auto& encoding = instruction->detail->x86.encoding;
            decoded.disp_offset = encoding.disp_offset;
            decoded.disp_size = encoding.disp_size;
            decoded.imm_offset = encoding.imm_offset;
            decoded.imm_size = encoding.imm_size;
        }
        result.push_back(std::move(decoded));
    }
    cs_free(instruction, 1);
    return result;
}

static bool same_fingerprint(const std::vector<decoded_instruction>& left,
                             const std::vector<decoded_instruction>& right) {
    if (left.size() != right.size())
        return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (left[i].id != right[i].id || left[i].bytes.size() != right[i].bytes.size())
            return false;
    }
    return true;
}

static std::vector<std::size_t> locate_fingerprint(csh handle, const pe_image& image,
                                                    const std::vector<decoded_instruction>& fingerprint,
                                                    std::size_t limit = 8) {
    std::vector<std::size_t> result;
    if (fingerprint.empty())
        return result;
    for (const auto& section : image.sections) {
        if (!section.executable)
            continue;
        for (std::size_t i = 0; i < section.size; ++i) {
            auto candidate = decode(handle, image, section.file_offset + i, fingerprint.size());
            if (same_fingerprint(fingerprint, candidate)) {
                result.push_back(section.file_offset + i);
                if (result.size() >= limit)
                    return result;
            }
        }
    }
    return result;
}

static std::vector<pattern_byte> make_pattern(const std::vector<decoded_instruction>& instructions) {
    std::vector<pattern_byte> result;
    for (const auto& instruction : instructions) {
        const std::size_t start = result.size();
        for (auto byte : instruction.bytes)
            result.push_back({byte, false});
        const auto wildcard = [&](std::uint8_t offset, std::uint8_t size) {
            for (std::size_t i = 0; i < size && start + offset + i < result.size(); ++i)
                result[start + offset + i].wildcard = true;
        };
        wildcard(instruction.disp_offset, instruction.disp_size);
        wildcard(instruction.imm_offset, instruction.imm_size);
    }
    return result;
}

static std::string pattern_text(const std::vector<pattern_byte>& pattern) {
    std::ostringstream output;
    output << std::uppercase << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (i)
            output << ' ';
        if (pattern[i].wildcard)
            output << '?';
        else
            output << std::setw(2) << static_cast<unsigned int>(pattern[i].value);
    }
    return output.str();
}

static std::optional<std::string> repair_candidate(csh handle, const signature_entry& entry,
                                                    const pe_image& baseline, const pe_image& current) {
    const auto old_matches = scan(baseline, entry.pattern);
    if (old_matches.size() != 1)
        return std::nullopt;
    for (std::size_t instruction_count = 6; instruction_count <= 20; ++instruction_count) {
        const auto fingerprint = decode(handle, baseline, old_matches[0].file_offset, instruction_count);
        if (fingerprint.size() != instruction_count)
            return std::nullopt;
        const auto locations = locate_fingerprint(handle, current, fingerprint, 2);
        if (locations.size() != 1)
            continue;
        const auto current_instructions = decode(handle, current, locations[0], instruction_count);
        auto candidate = make_pattern(current_instructions);
        const auto candidate_matches = scan(current, candidate, true, 2);
        if (candidate_matches.size() == 1)
            return pattern_text(candidate);
    }
    return std::nullopt;
}

static void usage() {
    std::cerr << "Usage: astral-signature-doctor --tables <dev_tables.cpp> --modules <current-dir>\n"
                 "       [--baseline <known-good-dir>] [--only <signature-id>]\n";
}

int main(int argc, char** argv) {
    fs::path tables, modules, baseline;
    std::string only;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if ((argument == "--tables" || argument == "--modules" || argument == "--baseline" || argument == "--only") && i + 1 < argc) {
            const std::string value = argv[++i];
            if (argument == "--tables") tables = value;
            else if (argument == "--modules") modules = value;
            else if (argument == "--baseline") baseline = value;
            else only = value;
        } else if (argument == "--help" || argument == "-h") {
            usage();
            return 0;
        } else {
            usage();
            return 2;
        }
    }
    if (tables.empty() || modules.empty() || !fs::exists(tables) || !fs::is_directory(modules)) {
        usage();
        return 2;
    }

    auto entries = parse_tables(tables);
    if (!only.empty()) {
        std::erase_if(entries, [&](const signature_entry& entry) { return entry.id != only; });
    }
    if (entries.empty()) {
        std::cerr << "No matching signature entries were parsed from " << tables << "\n";
        return 2;
    }

    csh capstone{};
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &capstone) != CS_ERR_OK) {
        std::cerr << "Unable to initialize Capstone.\n";
        return 2;
    }
    cs_option(capstone, CS_OPT_DETAIL, CS_OPT_ON);

    std::unordered_map<std::string, pe_image> current_images, baseline_images;
    std::size_t unique = 0, missing = 0, ambiguous = 0, unavailable = 0, repaired = 0;
    for (const auto& entry : entries) {
        if (!current_images.contains(entry.module)) {
            if (auto path = find_module(modules, entry.module)) {
                if (auto image = load_pe(*path))
                    current_images.emplace(entry.module, std::move(*image));
            }
        }
        const auto image_it = current_images.find(entry.module);
        if (image_it == current_images.end()) {
            ++unavailable;
            std::cout << "[NO MODULE] " << entry.id << " (" << entry.module << ")\n";
            continue;
        }
        const auto matches = scan(image_it->second, entry.pattern);
        if (matches.size() == 1) {
            ++unique;
            std::cout << "[OK]        " << entry.id << " @ RVA 0x" << std::hex << std::uppercase
                      << matches[0].rva << std::dec << "\n";
            continue;
        }
        if (matches.empty()) {
            ++missing;
            std::cout << "[MISSING]   " << entry.id << " (line " << entry.line << ")\n";
        } else {
            ++ambiguous;
            std::cout << "[AMBIGUOUS] " << entry.id << " (" << matches.size() << "+ matches, line "
                      << entry.line << ")\n";
        }

        if (!baseline.empty()) {
            if (!baseline_images.contains(entry.module)) {
                if (auto path = find_module(baseline, entry.module)) {
                    if (auto image = load_pe(*path))
                        baseline_images.emplace(entry.module, std::move(*image));
                }
            }
            const auto old_it = baseline_images.find(entry.module);
            if (old_it != baseline_images.end()) {
                if (auto candidate = repair_candidate(capstone, entry, old_it->second, image_it->second)) {
                    ++repaired;
                    std::cout << "            candidate: \"" << *candidate << "\"\n";
                } else {
                    std::cout << "            no unique structural repair candidate\n";
                }
            }
        }
    }
    cs_close(&capstone);

    std::cout << "\nSummary: " << entries.size() << " checked, " << unique << " unique, " << missing
              << " missing, " << ambiguous << " ambiguous, " << unavailable << " unavailable";
    if (!baseline.empty())
        std::cout << ", " << repaired << " repair candidates";
    std::cout << ".\n";
    return missing == 0 && ambiguous == 0 && unavailable == 0 ? 0 : 1;
}

