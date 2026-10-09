// RegistryClass on Linux: the sub_key becomes the INI section, so the class is an
// adapter over Platform/PAL_Registry. These tests drive the class itself, not the
// game-engine API, and isolate the store behind HOME/XDG.
#include "test_harness.h"

#include "registry.h"

#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
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

struct StoreFixture {
	SavedEnvironment home{"HOME"}, xdg{"XDG_DATA_HOME"};
	fs::path root;
	StoreFixture() {
		char pattern[] = "/tmp/registry-class-XXXXXX";
		const char *created = mkdtemp(pattern);
		if (!created) { perror("mkdtemp"); std::abort(); }
		root = created;
		if (setenv("HOME", (root / "home").c_str(), 1) != 0 ||
			setenv("XDG_DATA_HOME", (root / "xdg").c_str(), 1) != 0)
			std::abort();
	}
	fs::path file() const { return root / "xdg/CnCGeneralsZH-Reborn/registry.ini"; }
};

std::string readFile(const fs::path &path) {
	std::ifstream input(path);
	CHECK(input.is_open());
	return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}
}

TEST(registry_class_linux_sub_key_is_the_section) {
	StoreFixture fixture;
	RegistryClass registry("SOFTWARE\\Westwood\\Test");
	CHECK(registry.Is_Valid());
	registry.Set_Int("Number", 7);

	const std::string contents = readFile(fixture.file());
	CHECK(contents.find("[SOFTWARE\\Westwood\\Test]") != std::string::npos);
	CHECK(contents.find("Number=DWORD:7") != std::string::npos);

	// A different sub_key is a different key: it cannot read this one's values.
	RegistryClass other("SOFTWARE\\Westwood\\Other");
	CHECK_EQ(other.Get_Int("Number", -1), -1);
}

TEST(registry_class_linux_signed_int_roundtrip_keeps_the_bits) {
	StoreFixture fixture;
	RegistryClass registry("SOFTWARE\\Westwood\\Test");
	registry.Set_Int("Number", -1);
	CHECK_EQ(registry.Get_Int("Number", 0), -1);
	registry.Set_Int("High", 0x7FFFFFFF);
	CHECK_EQ(registry.Get_Int("High", 0), 0x7FFFFFFF);
	// A value the store cannot represent as a DWORD falls back to the default.
	CHECK_EQ(registry.Get_Int("Missing", 42), 42);
}

TEST(registry_class_linux_bool_and_float_roundtrip) {
	StoreFixture fixture;
	RegistryClass registry("SOFTWARE\\Westwood\\Test");
	registry.Set_Bool("Flag", true);
	CHECK(registry.Get_Bool("Flag", false));
	CHECK(registry.Get_Bool("Absent", false) == false);

	registry.Set_Float("Gain", 0.25f);
	CHECK_NEAR(registry.Get_Float("Gain", 0.0f), 0.25f, 1e-6f);
	CHECK_NEAR(registry.Get_Float("Absent", 1.5f), 1.5f, 1e-6f);

	// Floats are stored as the four raw bytes, the same way REG_DWORD did.
	const std::string contents = readFile(fixture.file());
	CHECK(contents.find("Gain=BINARY:0000803e") != std::string::npos);
}

TEST(registry_class_linux_strings_keep_punctuation) {
	StoreFixture fixture;
	RegistryClass registry("SOFTWARE\\Westwood\\Test");
	registry.Set_String("Name", "a=b; c");

	char buffer[32] = "";
	CHECK(registry.Get_String("Name", buffer, sizeof(buffer), "default"));
	CHECK_STR(buffer, "a=b; c");

	StringClass text;
	registry.Get_String("Name", text);
	CHECK_STR((const char *)text, "a=b; c");

	// Values are hex, so INI punctuation in the value is not mistaken for syntax.
	CHECK(readFile(fixture.file()).find("Name=SZ:613d623b2063") != std::string::npos);

	char fallback[16] = "";
	registry.Get_String("Absent", fallback, sizeof(fallback), "def");
	CHECK_STR(fallback, "def");
}

TEST(registry_class_linux_binary_and_enumeration) {
	StoreFixture fixture;
	RegistryClass registry("SOFTWARE\\Westwood\\Test");
	const unsigned char blob[] = {0x00, 0xFF, 0x42};
	registry.Set_Bin("Blob", blob, sizeof(blob));
	CHECK_EQ(registry.Get_Bin_Size("Blob"), 3);

	unsigned char out[4] = {0, 0, 0, 0};
	registry.Get_Bin("Blob", out, 3);
	CHECK_MEM(out, blob, 3);

	DynamicVectorClass<StringClass> keys;
	registry.Set_Int("Number", 1);
	registry.Get_Value_List(keys);
	CHECK_EQ(keys.Count(), 2);
	bool sawBlob = false, sawNumber = false;
	for (int i = 0; i < keys.Count(); ++i) {
		if (strcmp((const char *)keys[i], "Blob") == 0) sawBlob = true;
		if (strcmp((const char *)keys[i], "Number") == 0) sawNumber = true;
	}
	CHECK(sawBlob && sawNumber);

	registry.Delete_Value("Blob");
	CHECK_EQ(registry.Get_Bin_Size("Blob"), 0);
	registry.Deleta_All_Values();
	DynamicVectorClass<StringClass> empty;
	registry.Get_Value_List(empty);
	CHECK_EQ(empty.Count(), 0);
}

TEST(registry_class_linux_read_only_store_refuses_writes) {
	StoreFixture fixture;
	// A read-only instance has nothing to open until something has been stored, so
	// the key is not valid and opening it leaves no file behind.
	RegistryClass::Set_Read_Only(true);
	RegistryClass locked("SOFTWARE\\Westwood\\Test");
	CHECK(!locked.Is_Valid());
	CHECK(!fs::exists(fixture.file()));
	RegistryClass::Set_Read_Only(false);

	RegistryClass writable("SOFTWARE\\Westwood\\Test");
	CHECK(writable.Is_Valid());
	writable.Set_Int("Number", 1);
	CHECK_EQ(writable.Get_Int("Number", 0), 1);

	RegistryClass::Set_Read_Only(true);
	RegistryClass reopened("SOFTWARE\\Westwood\\Test");
	CHECK(reopened.Is_Valid());
	reopened.Set_Int("Number", 5);
	CHECK_EQ(reopened.Get_Int("Number", 0), 1);
	RegistryClass::Set_Read_Only(false);
}

TEST(registry_class_linux_exists_reports_only_stored_sections) {
	StoreFixture fixture;
	RegistryClass registry("SOFTWARE\\Westwood\\Test");
	CHECK(!RegistryClass::Exists("SOFTWARE\\Westwood\\Test"));
	registry.Set_Int("Number", 1);
	CHECK(RegistryClass::Exists("SOFTWARE\\Westwood\\Test"));
	CHECK(!RegistryClass::Exists("SOFTWARE\\Westwood\\Other"));
}
