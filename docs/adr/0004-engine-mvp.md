# ADR 0004: Engine MVP rootless (ptrace + seccomp)

Status: diterima (v0.1.0)

## Keputusan

Engine MVP adalah **rootless**: tracer `ptrace` terhadap child, dengan `seccomp`
sebagai **akselerator opsional** (bukan asumsi, harus lolos probe). Engine lain
(`rooted`, `emulator`, `vm`) berupa descriptor `unavailable` yang selalu
melaporkan `backend-required` dan tidak pernah dilaporkan sebagai support.

## Interface

`Engine` (probe + create_instance) dan `EngineInstance`
(start/wait/signal/resize/cancel/pty_master/write_input). Ownership
engine/session eksplisit; **tanpa** global registry mutable. Pemilihan:

- `--engine auto` memilih rootless bila probenya lolos, dan **menjelaskan alasan**
  lewat event `engine.selected`. `auto` tidak pernah fallback ke engine dengan
  semantik keamanan berbeda tanpa error.
- Engine yang diminta eksplisit tetapi tidak tersedia menghasilkan error
  (`T-UNIT-ENGINE-SELECT`); tidak ada fallback diam-diam.

## Alasan

Rootless adalah satu-satunya jalur yang layak tanpa root perangkat di profil
terminal (aplikasi terminal Android atau `adb shell`) dan di host Linux CI.
Rooted/VM/emulator memerlukan capability/dependency yang harus diprobe dan
diaudit; menyatukannya ke "support" tunggal akan menipu.

## Seccomp

Filter meng-`ALLOW` hanya pass-through; sisanya `SECCOMP_RET_TRACE` (dengan
marker `kSeccompTraceMarker` agar dibedakan dari filter guest). Fallback ke
`PTRACE_SYSCALL` penuh bila probe gagal. Semantik keamanan tidak berubah.

Pengukuran dengan `vhdp_bench` (2 iterasi, build Debug, host x86_64 Linux 6.17):
workload pass-through `bench-getpid` turun dari 7,1 sampai 8,2 detik (ptrace
saja) menjadi sekitar 2,1 detik dengan seccomp, kira-kira 3,5 sampai 4 kali
lebih cepat. Workload `bench-stat`, yang syscall utamanya selalu diterjemahkan
dan tetap berhenti di supervisor, percepatannya lebih kecil: dari sekitar 10,1
detik menjadi 5,6 sampai 8,6 detik. Angka ini bergantung pada host dan jumlah
iterasinya kecil; jalankan `vhdp_bench <rootfs> [iterasi]` di perangkat sendiri
untuk perbandingan.
