# ADR 0002: Build, toolchain, dan baseline optimisasi

Status: diterima (v0.1.0)

## Keputusan

- **Clang/LLVM (NDK) sebagai compiler referensi**; CMake + Ninja + CTest; libc++
  untuk Android. `CMAKE_C_STANDARD 17`, `CMAKE_CXX_STANDARD 20`, ekstensi off.
- Preset konsisten: `host-debug`, `host-release`, `host-asan-ubsan`, `host-tsan`,
  `android-arm64-release`, `android-x86_64-debug`.
- **Baseline release `-O2`.** `-O3`/ThinLTO/PGO/ISA opsional hanya setelah
  benchmark. Dilarang pada artifact distribusi: `-Ofast`, `-ffast-math`,
  `-march=native`.
- **Hardening** yang didukung: PIE/PIC, `-fstack-protector-strong`, `_FORTIFY_SOURCE=2`
  (non-debug), RELRO+NOW, non-exec stack, branch protection sesuai ABI
  (`-fcf-protection=full` x86_64, `-mbranch-protection=standard` aarch64), linker
  `-z defs`. Warning-as-error hanya untuk sumber first-party (`vhdp_warnings`).
- **Android page 16 KiB**: `-Wl,-z,max-page-size=16384`; probe fitur ISA saat
  runtime/ per-target, tanpa asumsi page size/endianness dari nama versi Android.

## Catatan realitas toolchain

- Host Clang NDK dipakai sebagai referensi bila `clang` tidak ada di PATH; namun
  **sanitizer** memakai **GCC** karena host Clang NDK tidak menyertakan runtime
  sanitizer untuk target host `x86_64-linux-gnu`. Ini didokumentasikan di preset.
- TSan memerlukan ASLR mati; preset menyetel `VHDP_TEST_LAUNCHER=setarch;-R`.
- `libvhdp.so` menautkan libstdc++/libgcc statis pada build non-Android non-
  sanitizer agar simbol C++ tetap privat (disembunyikan version script).

## Dependency

Tidak ada dependency pihak ketiga; tidak ada unduhan network saat build/test
normal. NDK dipin (`VHDP_NDK_VERSION`).
