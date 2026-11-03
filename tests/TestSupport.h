#pragma once
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <string>

// A deliberately small harness: the repository has no test framework and these tests only
// need to count failures, keep going and report where.
inline int g_testFailures = 0;
inline int g_testChecks = 0;

#define CHECK(condition)                                                         \
    do {                                                                         \
        g_testChecks++;                                                          \
        if (!(condition)) {                                                      \
            g_testFailures++;                                                    \
            std::printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition);   \
        }                                                                        \
    } while (0)

inline int testSummary(const char* suite) {
    std::printf("%s: %d checks, %d failed\n", suite, g_testChecks, g_testFailures);
    return g_testFailures == 0 ? 0 : 1;
}

// A unique scratch folder that is removed at the end of the test.
class TempDir {
public:
    TempDir() {
        std::random_device random;
        path = std::filesystem::temp_directory_path() / ("engine-test-" + std::to_string(random()));
        std::filesystem::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
    std::filesystem::path path;
};
