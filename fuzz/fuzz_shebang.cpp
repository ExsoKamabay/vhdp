// Fuzzes the #! interpreter-line parser.
#include "engines/rootless/exec_resolver.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string interp;
    std::string arg;
    bool has_arg = false;
    (void)vhdp::rootless::parse_shebang(reinterpret_cast<const char*>(data), size, interp, arg,
                                        has_arg);
    return 0;
}
