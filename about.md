# Rendering Utils

Rekam gameplay Geometry Dash menjadi file video, langsung dari dalam game.

## Cara pakai
1. Masuk ke level, buka menu **Pause**.
2. Tekan tombol **RU** di kiri, lalu **Render** untuk membuka **Render Studio**.
3. Atur resolusi, FPS, dan kualitas di tab **Opsi**.
4. Tekan **MULAI RENDER**. Level dimulai ulang dan perekaman berjalan.
5. Selesai level, video tersimpan otomatis. Atau pause lalu **STOP & SIMPAN**.

## Fitur
- Waktu game dikunci per frame, jadi video mulus walau HP lambat
- Resolusi 360p sampai 1080p, FPS 24 / 30 / 60
- Pengaturan kualitas video
- Berhenti otomatis setelah level selesai
- Opsi tampilkan hitbox di video
- Indikator REC (tidak ikut terekam)
- Daftar hasil render, ukuran file, dan hapus

## Catatan
- Musik level ikut direkam (diproses beberapa detik setelah selesai). Efek suara game tidak ikut.
- Kalau HP lag, game melambat otomatis supaya video tetap mulus. Ada indikator kecepatan (1.0x = normal).
- Format MP4 (H.264) supaya bisa diputar di galeri HP. Kalau HP tidak mendukung, otomatis pakai AVI (buka dengan VLC). Tersimpan di folder `renders` di data mod.
