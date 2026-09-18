// Fuzzes the ELF header/phdr decoder against arbitrary bytes.
#include "linux_abi/elf.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    vhdp_elf_info info{};
    uint64_t file_size = size;
    if (vhdp_elf_parse_header(data, size, file_size, &info) == VHDP_ELF_OK) {
        if (info.phoff < size) {
            vhdp_elf_scan_phdrs(data + info.phoff, size - static_cast<size_t>(info.phoff),
                                file_size, &info);
            if (info.has_interp && info.interp_offset < size) {
                vhdp_elf_validate_interp(
                    data + info.interp_offset,
                    static_cast<size_t>(info.interp_size < size - info.interp_offset
                                            ? info.interp_size
                                            : size - info.interp_offset));
            }
        }
    }
    return 0;
}
