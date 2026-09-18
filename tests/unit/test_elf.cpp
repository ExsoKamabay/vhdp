#include "vtest.hpp"

#include "linux_abi/elf.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

// Builds an ELF64 header + program header table in a buffer.
std::vector<std::uint8_t> make_elf(std::uint32_t machine, std::uint16_t type, bool with_interp,
                                   const char* interp) {
    std::vector<std::uint8_t> b(64 + 2 * 56 + 64, 0);
    std::memcpy(b.data(), "\177ELF", 4);
    b[4] = 2; // ELFCLASS64
    b[5] = 1; // LSB
    b[6] = 1;
    auto put16 = [&](std::size_t o, std::uint16_t v) {
        b[o] = static_cast<std::uint8_t>(v & 0xff);
        b[o + 1] = static_cast<std::uint8_t>((v >> 8) & 0xff);
    };
    auto put32 = [&](std::size_t o, std::uint32_t v) {
        for (unsigned i = 0; i < 4; ++i)
            b[o + i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xff);
    };
    auto put64 = [&](std::size_t o, std::uint64_t v) {
        for (unsigned i = 0; i < 8; ++i)
            b[o + i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xff);
    };
    put16(16, type);
    put16(18, static_cast<std::uint16_t>(machine));
    put32(20, 1);
    put64(32, 64); // phoff
    put16(52, 64); // ehsize
    put16(54, 56); // phentsize
    put16(56, with_interp ? 2 : 1);
    // PT_LOAD
    put32(64, 1);
    if (with_interp) {
        std::size_t ph = 64 + 56;
        std::size_t interp_off = 64 + 2 * 56;
        std::size_t len = std::strlen(interp) + 1;
        put32(ph, 3); // PT_INTERP
        put64(ph + 8, interp_off);
        put64(ph + 32, static_cast<std::uint64_t>(len));
        std::memcpy(b.data() + interp_off, interp, len);
    }
    return b;
}

int scan(std::vector<std::uint8_t>& b, vhdp_elf_info& info) {
    int r = vhdp_elf_parse_header(b.data(), b.size(), b.size(), &info);
    if (r != VHDP_ELF_OK) {
        return r;
    }
    return vhdp_elf_scan_phdrs(b.data() + info.phoff, b.size() - info.phoff, b.size(), &info);
}

} // namespace

VTEST(elf_static) {
    auto b = make_elf(vhdp_elf_host_machine(), 2, false, "");
    vhdp_elf_info info{};
    CHECK_EQ(scan(b, info), VHDP_ELF_OK);
    CHECK_EQ(info.elf_class, 64u);
    CHECK_EQ(info.machine, vhdp_elf_host_machine());
    CHECK_EQ(info.has_interp, 0u);
}

VTEST(elf_dynamic) {
    auto b = make_elf(vhdp_elf_host_machine(), 3, true, "/lib/ld.so");
    vhdp_elf_info info{};
    CHECK_EQ(scan(b, info), VHDP_ELF_OK);
    CHECK_EQ(info.has_interp, 1u);
    CHECK_EQ(vhdp_elf_validate_interp(b.data() + info.interp_offset, info.interp_size),
             VHDP_ELF_OK);
}

VTEST(elf_malformed) {
    std::vector<std::uint8_t> tiny = {0x7f, 'E', 'L', 'F'};
    vhdp_elf_info info{};
    CHECK_EQ(vhdp_elf_parse_header(tiny.data(), tiny.size(), tiny.size(), &info),
             VHDP_ELF_E_TRUNCATED);

    std::uint8_t junk[64] = {'n', 'o', 't', 'e', 'l', 'f'};
    CHECK_EQ(vhdp_elf_parse_header(junk, sizeof(junk), sizeof(junk), &info), VHDP_ELF_E_NOT_ELF);

    // Interp not NUL-terminated / not absolute.
    const std::uint8_t rel[] = {'l', 'i', 'b', 0};
    CHECK_EQ(vhdp_elf_validate_interp(rel, sizeof(rel)), VHDP_ELF_E_BAD_INTERP);
    const std::uint8_t noterm[] = {'/', 'a', 'b'};
    CHECK_EQ(vhdp_elf_validate_interp(noterm, sizeof(noterm)), VHDP_ELF_E_BAD_INTERP);

    // phoff beyond the file.
    auto b = make_elf(vhdp_elf_host_machine(), 2, false, "");
    b[32] = 0xff;
    b[33] = 0xff;
    vhdp_elf_info info2{};
    CHECK_EQ(vhdp_elf_parse_header(b.data(), b.size(), b.size(), &info2), VHDP_ELF_E_BAD_PHDR);
}

VTEST(elf_foreign) {
    std::uint32_t foreign =
        vhdp_elf_host_machine() == VHDP_EM_X86_64 ? VHDP_EM_AARCH64 : VHDP_EM_X86_64;
    auto b = make_elf(foreign, 2, false, "");
    vhdp_elf_info info{};
    CHECK_EQ(scan(b, info), VHDP_ELF_OK);
    CHECK_NE(info.machine, vhdp_elf_host_machine());
    CHECK(std::string(vhdp_elf_machine_name(foreign)) != "unknown");

    // 32-bit class reports UNSUPPORTED_CLASS but still fills machine.
    auto b32 = make_elf(vhdp_elf_host_machine(), 2, false, "");
    b32[4] = 1; // ELFCLASS32
    vhdp_elf_info info32{};
    CHECK_EQ(vhdp_elf_parse_header(b32.data(), b32.size(), b32.size(), &info32),
             VHDP_ELF_E_UNSUPPORTED_CLASS);
    CHECK_EQ(info32.elf_class, 32u);
}
