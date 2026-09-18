// Fuzzes the lexical guest-path helpers (no filesystem access).
#include "vfs/guest_path.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string_view sv(reinterpret_cast<const char*>(data), size);
    (void)vhdp::vfs::is_absolute(sv);
    (void)vhdp::vfs::has_dotdot(sv);
    (void)vhdp::vfs::components(sv);
    if (vhdp::vfs::is_absolute(sv)) {
        std::string n = vhdp::vfs::normalize_lexical(sv);
        // Normalisation of an absolute path must never contain "..".
        if (vhdp::vfs::has_dotdot(n)) {
            __builtin_trap();
        }
    }
    return 0;
}
