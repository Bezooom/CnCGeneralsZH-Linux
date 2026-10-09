// Linux registry contract tests. No WinAPI fake and no test implementation.
// Empty path maps to [General]; "\\Options" maps to [Options].
#include "test_harness.h"
#include "Common/Registry.h"
#include "Common/GameMemory.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
namespace fs = std::filesystem;

struct SavedEnvironment {
	const char *name;
	bool present;
	std::string value;
	explicit SavedEnvironment(const char *key) : name(key), present(getenv(key) != nullptr),
		value(present ? getenv(key) : "") {}
	~SavedEnvironment() {
		if (present) setenv(name, value.c_str(), 1);
		else unsetenv(name);
	}
};

struct RegistryFixture {
	SavedEnvironment home{"HOME"}, xdg{"XDG_DATA_HOME"};
	fs::path root;
	RegistryFixture() {
		char pattern[] = "/tmp/registry-linux-XXXXXX";
		const char *created = mkdtemp(pattern);
		// CHECK does not abort: do not proceed toward a user's real registry
		// if isolation cannot be established.
		if (!created) { perror("mkdtemp"); std::abort(); }
		root = created;
		if (setenv("HOME", (root / "home").c_str(), 1) != 0 ||
			setenv("XDG_DATA_HOME", (root / "xdg").c_str(), 1) != 0)
			std::abort();
		static bool booted = false;
		if (!booted) { initMemoryManager(); booted = true; }
	}
	~RegistryFixture() {
		std::error_code error;
		fs::remove_all(root, error);
		CHECK(!error);
	}
	fs::path file() const { return root / "xdg/CnCGeneralsZH-Reborn/registry.ini"; }
	fs::path fallback() const { return root / "home/.local/share/CnCGeneralsZH-Reborn/registry.ini"; }
};

std::string readFile(const fs::path &path) {
	std::ifstream input(path);
	CHECK(input.is_open());
	return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void expectMissing(const char *path, const char *key) {
	AsciiString text = "unchanged";
	UnsignedInt number = 0xabcdef01u;
	CHECK(!GetStringFromRegistry(path, key, text));
	CHECK_STR(text.str(), "unchanged");
	CHECK(!GetUnsignedIntFromRegistry(path, key, number));
	CHECK_EQ(number, 0xabcdef01u);
}
}

TEST(registry_linux_absent_file_and_keys_leave_outputs_unchanged) {
	RegistryFixture fixture;
	expectMissing("", "Missing");
	CHECK(!fs::exists(fixture.file())); // reads must not create an empty store
	CHECK(SetStringInRegistry("", "Existing", "value"));
	expectMissing("", "Missing");
	expectMissing("\\Options", "Existing");
}

TEST(registry_linux_xdg_creates_the_store_and_separates_sections) {
	RegistryFixture fixture;
	CHECK(SetStringInRegistry("", "Language", "english"));
	CHECK(SetStringInRegistry("\\Options", "Language", "german"));
	AsciiString value;
	CHECK(GetStringFromRegistry("", "Language", value));
	CHECK_STR(value.str(), "english");
	CHECK(GetStringFromRegistry("\\Options", "Language", value));
	CHECK_STR(value.str(), "german");
	const std::string contents = readFile(fixture.file());
	CHECK(contents.find("[General]") != std::string::npos);
	CHECK(contents.find("[Options]") != std::string::npos);
	CHECK(!fs::exists(fixture.fallback()));
}

TEST(registry_linux_unset_and_empty_xdg_use_home_local_share) {
	RegistryFixture fixture;
	CHECK_EQ(unsetenv("XDG_DATA_HOME"), 0);
	CHECK(SetUnsignedIntInRegistry("", "Version", 65537u));
	CHECK(fs::is_regular_file(fixture.fallback()));
	CHECK(!fs::exists(fixture.file()));
	CHECK_EQ(setenv("XDG_DATA_HOME", "", 1), 0);
	UnsignedInt value = 0;
	CHECK(GetUnsignedIntFromRegistry("", "Version", value));
	CHECK_EQ(value, 65537u);
	CHECK(SetStringInRegistry("\\Options", "Device", "default"));
	CHECK(!fs::exists(fixture.file()));
}

TEST(registry_linux_dword_roundtrip_preserves_unsigned_32_bits) {
	RegistryFixture fixture;
	CHECK_EQ(sizeof(UnsignedInt), 4u);
	CHECK_EQ(std::numeric_limits<UnsignedInt>::digits, 32);
	const UnsignedInt values[] = {0u, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu};
	for (UnsignedInt value : values) {
		CHECK(SetUnsignedIntInRegistry("\\Options", "Resolution", value));
		UnsignedInt actual = value ^ 1u;
		CHECK(GetUnsignedIntFromRegistry("\\Options", "Resolution", actual));
		CHECK_EQ(actual, value);
	}
}

TEST(registry_linux_strings_roundtrip_empty_long_and_ini_punctuation) {
	RegistryFixture fixture;
	const std::string values[] = {"", "english", " path with spaces = ; # [brackets] ",
		"C:\\Games\\Generals", std::string(1024, 'x')};
	for (const std::string &value : values) {
		CHECK(SetStringInRegistry("", "InstallPath", value.c_str()));
		AsciiString actual = "sentinel";
		CHECK(GetStringFromRegistry("", "InstallPath", actual));
		CHECK_STR(actual.str(), value.c_str());
	}
}

TEST(registry_linux_repeated_calls_preserve_other_keys_and_sections) {
	RegistryFixture fixture;
	CHECK(SetStringInRegistry("", "Language", "english"));
	CHECK(SetUnsignedIntInRegistry("\\Options", "Volume", 70u));
	for (unsigned i = 0; i < 12; ++i) {
		CHECK(SetUnsignedIntInRegistry("", "Version", i));
		AsciiString text;
		UnsignedInt volume = 0, version = 99;
		CHECK(GetStringFromRegistry("", "Language", text));
		CHECK_STR(text.str(), "english");
		CHECK(GetUnsignedIntFromRegistry("\\Options", "Volume", volume));
		CHECK_EQ(volume, 70u);
		CHECK(GetUnsignedIntFromRegistry("", "Version", version));
		CHECK_EQ(version, i);
	}
}

TEST(registry_linux_child_writes_survive_process_exit) {
	RegistryFixture fixture;
	// Parent has never read these keys; a process-local map cannot satisfy this.
	const pid_t child = fork();
	CHECK(child >= 0);
	if (child < 0) return;
	if (child == 0) {
		const bool written = SetStringInRegistry("", "Language", "french") &&
			SetUnsignedIntInRegistry("\\Options", "Volume", 42u);
		_exit(written ? 0 : 1);
	}
	int status = 0;
	CHECK_EQ(waitpid(child, &status, 0), child);
	CHECK(WIFEXITED(status));
	CHECK_EQ(WEXITSTATUS(status), 0);
	AsciiString text;
	UnsignedInt number = 0;
	CHECK(GetStringFromRegistry("", "Language", text));
	CHECK_STR(text.str(), "french");
	CHECK(GetUnsignedIntFromRegistry("\\Options", "Volume", number));
	CHECK_EQ(number, 42u);
}

TEST(registry_linux_setenv_overrides_are_not_cached) {
	RegistryFixture fixture;
	CHECK(SetStringInRegistry("", "Language", "first"));
	CHECK_EQ(setenv("XDG_DATA_HOME", (fixture.root / "second").c_str(), 1), 0);
	expectMissing("", "Language");
	CHECK(SetStringInRegistry("", "Language", "second"));
	CHECK(fs::is_regular_file(fixture.root / "second/CnCGeneralsZH-Reborn/registry.ini"));
	AsciiString value;
	CHECK(GetStringFromRegistry("", "Language", value));
	CHECK_STR(value.str(), "second");
	CHECK_EQ(setenv("XDG_DATA_HOME", (fixture.root / "xdg").c_str(), 1), 0);
	CHECK(GetStringFromRegistry("", "Language", value));
	CHECK_STR(value.str(), "first");
}

TEST(registry_linux_wrong_value_types_leave_outputs_unchanged) {
	RegistryFixture fixture;
	CHECK(SetStringInRegistry("", "Text", "not-a-number"));
	UnsignedInt number = 123u;
	CHECK(!GetUnsignedIntFromRegistry("", "Text", number));
	CHECK_EQ(number, 123u);
	CHECK(SetUnsignedIntInRegistry("", "Number", 42u));
	AsciiString text = "unchanged";
	CHECK(!GetStringFromRegistry("", "Number", text));
	CHECK_STR(text.str(), "unchanged");
}

TEST(registry_linux_binary_roundtrip_preserves_zero_ff_and_empty) {
	RegistryFixture fixture;
	const std::vector<unsigned char> values[] = {
		{0x00, 0xff, 0x00, 0x80, 0x7f, 0xff, 0x00}, {}, {0xff}, {0x00},
		std::vector<unsigned char>(65536, 0xff)
	};
	for (const auto &expected : values) {
		CHECK(SetBinaryInRegistry("\\Options", "Blob", expected));
		std::vector<unsigned char> actual = {0x12, 0x34};
		CHECK(GetBinaryFromRegistry("\\Options", "Blob", actual));
		CHECK(actual == expected);
		AsciiString text = "unchanged";
		UnsignedInt number = 123u;
		CHECK(!GetStringFromRegistry("\\Options", "Blob", text));
		CHECK_STR(text.str(), "unchanged");
		CHECK(!GetUnsignedIntFromRegistry("\\Options", "Blob", number));
		CHECK_EQ(number, 123u);
	}
	CHECK(SetStringInRegistry("", "Text", ""));
	CHECK(SetUnsignedIntInRegistry("", "Number", 0u));
	for (const char *key : {"Text", "Number", "Missing"}) {
		std::vector<unsigned char> actual = {0x00, 0xff, 0x42};
		const auto original = actual;
		CHECK(!GetBinaryFromRegistry("", key, actual));
		CHECK(actual == original);
	}
}

TEST(registry_linux_corrupt_ini_and_overflow_fail_without_partial_outputs_or_clobber) {
	RegistryFixture fixture;
	// Keep valid entries before the corruption: a partial parser must not return them
	// or silently discard the damaged tail on a subsequent write.
	const std::string prefix = "[General]\nText=SZ:6f6b\nNumber=DWORD:42\nBlob=BINARY:00ff\n";
	const std::string corruptions[] = {
		"[Options\n", "MissingEquals\n", "Text=SZ:61\n", "[Unknown]\n",
		"Bad=QWORD:42\n", "Bad=DWORD:4294967296\n", "Bad=DWORD:99999999999999999999\n",
		"Bad=DWORD:-1\n", "Bad=DWORD:42junk\n", "Bad=DWORD:\n",
		"Bad=SZ:0\n", "Bad=SZ:6g\n", "Bad=SZ:610062\n",
		"Bad=BINARY:00f\n", "Bad=BINARY:00ffzz\n",
		"Bad=BINARY:" + std::string(65537 * 2, 'f') + "\n",
		"Bad=SZ:" + std::string(32767 * 2, '6') + "\n"
	};
	fs::create_directories(fixture.file().parent_path());
	for (const auto &corruption : corruptions) {
		const std::string damaged = prefix + corruption;
		{
			std::ofstream output(fixture.file(), std::ios::binary | std::ios::trunc);
			output << damaged;
			output.close();
			CHECK(!output.fail());
		}
		for (const char *key : {"Text", "Number", "Blob", "Bad"}) {
			expectMissing("", key);
			std::vector<unsigned char> bytes = {0x00, 0xff, 0x42};
			const auto original = bytes;
			CHECK(!GetBinaryFromRegistry("", key, bytes));
			CHECK(bytes == original);
		}
		CHECK(!SetStringInRegistry("", "Text", "replacement"));
		CHECK_EQ(readFile(fixture.file()), damaged);
		CHECK(!SetUnsignedIntInRegistry("", "Number", 99u));
		CHECK_EQ(readFile(fixture.file()), damaged);
		CHECK(!SetBinaryInRegistry("", "Blob", {0x42}));
		CHECK_EQ(readFile(fixture.file()), damaged);
		CHECK(!fs::exists(fixture.fallback()));
	}
}

TEST(registry_linux_failed_writes_preserve_existing_typed_values) {
	RegistryFixture fixture;
	const std::vector<unsigned char> original = {0x00, 0xff, 0x00};
	CHECK(SetStringInRegistry("", "Text", "keep me"));
	CHECK(SetUnsignedIntInRegistry("\\Options", "Number", 0xffffffffu));
	CHECK(SetBinaryInRegistry("", "Blob", original));
	const std::string before = readFile(fixture.file());
	CHECK(!SetBinaryInRegistry("", "Blob", std::vector<unsigned char>(65537, 0x42)));
	CHECK_EQ(readFile(fixture.file()), before);
	// A directory at the sidecar path forces an I/O failure even when run as root.
	const fs::path lock = fixture.file().string() + ".lock";
	const fs::path savedLock = fixture.root / "saved.lock";
	fs::rename(lock, savedLock);
	CHECK(fs::create_directory(lock));
	CHECK(!SetStringInRegistry("", "Text", "replacement"));
	CHECK_EQ(readFile(fixture.file()), before);
	CHECK(!SetUnsignedIntInRegistry("\\Options", "Number", 0u));
	CHECK_EQ(readFile(fixture.file()), before);
	CHECK(!SetBinaryInRegistry("", "Blob", {}));
	CHECK_EQ(readFile(fixture.file()), before);
	CHECK(fs::remove(lock));
	fs::rename(savedLock, lock);
	AsciiString text;
	UnsignedInt number = 0u;
	std::vector<unsigned char> bytes;
	CHECK(GetStringFromRegistry("", "Text", text));
	CHECK_STR(text.str(), "keep me");
	CHECK(GetUnsignedIntFromRegistry("\\Options", "Number", number));
	CHECK_EQ(number, 0xffffffffu);
	CHECK(GetBinaryFromRegistry("", "Blob", bytes));
	CHECK(bytes == original);
	CHECK(!fs::exists(fixture.fallback()));
	// Recovery demonstrates that the failure did not poison subsequent opens.
	CHECK(SetStringInRegistry("", "Text", "recovered"));
	CHECK(GetStringFromRegistry("", "Text", text));
	CHECK_STR(text.str(), "recovered");
}

TEST(registry_linux_io_failure_returns_false_without_fallback_or_mutation) {
	RegistryFixture fixture;
	// ENOTDIR is deterministic even as root; chmod-based tests are not.
	{ std::ofstream blocker(fixture.root / "xdg"); blocker << "not a directory"; }
	CHECK(fs::is_regular_file(fixture.root / "xdg"));
	CHECK(!SetStringInRegistry("", "Language", "english"));
	CHECK(!SetUnsignedIntInRegistry("\\Options", "Volume", 50u));
	expectMissing("", "Language");
	CHECK_EQ(readFile(fixture.root / "xdg"), "not a directory");
	CHECK(!fs::exists(fixture.fallback()));
}
