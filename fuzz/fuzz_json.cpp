// Fuzzes the JSON validator and writer-escaping against arbitrary bytes.
#include "common/json.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string_view sv(reinterpret_cast<const char*>(data), size);
    (void)vhdp::json_validate(sv);
    // Escaping must always yield valid JSON regardless of input bytes.
    std::string q = vhdp::json_quote(sv);
    if (!vhdp::json_validate(q)) {
        __builtin_trap();
    }
    return 0;
}
