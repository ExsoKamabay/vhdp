# ADR 0003: Kebijakan Exception dan RTTI

Status: diterima (v0.1.0)

## Keputusan

- **Core tanpa RTTI** (`-fno-rtti` untuk semua target first-party). Tidak ada
  `dynamic_cast`/`typeid` di jalur produksi.
- **Model hasil eksplisit `Status`/`Result<T>`** (`src/common/status.hpp`) untuk
  semua kegagalan yang diharapkan. Exception **bukan** mekanisme kontrol alur.
- Exception dibiarkan aktif (default) karena sebagian STL (`std::bad_alloc`,
  `std::length_error`) dapat melemparnya, tetapi:
  - **Tidak ada exception yang boleh keluar dari C ABI, callback C, thread entry,
    atau (kelak) JNI.** Setiap entry C ABI membungkus body dalam `guarded(...)`
    yang mengubah `std::bad_alloc` menjadi `VHDP_E_NO_MEMORY` dan exception lain
    menjadi `VHDP_E_INTERNAL`.
  - Trampoline callback C++ (`vhdp.hpp`) menangkap exception dari callback user dan
    menuliskannya ke stderr, tidak meneruskannya ke C.
  - Destructor/cleanup efektif `noexcept`.
- Alokasi/ukuran yang berasal dari guest **dibatasi** sebelum dialokasikan
  sehingga jalur nothrow diprioritaskan dan OOM tidak dipicu oleh input.

## Konsekuensi

- API publik (C dan wrapper C++) tidak melempar; konsumen memakai kode status.
- Karena RTTI mati, hierarki `Engine`/`EngineInstance` memakai virtual dispatch
  biasa tanpa `dynamic_cast`.
