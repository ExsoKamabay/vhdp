# Troubleshooting

Mulailah dari `phdp doctor` (probe read-only dan probe aktif di proses anak
sekali pakai) dan `phdp inspect <rootfs>`.

## "no usable engine" atau `VHDP_E_ENGINE_UNAVAILABLE`

`phdp doctor` menunjukkan alasannya. Penyebab yang umum:

- `ptrace.child = FAIL`: kernel atau host melarang tracing terhadap child.
  Periksa `kernel.yama.ptrace_scope`; nilai 2 ke atas melarangnya. Di dalam
  container, runtime-nya harus memberi capability `SYS_PTRACE` dan tidak
  memblokir `ptrace` lewat profil seccomp.
- `exec.storage_policy = FAIL`: proses berjalan di domain SELinux aplikasi
  Android, tetapi loader userland `libvhdp-loader.so` tidak ditemukan di sebelah
  `libvhdp` (atau `$VHDP_LOADER` tidak menunjuk berkas executable), atau probe
  pemetaan `PROT_EXEC` ditolak. Lihat [ANDROID_BUILD.md](ANDROID_BUILD.md).
- `abi.host = FAIL`: arsitektur host berbeda dengan arsitektur yang dikompilasi
  untuk `libvhdp`.

## "this process is already traced"

`phdp run` menolak berjalan bila prosesnya sendiri sudah di-ptrace (TracerPid
bukan 0), misalnya karena dijalankan dari shell yang sudah berada di dalam sesi
VHDP, atau di bawah debugger. Linux hanya mengizinkan satu tracer per proses.
Jalankan dari shell yang tidak di-ptrace. `doctor`, `inspect`, dan
`capabilities` tetap bisa dipakai.

## Status keluar yang membingungkan

- `127`: perintah atau loader tidak ditemukan (`ENOENT`).
- `126`: tidak bisa dieksekusi: bukan ELF atau `#!` yang valid, ABI tidak cocok,
  atau berada di mount `noexec` (`ENOEXEC`/`EACCES`). `phdp inspect`
  menjelaskannya.
- `124`: `--timeout` tercapai dan pohon proses dibunuh.
- `125`: sesi gagal dimulai atau dibatalkan.
- `128+N`: guest dibunuh signal `N` (misalnya `130` untuk SIGINT/Ctrl-C).
- `2`: argumen `phdp` salah.

## "ABI mismatch" saat menjalankan

Rootfs dibuat untuk arsitektur lain. Engine rootless hanya mendukung arsitektur
yang sama. `phdp inspect` menampilkan `machine` untuk setiap shell. Rootfs lintas
arsitektur butuh backend emulator yang belum ada.

## "dynamic loader ... is missing"

ELF dinamis menunjuk `PT_INTERP` yang tidak ada di rootfs. Pastikan loader
(`/lib/ld-*.so`, `/lib64/ld-linux-*.so.2`) dan pustaka bersama ikut terpasang di
rootfs. Lihat [ROOTFS.md](ROOTFS.md).

## `cmd | head` menutup lebih awal atau muncul write error

Ini terkait penanganan SIGPIPE. CLI meng-`SIG_IGN` SIGPIPE untuk dirinya sendiri,
tetapi guest menerima disposisi default. Jalankan di shell guest yang menangani
SIGPIPE, atau laporkan sebagai masalah bila bisa diulang.

## PTY: Ctrl-C tidak sampai atau ukuran salah

Gunakan `--pty` agar guest mendapat terminal sendiri. Tanpa `--pty`, guest
berbagi terminal dengan `phdp`, dan signal terminal (SIGINT, SIGWINCH) sampai
langsung. Perubahan ukuran diteruskan sebagai `TIOCSWINSZ`.

## Toolchain tidak tersedia saat build

- Toolchain `host-clang` atau `host-gcc` gagal: pesan error menyebut cara
  memperbaikinya (pasang clang, atau set `ANDROID_NDK_HOME` ke NDK yang dipin
  agar host Clang dari NDK dipakai). Preset sanitizer memakai GCC karena host
  Clang dari NDK tidak membawa runtime sanitizer untuk host.
- `format-check` dan `tidy` melaporkan `NOT RUN` bila `clang-format` atau
  `clang-tidy` tidak ditemukan; set `VHDP_LLVM_BIN_DIR` ke direktori LLVM
  (misalnya dari NDK).
- Preset TSan memakai `setarch -R` (ASLR dimatikan) lewat `VHDP_TEST_LAUNCHER`.

## Pemeriksaan cepat

```sh
ctest --preset host-debug --output-on-failure
cmake --build --preset host-debug --target format-check tidy
```
