# Menyiapkan Rootfs

VHDP menjalankan direktori rootfs Linux yang arsitekturnya sama dengan host:
guest aarch64 di host aarch64, guest x86_64 di host x86_64. Rootfs untuk
arsitektur lain butuh backend emulator yang belum ada.

## Syarat rootfs

- Sebuah direktori, bukan `/` host.
- Berisi `/bin/sh` (atau `/bin/bash`, `/bin/ash`) untuk shell default, plus
  dynamic loader dan pustaka bersama bila programnya dinamis.
- Berada di filesystem yang boleh mengeksekusi berkas (tidak `noexec`) dan
  menyimpan mode serta symlink POSIX. Di Android, penyimpanan internal aplikasi
  atau `/data/local/tmp` biasanya cocok; `/sdcard` tidak, karena tidak menjamin
  bit executable, ownership, maupun symlink.

Periksa sebelum menjalankan:

```sh
phdp inspect ./rootfs          # ringkasan dan temuan
phdp inspect --json ./rootfs   # untuk otomasi
```

`inspect` melaporkan jenis filesystem, `noexec`, isi `os-release`, shell yang
ada, kecocokan ABI ELF, dan keberadaan dynamic loader. Status `error` berarti
rootfs tidak akan berjalan, misalnya karena ABI tidak cocok atau loader hilang.

## Memperoleh rootfs

Pakai rootfs yang Anda miliki atau boleh Anda pakai, misalnya arsip rootfs
resmi dari distro (Debian, Ubuntu, Alpine) untuk arsitektur yang benar. VHDP
tidak mengunduh apa pun. Ekstrak sebagai pengguna biasa:

```sh
mkdir rootfs
tar -xpf rootfs-arm64.tar.gz -C rootfs
phdp inspect ./rootfs
```

## Konfigurasi boot pertama

Program yang menyematkan `libvhdp` bisa memanggil `vhdp_configure_rootfs` sekali
setelah ekstraksi. Fungsi ini membuat mountpoint, entri `/dev` yang tidak
disediakan proyeksi (`/dev/fd`, `/dev/stdin`, `/dev/stdout`, `/dev/stderr`,
`/dev/shm`, `/dev/pts`), `resolv.conf`, `nsswitch.conf`, `hosts`, profil login
root, serta pengguna biasa `dracos` (uid/gid 1000) dengan sudo tanpa password
bila `/etc/sudoers.d` ada. Berkas yang sudah ada tidak ditimpa, kecuali
`/etc/resolv.conf` yang sering berupa symlink ke target yang tidak ada.

Bila instalasi paket pernah terputus, `vhdp_dpkg_plan_json` mengembalikan script
pemulihan dpkg yang bisa dijalankan di sesi dengan `--uid 0 --gid 0`.

## Impor yang aman

Bila Anda menulis importer arsip sendiri, ekstrak melalui dirfd root dan tolak:
path absolut, `..`, symlink atau hardlink yang keluar dari root, device khusus,
ukuran sparse yang tidak wajar, dan decompression bomb. Kebijakannya sama
dengan resolver VHDP: gagal secara tertutup.

## Bind direktori host

```sh
phdp run --bind /path/host:/mnt/shared:ro ./rootfs -- /bin/sh
phdp run --bind /path/host:/data:rw ./rootfs -- /bin/sh
```

Bind read-only secara default; `:rw` harus diminta. Path sumber
dikanonikalisasi; target guest harus absolut, tidak boleh mengandung `..`, dan
tidak boleh menimpa `/`.

## Batasan

- UID/GID 0 di guest (`--uid 0`) hanya emulasi metadata, bukan root host.
- Ownership berkas yang diubah guest tidak selalu tersimpan bila filesystem host
  tidak bisa merepresentasikannya; `chown` untuk identitas root palsu tidak
  mengubah apa pun.
- `/proc`, `/sys`, dan `/dev` penuh tidak tersedia pada engine rootless. Gunakan
  `--dev minimal` (default) dan `--proc host` seperlunya; batasannya ada di
  SECURITY.md.
