// Compiling this proves include/vhdp/vhdp.hpp is self-contained C++20 and only
// depends on the C ABI (not on libvhdp internals).
#include "vhdp/vhdp.hpp"

#include <string>

std::string vhdp_test_header_cpp();
std::string vhdp_test_header_cpp() {
    vhdp::Status s = vhdp::Status::from_c(VHDP_E_TIMEOUT);
    vhdp::Result<int> r = 5;
    return std::string(s.name()) + std::to_string(r.ok() ? r.value() : 0) +
           std::to_string(static_cast<int>(vhdp::Engine::Rootless));
}
