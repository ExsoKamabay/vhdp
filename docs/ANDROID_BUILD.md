# Membangun VHDP untuk Android

## Prasyarat

- Android NDK r29 (`29.0.14206865`), dipin oleh `VHDP_NDK_VERSION`.
- CMake 3.22 atau lebih baru, dan Ninja.

Lokasi NDK dicari berurutan: `-DVHDP_ANDROID_NDK=...`, variabel environment
`VHDP_ANDROID_NDK`, `ANDROID_NDK_HOME`, `ANDROID_NDK_ROOT`, `ANDROID_NDK`, lalu
`$ANDROID_HOME/ndk/<versi>`, `$ANDROID_SDK_ROOT/ndk/<versi>`, dan
`$HOME/Android/Sdk/ndk/<versi>`. Versi NDK lain tidak dipilih diam-diam; pesan
error menyebut versi yang terpasang. NDK bisa dipasang dengan
`sdkmanager "ndk;29.0.14206865"`.

## Build

```sh
# perangkat arm64
cmake --preset android-arm64-release
cmake --build --preset android-arm64-release --parallel

# emulator atau perangkat x86_64
cmake --preset android-x86_64-debug
cmake --build --preset android-x86_64-debug --parallel
```

Preset memakai toolchain resmi NDK dengan `ANDROID_PLATFORM=android-23`,
`ANDROID_STL=c++_static`, dan `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON`. Hasilnya
ada di `build/<preset>/src/`: `libvhdp.so`, `libvhdp.a`, `phdp`, dan
`vhdp-loader`.

Linker menambahkan `-Wl,-z,max-page-size=16384` sehingga biner cocok untuk
perangkat dengan page 16 KiB. Cara memeriksanya:

```sh
llvm-readelf -l build/android-arm64-release/src/libvhdp.so | grep LOAD   # align 0x4000
llvm-readelf -n build/android-arm64-release/src/libvhdp.so | grep -i BTI # BTI, PAC
llvm-nm -D --defined-only build/android-arm64-release/src/libvhdp.so     # hanya vhdp_*
```

Test dan contoh otomatis dimatikan saat cross-compile karena harus dijalankan di
host. Untuk membangunnya bagi perangkat, aktifkan `-DVHDP_BUILD_TESTS=ON` lalu
jalankan biner test di perangkat.

## Memasang di aplikasi

Aplikasi dengan targetSdk 29 ke atas hanya boleh meng-`execve` berkas dari
direktori pustaka native (`nativeLibraryDir`), dan package manager hanya
memasang berkas bernama `lib*.so` di sana. Susunannya:

| Hasil build | Nama di aplikasi | Keterangan |
|---|---|---|
| `libvhdp.so` | `libvhdp.so` | Library, bila aplikasi memanggil C ABI langsung. |
| `phdp` | mis. `libphdp.so` | CLI yang di-`execve` aplikasi untuk menjalankan sesi. |
| `vhdp-loader` | `libvhdp-loader.so` | Nama ini yang dicari `platform::find_userland_loader`, di direktori yang sama dengan modul yang berisi `libvhdp`. |

`$VHDP_LOADER` bisa menunjuk lokasi lain untuk loader. Tanpa loader, profil
`android-app` tidak tersedia dan `doctor` melaporkan `exec.storage_policy` gagal.

## Minimum API dan batas platform

- API minimum ditentukan oleh API yang dipakai. `android-23` cukup untuk
  `PTRACE_SEIZE`, `seccomp`, `pidfd`, dan `close_range` (dengan fallback saat
  runtime bila tidak ada). Aplikasi pemakai menentukan `targetSdkVersion`-nya
  sendiri.
- Profil terminal (`terminal-unprivileged`, dijalankan dari aplikasi terminal
  atau `adb shell`): engine rootless bisa dipakai bila probe `ptrace` terhadap
  child lolos. Jalankan `phdp doctor` untuk memeriksa.
- Profil aplikasi (`android-app`, targetSdk 29 ke atas): `execve` dari
  penyimpanan aplikasi yang bisa ditulis diblokir, jadi program guest tidak
  dijalankan lewat `execve`. Yang di-`execve` hanya berkas di direktori pustaka
  native (CLI dan `libvhdp-loader.so`); ELF guest dipetakan loader userland
  dengan `mmap(PROT_EXEC)`, jalur yang diizinkan platform. `phdp doctor`
  melaporkan `exec.storage_policy = pass` bila loader ada dan probe pemetaan
  tidak ditolak, dan `fail` bila salah satunya tidak terpenuhi. Profil ini
  `supported`: sudah diuji di ponsel arm64 Android 15 dan di Android 13 x86_64.
  Kode guest berjalan di konteks keamanan aplikasi dengan izin aplikasi, tanpa
  hak tambahan.
- Jangan menurunkan `targetSdkVersion`, membypass SELinux, atau menyamarkan
  executable sebagai solusi W^X di produksi.

## Rootfs di Android

Rootfs harus diimpor lewat URI atau file descriptor ke penyimpanan internal
aplikasi. Jangan memakai `/sdcard` sebagai root POSIX: tidak ada jaminan untuk
permission, ownership, symlink, case-sensitivity, dan bit executable. Lihat
[ROOTFS.md](ROOTFS.md).

## JNI dan AAR

Project ini tidak membawa lapisan JNI maupun paket AAR. Aplikasi yang ingin
memanggil `libvhdp` dari kode JVM menulis jembatan JNI sendiri di atas C ABI.
Pedoman dari ADR 0008: ekspor hanya `JNI_OnLoad` dan daftarkan native secara
dinamis, buat dan hancurkan `vhdp_context` per panggilan atau per sesi, kirim
string sebagai byte array UTF-8, dan biarkan exception C++ ditahan di batas C
ABI. Tetapkan satu strategi libc++ per proses; bila beberapa library native
memakai C++, gunakan `c++_shared` untuk seluruh aplikasi agar `libc++_shared.so`
tidak terduplikasi.

## Catatan pengujian

Host pengembangan adalah x86_64, sehingga biner arm64 tidak bisa dijalankan di
host. Profil `android-app` sudah diuji di perangkat. Profil
`terminal-unprivileged` dan `android-emulator-x86_64` tetap `untested` sampai
dijalankan di perangkat atau emulator dan hasilnya dicatat.
