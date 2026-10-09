/*
**  Platform Abstraction Layer — Registry store
*/
#pragma once

#if defined(PLATFORM_LINUX)

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace pal {

constexpr size_t kMaxFileSize = 1024 * 1024;
constexpr size_t kMaxStringSize = 32766;  // AsciiString's 32767-byte allocation limit
constexpr size_t kMaxBinarySize = 65536;
constexpr size_t kMaxKeyLength = 255;

using Section = std::map<std::string, std::string>;
using Store = std::map<std::string, Section>;

// One INI file per user: $XDG_DATA_HOME/CnCGeneralsZH-Reborn/registry.ini, falling
// back to $HOME/.local/share when XDG_DATA_HOME is unset or not an absolute path.
bool RegistryFile(std::filesystem::path &file);

// A key is one INI name: no control characters, no NUL, no INI punctuation.
bool ValidKey(const std::string &key);

// Values carry their type as a prefix: DWORD:<decimal>, SZ:<hex>, BINARY:<hex>.
// Hex keeps newlines, backslashes and INI punctuation unambiguous.
bool ParseDword(const std::string &text, unsigned int &value);
std::string EncodeHex(const unsigned char *bytes, size_t size);
bool DecodeHex(const std::string &text, std::vector<unsigned char> &bytes);
bool ValidValue(const std::string &typedValue);

// A caller may only touch the sections it names. A file holding a section the
// caller never wrote is rejected rather than skipped: a parser that ignored the
// unknown section would still rewrite the file on the next write and drop it.
bool LoadStore(const std::filesystem::path &file, const std::vector<std::string> &allowed,
               Store &store, bool allowMissing);

bool ReadValue(const std::string &section, const std::string &key, const char *type,
               const std::vector<std::string> &allowed, std::string &value);
bool WriteValue(const std::string &section, const std::string &key,
                const std::string &typedValue, const std::vector<std::string> &allowed);
bool RemoveValue(const std::string &section, const std::string &key,
                 const std::vector<std::string> &allowed);
bool RemoveAllValues(const std::string &section, const std::vector<std::string> &allowed);
bool SectionExists(const std::string &section, const std::vector<std::string> &allowed);
bool SectionKeys(const std::string &section, const std::vector<std::string> &allowed,
                 std::vector<std::string> &keys);

}  // namespace pal

#endif  // PLATFORM_LINUX
