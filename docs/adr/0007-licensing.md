# ADR 0007: Lisensi dan provenance

Status: diterima (v1.0.0)

## Keputusan

Project ini dilisensikan di bawah Apache License 2.0. Lisensi ini dipilih agar
penggunaan komersial, distribusi, dan perlindungan paten untuk library sistem
ini jelas.

- Implementasinya original. Dokumentasi resmi kernel dan platform hanya dibaca
  untuk memahami kontrak dan batas platform, tidak disalin.
- Tidak ada dependensi pihak ketiga yang disertakan di dalam sumber. Satu-satunya
  toolchain eksternal adalah Android NDK (versi dipin), dipakai sebagai compiler
  dan tidak ikut dalam artefak sumber.
- Setiap salinan distribusi resmi harus menyertakan `LICENSE` dan `NOTICE`.

## Alasan

Apache-2.0 cocok untuk library sistem, memberi perlindungan paten yang
eksplisit, dan tetap mengizinkan penggunaan komersial serta modifikasi. Project
tidak menambahkan dependensi berlisensi copyleft ke core tanpa keputusan
terpisah dan catatan provenance yang jelas.

## Kewajiban bila menambah dependensi

Bila kelak backend emulator, VM, atau kode luar lain ditambahkan: catat setiap
dependensi, lisensinya, asal-usulnya, patch lokal, dan cara memperoleh sumber
yang sesuai. Jangan mengklaim clean-room bila implementasi berlisensi tidak
kompatibel pernah dibaca atau disalin. Backend semacam itu harus berada di balik
opsi CMake dan tidak boleh mengubah lisensi core tanpa keputusan eksplisit.
