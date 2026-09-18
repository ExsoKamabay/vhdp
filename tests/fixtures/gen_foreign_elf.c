/*
 * Emits a small but structurally valid ELF64 executable whose e_machine is NOT
 * the host machine, so the exec resolver reports an ABI mismatch (ENOEXEC). Only
 * the ELF header and one PT_LOAD program header are written; it is never executed.
 *   gen_foreign_elf <out-path>
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define EM_X86_64 62
#define EM_AARCH64 183

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s OUT\n", argv[0]);
        return 2;
    }
#if defined(__x86_64__)
    uint16_t foreign = EM_AARCH64;
#else
    uint16_t foreign = EM_X86_64;
#endif
    unsigned char buf[120];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "\177ELF", 4);
    buf[4] = 2;        /* ELFCLASS64 */
    buf[5] = 1;        /* ELFDATA2LSB */
    buf[6] = 1;        /* EV_CURRENT */
    uint16_t type = 2; /* ET_EXEC */
    memcpy(buf + 16, &type, 2);
    memcpy(buf + 18, &foreign, 2);
    uint32_t ver = 1;
    memcpy(buf + 20, &ver, 4);
    uint64_t phoff = 64;
    memcpy(buf + 32, &phoff, 8);
    uint16_t ehsize = 64;
    memcpy(buf + 52, &ehsize, 2);
    uint16_t phentsize = 56;
    memcpy(buf + 54, &phentsize, 2);
    uint16_t phnum = 1;
    memcpy(buf + 56, &phnum, 2);
    /* One PT_LOAD program header at offset 64. */
    uint32_t p_type = 1; /* PT_LOAD */
    memcpy(buf + 64, &p_type, 4);
    FILE* f = fopen(argv[1], "wb");
    if (f == NULL) {
        perror("fopen");
        return 1;
    }
    fwrite(buf, 1, sizeof(buf), f);
    fclose(f);
    (void)chmod(argv[1], 0755);
    return 0;
}
