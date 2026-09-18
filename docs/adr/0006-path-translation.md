# ADR 0006: Path translation (VFS resolver)

Status: diterima (v0.1.0)

## Keputusan

Path guest diterjemahkan ke host oleh `MountTable` (longest-prefix: rootfs + bind +
proyeksi `vdr`) dan `resolver.cpp` yang menyelesaikan path **komponen demi
komponen di ruang guest**:

- Tiap prefix dipetakan ke host lalu di-`lstat`.
- Symlink diinterpretasikan relatif terhadap **root guest** (absolut maupun
  relatif); loop dibatasi `MAXSYMLINKS` dan menghasilkan `ELOOP`.
- `..` tidak pernah naik di atas `/` guest.
- Magic link `/proc` (mode `--proc host`): `self`/`thread-self` diremap ke proses
  guest pemanggil; target yang tidak dapat dipetakan ke dalam mount **fail closed**
  (`EACCES`).
- Keluarga `*at` memakai dirfd guest; fd yang menunjuk ke luar mount ditolak.

## Prioritas primitif

Gunakan primitif berbasis fd (`openat`, `O_NOFOLLOW`, komponen berjangkar) dan,
bila tersedia, `openat2` di masa depan. Untuk kernel lama, component-walk berjangkar
root dirfd. Argumen path yang ditulis ke tracee ditaruh di bawah stack pointer dan
dikembalikan saat syscall-exit.

## Batasan yang dinyatakan

Resolver ini adalah **compatibility boundary, bukan sandbox**. Sisa risiko
rename/TOCTOU antara resolusi VHDP dan lookup kernel didokumentasikan (lihat
SECURITY.md); memperkecilnya memerlukan resolusi berbasis fd yang lebih ketat dan,
untuk workload hostile, VM. Tidak diklaim sebagai sandbox sebelum audit eksternal.

## Test

`T-UNIT-RESOLVER*`, `T-FS-DOTDOT`, `T-FS-SYMLINK-ESCAPE`, `T-FS-DIRFD`,
`T-FS-MAGIC-LINK`, `T-FS-INHERITED-FD`, `T-UNIT-MOUNT-TABLE`, `T-UNIT-GUESTPATH*`.
