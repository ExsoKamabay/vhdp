// Minimal self-contained test framework (no network dependency, no gtest).
// Each case is registered with CTest individually: `<binary> <case_name>`.
// Exit status: 0 pass, 1 fail, 77 skipped (CTest SKIP_RETURN_CODE).
#pragma once

#include <cstdio>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace vtest {

struct Case {
    const char* name;
    void (*fn)();
};

std::vector<Case>& registry();

struct Registrar {
    Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

struct Abort {};
struct Skip {
    std::string reason;
};

void record_failure(const char* file, int line, const std::string& what);
int failure_count();

template <class T>
concept Streamable = requires(std::ostream& os, const T& v) { os << v; };

template <class T>
std::string show(const T& v) {
    if constexpr (std::is_enum_v<T>) {
        return std::to_string(static_cast<long long>(static_cast<std::underlying_type_t<T>>(v)));
    } else if constexpr (Streamable<T>) {
        std::ostringstream os;
        os << v;
        return os.str();
    } else {
        return "<value>";
    }
}
inline std::string show(const std::string& v) {
    return "\"" + v + "\"";
}
inline std::string show(std::string_view v) {
    return "\"" + std::string(v) + "\"";
}
inline std::string show(const char* v) {
    return v != nullptr ? "\"" + std::string(v) + "\"" : "(null)";
}
inline std::string show(bool v) {
    return v ? "true" : "false";
}
inline std::string show(std::nullptr_t) {
    return "nullptr";
}

} // namespace vtest

#define VTEST(name)                                                                                \
    static void vtest_##name();                                                                    \
    static const ::vtest::Registrar vtest_reg_##name(#name, &vtest_##name);                        \
    static void vtest_##name()

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            ::vtest::record_failure(__FILE__, __LINE__, "CHECK(" #cond ")");                       \
        }                                                                                          \
    } while (0)

#define REQUIRE(cond)                                                                              \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            ::vtest::record_failure(__FILE__, __LINE__, "REQUIRE(" #cond ")");                     \
            throw ::vtest::Abort{};                                                                \
        }                                                                                          \
    } while (0)

#define VTEST_CMP_(op, a, b, fatal)                                                                \
    do {                                                                                           \
        auto vtest_a_ = (a);                                                                       \
        auto vtest_b_ = (b);                                                                       \
        if (!(vtest_a_ op vtest_b_)) {                                                             \
            ::vtest::record_failure(__FILE__, __LINE__,                                            \
                                    std::string(#a " " #op " " #b) + "  [" +                       \
                                        ::vtest::show(vtest_a_) + " vs " +                         \
                                        ::vtest::show(vtest_b_) + "]");                            \
            if (fatal) {                                                                           \
                throw ::vtest::Abort{};                                                            \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define CHECK_EQ(a, b) VTEST_CMP_(==, a, b, false)
#define CHECK_NE(a, b) VTEST_CMP_(!=, a, b, false)
#define REQUIRE_EQ(a, b) VTEST_CMP_(==, a, b, true)
#define REQUIRE_NE(a, b) VTEST_CMP_(!=, a, b, true)
#define VTEST_SKIP(reason)                                                                         \
    throw ::vtest::Skip {                                                                          \
        reason                                                                                     \
    }
