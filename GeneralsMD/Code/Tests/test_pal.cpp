/*
** Command & Conquer Generals ZH Reborn
** Platform Abstraction Layer — Unit Tests
*/

#include "test_harness.h"
#include "PAL_Timing.h"
#include "PAL_DynamicLibrary.h"
#include "PAL_FileSystem.h"

#include <cmath>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

TEST(pal_timing_precision_timer)
{
    Platform_InitPrecisionTimer();
    uint64_t t1 = Platform_GetPrecisionTimer();
    CHECK(t1 > 0);
    uint64_t t2;
    Platform_GetPrecisionTimer(&t2);
    CHECK(t2 >= t1);
    CHECK(Platform_GetTicksPerSec() > 0);
    CHECK(Platform_GetTicksPerMSec() > 0.0);
}

TEST(pal_timing_float_to_long)
{
    CHECK_EQ(Platform_FloatToLong(0.0f), 0L);
    CHECK_EQ(Platform_FloatToLong(42.0f), 42L);
    CHECK_EQ(Platform_FloatToLong(-17.0f), -17L);
    CHECK_EQ(Platform_FloatToLong(3.99f), 3L);
    CHECK_EQ(Platform_FloatToLong(-3.99f), -3L);
}

TEST(pal_timing_crc32)
{
    alignas(8) const char msg1[] = "Command & Conquer Generals Zero Hour";
    alignas(8) const char msg2[] = "Command & Conquer Generals Zero Hour";
    alignas(8) const char msg3[] = "Command & Conquer Generals Reborn";

    uint32_t crc1 = Platform_CRC32(msg1, sizeof(msg1) - 1);
    uint32_t crc2 = Platform_CRC32(msg2, sizeof(msg2) - 1);
    uint32_t crc3 = Platform_CRC32(msg3, sizeof(msg3) - 1);

    CHECK(crc1 != 0);
    CHECK_EQ(crc1, crc2);
    CHECK(crc1 != crc3);
}

TEST(pal_timing_spinlock)
{
    PlatformSpinLock spinlock;
    {
        PlatformSpinLock::ScopedLock guard(spinlock);
    }
    // Re-lockable after release
    spinlock.lock();
    spinlock.unlock();
}

TEST(pal_timing_tls)
{
    PlatformTLS<int> tls;
    CHECK(tls.get() == nullptr);
    int val = 1234;
    tls.set(&val);
    CHECK_EQ(tls.get(), &val);
    CHECK_EQ(*tls.get(), 1234);
    tls.set(nullptr);
    CHECK(tls.get() == nullptr);
}

TEST(pal_filesystem_normalize_path)
{
    std::string p1 = "Data\\INI\\GameData.ini";
    Platform_NormalizePath(p1);
    CHECK_EQ(p1, "Data/INI/GameData.ini");

    std::string p2 = "Data\\\\\\INI//GameData.ini";
    Platform_NormalizePath(p2);
    CHECK_EQ(p2, "Data/INI/GameData.ini");

    std::string p3 = "/unix/path/file.txt";
    Platform_NormalizePath(p3);
    CHECK_EQ(p3, "/unix/path/file.txt");
}

TEST(pal_filesystem_case_resolution)
{
    fs::path temp_dir = fs::temp_directory_path() / "pal_test_dir_case";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir / "SubDir" / "Nested", ec);

    std::string file_path = (temp_dir / "SubDir" / "Nested" / "SampleFile.TXT").string();
    {
        std::ofstream out(file_path);
        out << "Hello PAL";
    }

    // Exact path
    std::string resolved;
    bool found = Platform_ResolveCasePath(file_path, resolved);
    CHECK(found);
    CHECK_EQ(resolved, file_path);

    // Mismatched casing: "subdir/nested/samplefile.txt"
    std::string test_query = (temp_dir / "subdir" / "nested" / "samplefile.txt").string();
    found = Platform_ResolveCasePath(test_query, resolved);
    CHECK(found);
    CHECK_EQ(resolved, file_path);

    // Backslash style: "SUBDIR\\NESTED\\SAMPLEFILE.TXT"
    std::string bslash_query = temp_dir.string() + "/SUBDIR\\NESTED\\SAMPLEFILE.TXT";
    found = Platform_ResolveCasePath(bslash_query, resolved);
    CHECK(found);
    CHECK_EQ(resolved, file_path);

    // Non-existent path
    std::string missing_query = (temp_dir / "nonexistent" / "file.txt").string();
    found = Platform_ResolveCasePath(missing_query, resolved);
    CHECK(!found);

    fs::remove_all(temp_dir, ec);
}

TEST(pal_filesystem_directories)
{
    std::string user_data = Platform_GetUserDataDir();
    CHECK(!user_data.empty());
    CHECK(user_data.find("CnCGeneralsZH-Reborn") != std::string::npos);

    std::string user_config = Platform_GetUserConfigDir();
    CHECK(!user_config.empty());
    CHECK(user_config.find("CnCGeneralsZH-Reborn") != std::string::npos);
}

TEST(pal_dynamic_library_loading)
{
    // Load standard math library
    void* lib = Platform_LoadLibrary("libm.so.6");
    if (!lib) {
        // Fallback for systems where libm is linked under different name
        lib = Platform_LoadLibrary("libc.so.6");
    }
    CHECK(lib != nullptr);

    typedef double (*CosFn)(double);
    CosFn cos_fn = reinterpret_cast<CosFn>(Platform_GetProcAddress(lib, "cos"));
    CHECK(cos_fn != nullptr);
    double res = cos_fn(0.0);
    CHECK(std::abs(res - 1.0) < 1e-6);

    CHECK(Platform_FreeLibrary(lib));
}
