# ADR 0008: Packaging Android dan batas W^X

Status: diterima (v0.1.0), diperbarui pada v1.0.0

## Keputusan

- Build memakai toolchain resmi NDK yang dipin. Artefak `arm64-v8a` dan `x86_64`
  kompatibel dengan page 16 KiB (`-Wl,-z,max-page-size=16384`).
- Profil dipisah tegas (lihat [CAPABILITIES.md](../CAPABILITIES.md)).
  `terminal-unprivileged` bisa dipakai bila probe `ptrace` lolos. `android-app`
  (targetSdk 29 ke atas) tidak boleh meng-`execve` berkas dari penyimpanan
  aplikasi yang bisa ditulis, jadi program guest dimuat oleh loader userland
  (`vhdp-loader`, dipasang sebagai `libvhdp-loader.so` di direktori pustaka
  native) lewat `mmap(PROT_EXEC)`. `targetSdkVersion` ditentukan aplikasi
  pemakai.
- Project ini tidak menyediakan lapisan JNI atau paket AAR. Aplikasi yang
  membutuhkannya menulis jembatan JNI sendiri, terpisah dari C ABI publik: ekspor
  hanya `JNI_OnLoad` dengan pendaftaran native dinamis, satu strategi libc++ per
  proses, dan tidak ada duplikasi `libc++_shared.so` antar library.

## Larangan untuk produksi

Jangan menurunkan `targetSdkVersion`, membypass SELinux, memakai exploit, meminta
mode permissive, atau menyamarkan executable sebagai solusi W^X. Bila sebuah
profil belum punya jalur yang sah, nyatakan profil itu belum didukung.

## Rootfs

Impor lewat URI atau file descriptor ke penyimpanan internal aplikasi; jangan
memakai `/sdcard` sebagai root POSIX. `phdp doctor` mendeteksi domain SELinux
aplikasi dan kebijakan penyimpanan executable, lalu melaporkan kelayakannya.

## Riwayat status

- v0.1.0: `android-app` berstatus `unsupported` karena belum ada jalur exec yang
  sah; JNI dan AAR ditunda.
- v1.0.0: loader userland menjadi jalur exec yang sah. `android-app` berstatus
  `supported` setelah diuji di ponsel arm64 Android 15 (API 35, SELinux
  enforcing) dan Android 13 x86_64. Profil `terminal-unprivileged` dan
  `android-emulator-x86_64` tetap `untested` sampai dijalankan di perangkat dan
  dicatat.
