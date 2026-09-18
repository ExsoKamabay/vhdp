#include "vtest.hpp"

#include <cstdlib>
#include <cstring>
#include <exception>

namespace vtest {

namespace {
int g_failures = 0;
}

std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

void record_failure(const char* file, int line, const std::string& what) {
    ++g_failures;
    std::fprintf(stderr, "%s:%d: FAILED: %s\n", file, line, what.c_str());
}

int failure_count() {
    return g_failures;
}

namespace {

// 0 pass, 1 fail, 77 skip
int run_case(const Case& c) {
    int before = g_failures;
    try {
        c.fn();
    } catch (const Abort&) {
        // failure already recorded
    } catch (const Skip& s) {
        std::fprintf(stderr, "[ SKIP ] %s: %s\n", c.name, s.reason.c_str());
        return 77;
    } catch (const std::exception& e) {
        record_failure(c.name, 0, std::string("uncaught std::exception: ") + e.what());
    } catch (...) {
        record_failure(c.name, 0, "uncaught non-standard exception");
    }
    bool ok = g_failures == before;
    std::fprintf(stderr, "[ %s ] %s\n", ok ? " OK " : "FAIL", c.name);
    return ok ? 0 : 1;
}

} // namespace
} // namespace vtest

int main(int argc, char** argv) {
    auto& cases = vtest::registry();
    if (argc >= 2 && std::strcmp(argv[1], "--list") == 0) {
        for (const auto& c : cases) {
            std::printf("%s\n", c.name);
        }
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--prefix") == 0) {
        int worst = 0;
        int matched = 0;
        std::size_t plen = std::strlen(argv[2]);
        for (const auto& c : cases) {
            if (std::strncmp(c.name, argv[2], plen) != 0) {
                continue;
            }
            ++matched;
            int r = vtest::run_case(c);
            if (r == 1 || (r == 77 && worst == 0)) {
                worst = r;
            }
        }
        if (matched == 0) {
            std::fprintf(stderr, "no test case matches prefix '%s'\n", argv[2]);
            return 1;
        }
        std::fprintf(stderr, "%d cases matched '%s'\n", matched, argv[2]);
        return worst;
    }
    if (argc >= 2) {
        int worst = 0;
        for (int i = 1; i < argc; ++i) {
            bool found = false;
            for (const auto& c : cases) {
                if (std::strcmp(c.name, argv[i]) == 0) {
                    found = true;
                    int r = vtest::run_case(c);
                    if (r == 1 || (r == 77 && worst == 0)) {
                        worst = r;
                    }
                }
            }
            if (!found) {
                std::fprintf(stderr, "unknown test case: %s (use --list)\n", argv[i]);
                return 1;
            }
        }
        return worst;
    }
    int failed = 0;
    for (const auto& c : cases) {
        if (vtest::run_case(c) == 1) {
            ++failed;
        }
    }
    std::fprintf(stderr, "%zu cases, %d failed\n", cases.size(), failed);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
