# ADR 0001: Pembagian C++ / C / Assembly

Status: diterima (v0.1.0)

## Keputusan

- **C++20** untuk core/orchestrator, state machine, lifecycle, engine abstraction,
  concurrency, CLI, dan wrapper API. RAII, value semantics, `std::unique_ptr`,
  `std::span`, ownership eksplisit. Tanpa owning raw pointer / global mutable
  state / singleton tersembunyi di hot path tanpa bukti.
- **C17** untuk C ABI publik, struktur wire/FFI, ELF/syscall decoder yang butuh
  layout stabil, dan kode yang berjalan **setelah `fork()` sebelum `execve()`**
  (harus async-signal-safe: hanya raw syscall, tanpa errno/TLS/alloc/lock).
- **Assembly `.S`** hanya untuk boundary per-arsitektur yang benar-benar memerlukan
  kontrol register/instruction: **raw syscall gateway** (`src/arch/*/raw_syscall.S`)
  yang memetakan argumen ke konvensi syscall kernel tanpa menyentuh errno/TLS,
  dan dipakai oleh bootstrap tracee serta probe.

## Alasan assembly esensial di sini

Konvensi syscall kernel berbeda dari ABI pemanggil (x86_64: arg4 di `r10`, bukan
`rcx`; aarch64: nomor di `x8` via `svc #0`) dan harus bebas errno/TLS agar aman di
child multithreaded pasca-`fork`. Wrapper libc (`syscall(3)`) menyentuh errno
(TLS) dan bukan kontrak yang bisa kita jamin async-signal-safe di semua bionic/glibc.

## Kewajiban

- Referensi portable C ada (`raw_syscall_portable.c`) dan diekspor untuk
  differential test (`T-UNIT-RAW-SYSCALL`, `T-DIFF-RAW-SYSCALL-REGS`:
  kesetaraan hasil, preservasi callee-saved, netralitas errno/stack).
- Opsi `VHDP_ENABLE_ASM`/`VHDP_FORCE_PORTABLE`; arch tak didukung memakai stub
  portable. Business logic/parser/resolver/state machine **tidak** ditulis di asm.
- Tiap fungsi asm mendokumentasikan calling convention, register in/out, clobber,
  stack alignment, CFI/unwind, asumsi TLS/signal, endianness, dan fitur CPU. Pada
  Android AArch64 register platform (`x18`) dihormati; ada landing pad BTI dan
  GNU property IBT/SHSTK (x86_64) serta BTI/PAC (aarch64).
- Optimasi asm hanya di-merge bila profiling menemukan hot path dan benchmark
  menunjukkan kemenangan material tanpa regresi correctness/stabilitas/daya/arch
  lain.
