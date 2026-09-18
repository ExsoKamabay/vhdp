# Menyematkan `libvhdp` (C dan C++)

CLI `phdp` adalah facade tipis di atas library yang sama. Pemakai native memakai:

- C ABI stabil: [`include/vhdp/vhdp.h`](../include/vhdp/vhdp.h)
- Wrapper C++20 RAII (header-only): [`include/vhdp/vhdp.hpp`](../include/vhdp/vhdp.hpp)

Contoh lengkap: [`examples/c/embed_run.c`](../examples/c/embed_run.c),
[`examples/cpp/embed_run.cpp`](../examples/cpp/embed_run.cpp).

## Kontrak ABI

- Setiap struct berversi diawali `{uint32_t struct_size; uint32_t abi_version;}`.
  Isi `struct_size = sizeof(T)` dan `abi_version = VHDP_ABI_VERSION`. Field
  reserved harus nol.
- Handle bersifat opaque dan dimiliki pemanggil antara `*_create` dan `*_destroy`.
- Status/flag/kind adalah `typedef` integer fixed-width dengan konstanta bernama
  (mis. `vhdp_status_t`), bukan `enum` C, agar ukuran/ABI-nya stabil lintas
  toolchain.
- Tidak ada exception yang melewati boundary; kegagalan menjadi `vhdp_status_t`.
  Pesan terakhir per-thread diambil via `vhdp_last_error_message`.
- String masuk disalin (library tak menyimpan pointer pemanggil, kecuali pointer
  user callback). String keluar: statik, valid hanya selama callback, atau ditulis
  ke buffer pemanggil.

## Lifecycle minimal (C)

```c
vhdp_context_options o = { .struct_size = sizeof(o), .abi_version = VHDP_ABI_VERSION };
vhdp_context* ctx; vhdp_context_create(&o, &ctx);
vhdp_config* cfg; vhdp_config_create(ctx, &cfg);
vhdp_config_set_rootfs(cfg, "./rootfs");
const char* argv[] = { "/bin/echo", "hi" };
vhdp_config_set_argv(cfg, 2, argv);
vhdp_session* s; vhdp_session_create(ctx, cfg, &s);  /* config disalin */
vhdp_config_destroy(cfg);
vhdp_session_start(s);
vhdp_exit_info info = { .struct_size = sizeof(info), .abi_version = VHDP_ABI_VERSION };
vhdp_session_wait(s, -1, &info);      /* info.shell_status = exit-status gaya shell */
vhdp_session_destroy(s);
vhdp_context_destroy(ctx);            /* setelah semua session tujuan hancur */
```

## C++ RAII

```cpp
auto ctx = vhdp::Context::create();
auto cfg = vhdp::Config::create(ctx.value());
cfg.value().rootfs("./rootfs");
cfg.value().argv({"/bin/echo", "hi"});
auto s = vhdp::Session::create(ctx.value(), cfg.value());
s.value().start();
auto info = s.value().wait();                 // vhdp::Result<vhdp::ExitInfo>
return info ? info.value().shell_status : 125;
```

Wrapper C++ dikompilasi oleh konsumen dan hanya bergantung pada C ABI, sehingga
standar C++ konsumen tidak perlu sama dengan yang dipakai membangun `libvhdp`.
Wrapper tidak melempar exception; operasi yang bisa gagal mengembalikan
`vhdp::Status`/`vhdp::Result<T>`.

## Thread-safety dan callback

- Fungsi info (`vhdp_abi_version`, dst.) aman dari thread mana pun.
- `vhdp_context`: create/destroy session dari thread mana pun; query tersinkron.
- `vhdp_session`: start/wait/state/signal/resize/write_input/cancel aman dipanggil
  konkuren dari thread berbeda; `destroy` tidak boleh balapan dengan panggilan lain
  pada session yang sama.
- Callback event/output berjalan di thread internal library. Data yang diberikan
  ke callback hanya valid sampai callback kembali. Memanggil `wait`/`destroy` dari
  dalam callback session yang sama mengembalikan `VHDP_E_BUSY` (bukan deadlock).
  Callback harus cepat (sinkron; memberi backpressure ke guest).

## Laporan JSON dan fungsi bantu rootfs

Fungsi berikut menulis dokumen JSON UTF-8 berakhiran NUL ke buffer pemanggil.
`*needed` menerima ukuran yang dibutuhkan termasuk NUL. Bila `cap` terlalu kecil,
fungsi mengembalikan `VHDP_E_BUFFER_TOO_SMALL`. Laporan `doctor` dan `inspect`
di-cache di context, dan hasil probe engine untuk `capabilities` dihitung sekali
per proses, sehingga panggilan ulang dengan buffer yang lebih besar
mengembalikan dokumen yang sama. `vhdp_configure_rootfs` tidak di-cache:
panggilan ulang menjalankan konfigurasi lagi dan laporannya bisa berbeda, jadi
siapkan buffer yang cukup besar sejak panggilan pertama.

| Fungsi | Isi dokumen |
|---|---|
| `vhdp_doctor_json(ctx, flags, ...)` | Hasil probe host. `VHDP_DOCTOR_NO_ACTIVE_PROBES` melewatkan probe yang membuat proses anak, `VHDP_DOCTOR_REFRESH` mengabaikan cache. |
| `vhdp_capabilities_json(ctx, ...)` | Matriks kemampuan, status engine, jumlah syscall per kelas, registry hardware, katalog driver. |
| `vhdp_inspect_rootfs_json(ctx, path, ...)` | Pemeriksaan rootfs: filesystem, `noexec`, `os-release`, shell, ABI, loader. |
| `vhdp_configure_rootfs(ctx, path, ...)` | Menyiapkan rootfs untuk boot pertama dan melaporkan `{input, rootfs, status, actions[]}`. Menulis berkas di rootfs. |
| `vhdp_dpkg_plan_json(ctx, path, ...)` | Rencana pemulihan dpkg `{input, rootfs, is_dpkg_rootfs, status_db, locks[], needs_recovery, script}`. Tidak menjalankan apa pun. |
| `vhdp_projection_plan_json(ctx, ...)` | Data host dan ketersediaan `/dev`, `/proc`, `/sys` untuk diproyeksikan `{host, system_binds[], status}`. |

Tiga fungsi terakhir tidak memakai engine sehingga aman dipanggil dari proses
yang tidak boleh menjalankan ptrace. Script dari `vhdp_dpkg_plan_json` dijalankan
pemanggil di sesi dengan uid guest 0, misalnya dengan
`vhdp_config_set_argv(cfg, 3, (const char*[]){"/bin/sh", "-c", script})` dan
`vhdp_config_set_uid(cfg, 0)`. Rencana bind dari `vhdp_projection_plan_json`
diterapkan pemanggil dengan `vhdp_config_add_bind` untuk setiap entri yang
`available`.

## Menautkan

CMake package tersedia setelah install:

```cmake
find_package(VHDP REQUIRED)
target_link_libraries(app PRIVATE VHDP::vhdp)   # shared, C ABI
```

Atau tautkan `libvhdp.a` untuk executable mandiri. Hanya allowlist C ABI yang
diekspor; simbol C++/STL disembunyikan dan diverifikasi (`T-ABI-SYMBOLS`).
