/*
**  Platform Abstraction Layer — Registry store
*/

#include "PAL_Registry.h"

#if defined(PLATFORM_LINUX)

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <functional>
#include <mutex>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace pal {
namespace {

std::mutex registryMutex;

struct FileDescriptor {
	int fd;
	explicit FileDescriptor(int value) : fd(value) {}
	~FileDescriptor() { if (fd >= 0) ::close(fd); }
	FileDescriptor(const FileDescriptor&) = delete;
	FileDescriptor& operator=(const FileDescriptor&) = delete;
	bool close() { int value = fd; fd = -1; return ::close(value) == 0; }
};

int hexDigit(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

std::string serialize(const Store& store)
{
	std::string content;
	for (const auto& section : store) {
		content += "[" + section.first + "]\n";
		for (const auto& value : section.second) content += value.first + "=" + value.second + "\n";
	}
	return content;
}

// Read-modify-write under a sidecar lock, then an atomic replace. A stable lock
// file serializes the whole operation across processes; writing through a temp
// name and renaming means a reader never sees half a file.
bool commit(const Store& store, const std::filesystem::path& file)
{
	const std::string content = serialize(store);
	if (content.size() > kMaxFileSize) return false;

	FileDescriptor directory(::open(file.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
	if (directory.fd < 0) return false;

	std::string pattern = file.string() + ".tmp.XXXXXX";
	std::vector<char> name(pattern.begin(), pattern.end());
	name.push_back('\0');
	FileDescriptor output(::mkostemp(name.data(), O_CLOEXEC));
	if (output.fd < 0) return false;

	bool ok = true;
	for (size_t pos = 0; pos < content.size();) {
		ssize_t count = ::write(output.fd, content.data() + pos, content.size() - pos);
		if (count < 0 && errno == EINTR) continue;
		if (count <= 0) { ok = false; break; }
		pos += static_cast<size_t>(count);
	}
	if (ok && ::fsync(output.fd) != 0) ok = false;
	if (!output.close()) ok = false;
	if (ok && ::rename(name.data(), file.c_str()) != 0) ok = false;
	if (!ok) { ::unlink(name.data()); return false; }

	// A directory sync failure is reported even though rename has already committed.
	ok = ::fsync(directory.fd) == 0;
	if (!directory.close()) ok = false;
	return ok;
}

// Read-modify-write under a sidecar lock. The lock file is stable (not the store
// itself), so two processes cannot both read, both mutate and then race on the
// rename; the mutation is applied to whatever was committed while waiting.
bool mutate(const std::filesystem::path &file, const std::vector<std::string> &allowed,
            const std::function<bool(Store &)> &change)
{
	std::error_code error;
	std::filesystem::create_directories(file.parent_path(), error);
	if (error) return false;

	FileDescriptor lock(::open((file.string() + ".lock").c_str(),
	                           O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0600));
	if (lock.fd < 0) return false;
	struct stat info;
	if (::fstat(lock.fd, &info) != 0 || !S_ISREG(info.st_mode)) return false;
	while (::flock(lock.fd, LOCK_EX) != 0) if (errno != EINTR) return false;

	Store store;
	if (!LoadStore(file, allowed, store, true)) { lock.close(); return false; }
	if (!change(store)) { lock.close(); return false; }

	bool ok = commit(store, file);
	if (!lock.close()) ok = false;
	return ok;
}
}  // namespace

bool RegistryFile(std::filesystem::path& file)
{
	const char* base = std::getenv("XDG_DATA_HOME");
	if (base && base[0] == '/') {
		file = std::filesystem::path(base);
	} else {
		// XDG requires an absolute path; an empty/relative override is ignored.
		base = std::getenv("HOME");
		if (!base || base[0] != '/') return false;
		file = std::filesystem::path(base) / ".local/share";
	}
	file /= "CnCGeneralsZH-Reborn/registry.ini";
	return true;
}

bool ValidKey(const std::string& key)
{
	if (key.empty() || key.size() > kMaxKeyLength) return false;
	for (unsigned char c : key)
		if (c <= 32 || c >= 127 || c == '=' || c == '[' || c == ']' || c == ';' || c == '#' || c == ':')
			return false;
	return true;
}

bool ParseDword(const std::string& text, unsigned int& value)
{
	if (text.empty() || text.size() > 10) return false;
	unsigned int number = 0;
	for (char c : text) {
		if (c < '0' || c > '9' || number > (0xffffffffu - (c - '0')) / 10u) return false;
		number = number * 10u + (c - '0');
	}
	value = number;
	return true;
}

std::string EncodeHex(const unsigned char* bytes, size_t size)
{
	static const char digits[] = "0123456789abcdef";
	std::string result;
	result.reserve(size * 2);
	for (size_t i = 0; i < size; ++i) {
		result += digits[bytes[i] >> 4];
		result += digits[bytes[i] & 15];
	}
	return result;
}

bool DecodeHex(const std::string& text, std::vector<unsigned char>& bytes)
{
	if (text.size() % 2 || text.size() > kMaxBinarySize * 2) return false;
	for (size_t i = 0; i < text.size(); i += 2) {
		int a = hexDigit(text[i]), b = hexDigit(text[i + 1]);
		if (a < 0 || b < 0) return false;
		bytes.push_back(static_cast<unsigned char>((a << 4) | b));
	}
	return true;
}

bool ValidValue(const std::string& typedValue)
{
	if (typedValue.compare(0, 6, "DWORD:") == 0) {
		unsigned int number;
		return ParseDword(typedValue.substr(6), number);
	}
	const bool text = typedValue.compare(0, 3, "SZ:") == 0;
	if (!text && typedValue.compare(0, 7, "BINARY:") != 0) return false;
	std::vector<unsigned char> bytes;
	if (!DecodeHex(typedValue.substr(text ? 3 : 7), bytes)) return false;
	if (text) {
		if (bytes.size() > kMaxStringSize) return false;
		for (unsigned char c : bytes) if (!c) return false;
	}
	return true;
}

bool LoadStore(const std::filesystem::path& file, const std::vector<std::string>& allowed,
               Store& store, bool allowMissing)
{
	FileDescriptor input(::open(file.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
	if (input.fd < 0) return allowMissing && errno == ENOENT;
	struct stat info;
	if (::fstat(input.fd, &info) != 0 || !S_ISREG(info.st_mode) ||
	    info.st_size > static_cast<off_t>(kMaxFileSize))
		return false;

	std::string content;
	char buffer[4096];
	for (;;) {
		ssize_t count = ::read(input.fd, buffer, sizeof(buffer));
		if (count < 0) { if (errno == EINTR) continue; return false; }
		if (!count) break;
		if (content.size() + static_cast<size_t>(count) > kMaxFileSize) return false;
		content.append(buffer, static_cast<size_t>(count));
	}
	if (!input.close()) return false;

	std::string section;
	for (size_t pos = 0; pos < content.size();) {
		size_t end = content.find('\n', pos);
		if (end == std::string::npos) end = content.size();
		std::string line = content.substr(pos, end - pos);
		pos = end + 1;
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.empty() || line[0] == ';' || line[0] == '#') continue;
		if (line[0] == '[') {
			if (line.back() != ']') return false;
			section = line.substr(1, line.size() - 2);
			if (!allowed.empty() &&
			    std::find(allowed.begin(), allowed.end(), section) == allowed.end())
				return false;
			continue;
		}
		size_t equals = line.find('=');
		if (section.empty() || equals == std::string::npos || equals > kMaxKeyLength) return false;
		std::string key = line.substr(0, equals);
		if (key.find('\0') != std::string::npos || !ValidKey(key)) return false;
		std::string value = line.substr(equals + 1);
		if (!ValidValue(value) || !store[section].emplace(key, value).second) return false;
	}
	return true;
}

bool ReadValue(const std::string& section, const std::string& key, const char* type,
               const std::vector<std::string>& allowed, std::string& value)
{
	try {
		std::lock_guard<std::mutex> guard(registryMutex);
		std::filesystem::path file;
		Store store;
		if (!ValidKey(key) || !RegistryFile(file) || !LoadStore(file, allowed, store, false)) return false;
		auto s = store.find(section);
		if (s == store.end()) return false;
		auto v = s->second.find(key);
		if (v == s->second.end() || v->second.compare(0, std::strlen(type), type) != 0) return false;
		value = v->second.substr(std::strlen(type));
		return true;
	} catch (const std::exception&) { return false; }
}

bool WriteValue(const std::string& section, const std::string& key,
                const std::string& typedValue, const std::vector<std::string>& allowed)
{
	try {
		std::lock_guard<std::mutex> guard(registryMutex);
		if (!ValidKey(key) || !ValidValue(typedValue)) return false;
		std::filesystem::path file;
		if (!RegistryFile(file)) return false;
		return mutate(file, allowed, [&](Store &store) { store[section][key] = typedValue; return true; });
	} catch (const std::exception&) { return false; }
}

bool RemoveValue(const std::string& section, const std::string& key,
                 const std::vector<std::string>& allowed)
{
	try {
		std::lock_guard<std::mutex> guard(registryMutex);
		if (!ValidKey(key)) return false;
		std::filesystem::path file;
		if (!RegistryFile(file)) return false;
		return mutate(file, allowed, [&](Store &store) {
			auto s = store.find(section);
			if (s == store.end() || s->second.find(key) == s->second.end()) return false;
			s->second.erase(key);
			return true;
		});
	} catch (const std::exception&) { return false; }
}

bool RemoveAllValues(const std::string& section, const std::vector<std::string>& allowed)
{
	try {
		std::lock_guard<std::mutex> guard(registryMutex);
		std::filesystem::path file;
		if (!RegistryFile(file)) return false;
		return mutate(file, allowed, [&](Store &store) {
			auto s = store.find(section);
			if (s == store.end()) return false;
			s->second.clear();
			return true;
		});
	} catch (const std::exception&) { return false; }
}

bool SectionExists(const std::string& section, const std::vector<std::string>& allowed)
{
	try {
		std::lock_guard<std::mutex> guard(registryMutex);
		std::filesystem::path file;
		Store store;
		if (!RegistryFile(file) || !LoadStore(file, allowed, store, false)) return false;
		return store.find(section) != store.end();
	} catch (const std::exception&) { return false; }
}

bool SectionKeys(const std::string& section, const std::vector<std::string>& allowed,
                 std::vector<std::string>& keys)
{
	try {
		std::lock_guard<std::mutex> guard(registryMutex);
		std::filesystem::path file;
		Store store;
		if (!RegistryFile(file) || !LoadStore(file, allowed, store, false)) return false;
		auto s = store.find(section);
		if (s == store.end()) return false;
		for (const auto& value : s->second) keys.push_back(value.first);
		return true;
	} catch (const std::exception&) { return false; }
}

}  // namespace pal

#endif  // PLATFORM_LINUX
