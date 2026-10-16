# God of War III Remastered for PC 

<div align="center">
  <a href="https://ko-fi.com/S7O428PBCC" target="_blank"><img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="ko-fi"></a>
</div>

[English](README.md) | [Português (Brasil)](README.pt-BR.md) | **Bahasa Indonesia**

Port PC dari **God of War III Remastered** (PlayStation 4) untuk Windows 10 dan 11, dengan versi
eksperimental untuk Linux dan untuk Mac dengan Apple Silicon ([macOS](#macos-apple-silicon-eksperimental)).

Sisi CPU-nya native: file eksekusi asli game berjalan langsung di prosesor PC (PS4 memakai
arsitektur x86-64 yang sama), dan runtime yang ditulis khusus untuk game ini menggantikan library
sistem PS4, jadi tidak ada emulator. Grafiknya diterjemahkan: berjalan di video core
[shadPS4](https://github.com/shadps4-emu/shadPS4), yang mengubah perintah GPU dan shader PS4 ke
Vulkan, dengan perbaikan rendering God of War III dari
[branch `eltutz` milik cuesta4](https://github.com/cuesta4/shadPS4/tree/eltutz). Menjalankan kode
game langsung di CPU juga cara kerja shadPS4; yang diubah proyek ini adalah lapisan sistem dan
pekerjaan khusus game ini (patch, cache shader di latar belakang, launcher, dan menu di dalam
game). Di Mac, Rosetta 2 dari Apple menerjemahkan kode x86-64 itu untuk Apple Silicon, dan
grafiknya sampai ke Metal lewat driver Vulkan KosmicKrisp dari Mesa.
Proyek ini adalah fork dari [port Windows milik Supermedo](https://github.com/Supermedo/bloodborne_pc)
atas [bloodborne_pc milik deadinside28](https://github.com/deadinside28/bloodborne_pc) (lihat
[Kredit](#kredit)).

> **Tidak ada file game yang disertakan.** Anda perlu dump dari PS4 Anda sendiri (sudah diuji:
> CUSA01623 dan CUSA01715, versi 01.00 dan 01.02).
> Proyek ini tidak berafiliasi dengan Sony Interactive Entertainment maupun Santa Monica Studio.
> God of War adalah merek dagang Sony Interactive Entertainment.

Di mesin uji (AMD Radeon RX 6700 XT, Ryzen 5 5600X, 32 GB) game berjalan rata-rata 144 FPS
(55 sampai 240) di 1080p dengan frame rate engine 240 FPS, dan rata-rata 108 FPS di 4K dengan
FSR 1.

**Sudah diuji dan bisa dimainkan dari awal sampai tamat** di 1080p native dengan frame rate engine
240 FPS. Di render resolution lain sebagian tekstur tampak rusak; perbaikannya sedang dikerjakan.

Perkembangan proyek dicatat (dalam bahasa Portugis) di [docs/gow3/ROADMAP.md](docs/gow3/ROADMAP.md).

## Yang sudah berjalan

- Pembukaan, menu, cutscene video (H.264), dan gameplay.
- Audio: musik, suara, dan efek (ATRAC9 dan MP3).
- Controller (DualSense, Xbox, dan lainnya, lewat SDL3) dan keyboard sekaligus, dua-duanya bisa
  diatur ulang tombolnya.
- Simpan dan muat: daftar save tampil di layar, seperti di PS4. Autosave juga berjalan. Launcher
  membuat cadangan save sebelum setiap permainan dimulai.
- Patch: render resolution 480p, 720p, 1440p, 1800p, atau 4K, perbaikan tekstur rusak, dan lewati
  video dengan tombol X (komunitas); frame rate engine 120 FPS (kvicken) atau 240 FPS (diadaptasi
  oleh Felipe Prado).
- **Langsung mulai:** cache shader yang tersimpan dimuat di latar belakang selagi Anda bermain,
  dan shader baru juga dikompilasi di latar belakang, jadi game tidak berhenti untuk membuatnya.
- **Launcher** dengan warna khas game, berbahasa Inggris dengan sebagian terjemahan: pengaturan
  cepat di halaman awal, tampilan, performa, folder game dan cadangan save, patch, mod (urutan,
  preset, dan bentrokan), kontrol dengan ikon tombol PlayStation atau Xbox, pembaruan, dan diagnosis.
- **Menu di dalam game** (Insert di keyboard, ` di Mac, R3+L2 di controller), dengan empat tab:
  - **Game:** cheat (darah, magic, item meter, dan Rage of Sparta tak terbatas, red orb maksimal);
    pengali 0,1x sampai 100x untuk red orb, green orb (darah), blue orb (magic), dan gold orb
    (Rage of Sparta), serta untuk damage yang diberikan dan diterima; bar darah musuh.
  - **Image:** mode tampilan (windowed, borderless, fullscreen), VSync, upscaling FSR 1 (atau
    MetalFX dari Apple di Mac), dan penajaman RCAS.
  - **Performance:** batas frame rate (30 sampai tanpa batas), jumlah frame dalam antrean,
    kompilasi shader di latar belakang; render resolution, frame rate engine, dan opsi pembacaan
    data GPU diterapkan dengan satu tombol "Apply and restart".
  - **Overlay:** overlay performa berisi FPS, frametime, upscaler, GPU (nama dan pemakaian), CPU,
    RAM, dan VRAM, masing-masing opsional, dengan ukuran huruf, pojok layar, transparansi latar,
    dan tata letak yang bisa diatur. Angkanya adalah pemakaian game itu sendiri, bukan seluruh
    sistem (di Mac, pemakaian GPU-nya adalah pemakaian seluruh GPU: macOS tidak punya penghitung
    per proses). Item upscaler-nya menyebut apa yang memperbesar gambar, misalnya
    "MetalFX 720p->1080p".

## Unduh dan main

**Yang Anda perlukan**
- Windows 10 (1903 atau lebih baru) atau Windows 11, 64-bit.
- Kartu grafis dengan Vulkan 1.3 dan driver terbaru.
- Salinan God of War III Remastered milik Anda sendiri, berupa folder yang berisi `eboot.bin`,
  `sce_module`, dan `sce_sys`. Sudah diuji: **CUSA01623 (AS) dan CUSA01715 (Eropa), versi 01.00
  dan 01.02** (update lengkap). Region dan versi lain mungkin jalan tetapi belum diuji: jika Anda
  mencobanya, silakan buka issue dengan menyertakan `user/last_run.log`. Unduhan ini tidak berisi
  file game.

**Langkah-langkah**
1. Unduh `gow3-windows.zip` dari
   [halaman Releases](https://github.com/FelipePrado0/gow3_pc/releases/latest).
2. Ekstrak ke folder pilihan Anda, misalnya `C:\Games\gow3-windows`. Jangan menjalankannya dari
   dalam zip.
3. Buka **`God of War III.exe`**. Jika Windows menampilkan "Windows protected your PC", klik
   **More info**, lalu **Run anyway**: programnya tidak bertanda tangan digital.
4. Klik **Choose the game folder**. Anda akan dibawa ke halaman **Game**. Di sebelah **Game
   folder**, klik **Browse…** dan pilih folder yang berisi `eboot.bin`.
5. Kembali di **Home**, gambar sampul dan "CUSA01623 · version 01.02" akan muncul. Ubah **Quick
   settings** jika perlu: mode tampilan, batas frame rate, render resolution, dan frame rate
   engine.
6. Klik **PLAY**. Permainan pertama sedikit lebih lama karena image game sedang disiapkan.
   Berikutnya akan cepat.

Patch dan perbaikan sudah disertakan dan diterapkan otomatis: perbaikan tekstur, lewati video
dengan X, dan 120 FPS aktif secara bawaan. Ubah resolusi dan frame rate di **Home** atau
**Performance**.

**Saat bermain**
- **Insert** di keyboard, atau **R3 + L2** di controller, membuka menu port: cheat, pengali,
  gambar, performa, dan overlay performa.
- Shader yang tersimpan dimuat di latar belakang. Catatan kecil "Loading shader cache" di pojok
  layar menunjukkan kemajuannya.

**Berikutnya**
- **`Play God of War III.exe`** langsung memulai game dengan pengaturan yang tersimpan. Anda bisa
  membuat shortcut desktop ke file itu, atau menambahkannya ke Steam lewat **Add a Non-Steam Game**.
- Save, cache shader, dan log Anda ada di folder `user` di sebelah `God of War III.exe`. Simpan
  folder itu saat memperbarui.
- **Advanced → Check for updates** mengunduh dan memasang versi baru. Save dan pengaturan Anda
  tetap aman.

**Jika ada masalah**
- Layar hitam saat mulai: **Advanced → Clear shader cache**, lalu mulai lagi.
- Game tidak mau terbuka: periksa halaman **Log**, atau kirim `user\last_run.log` bersama laporan
  Anda.

### Di Mac (Apple Silicon, eksperimental)

**Yang Anda perlukan**
- Mac dengan Apple Silicon (M1 atau lebih baru) dan macOS 26 (Tahoe) atau lebih baru. Mac Intel
  tidak didukung.
- Rosetta 2: kode game itu sendiri berarsitektur x86-64. Jika belum terpasang, halaman **Log**
  akan memberi tahu; pasang di Terminal dengan `softwareupdate --install-rosetta --agree-to-license`.
- Salinan game milik Anda sendiri, seperti di atas. Tidak perlu yang lain: Python, library, dan
  driver Vulkan sudah ada di dalam aplikasi.

**Langkah-langkah**
1. Unduh `gow3-macos.dmg` dari
   [halaman rilis macOS](https://github.com/nizwar/gow3_macos/releases/latest).
2. Buka file itu dan seret **God of War III** ke **Applications**.
3. Aplikasinya tidak ditandatangani dengan Apple Developer ID, jadi macOS memblokirnya. Izinkan
   sekali di Terminal:
   ```
   xattr -dr com.apple.quarantine "/Applications/God of War III.app"
   ```
4. Buka **God of War III** dan klik **Choose the game folder**. Di sebelah **Game folder**, klik
   **Browse…** dan pilih folder yang berisi `eboot.bin`. Update yang di-dump terpisah dari game
   dimasukkan di **Update folder**.
5. Klik **PLAY**. Permainan pertama lebih lama: Rosetta menerjemahkan kode game dan shader-nya
   dikompilasi.

Untuk FPS lebih tinggi di layar 1080p: **Performance → Render resolution → 720p** (atau 480p) dan
**Display → Upscale to the window** dengan **Upscaler → MetalFX (Apple)**. Setiap render
resolution yang lebih kecil dari jendela akan diperbesar, dan overlay performa lalu menampilkan
"MetalFX 720p->1080p". Di M4 Pro hasilnya sekitar 52 FPS di pertarungan pertama, dibandingkan
32 FPS di 1080p native. Di 480p area terang menjadi terlalu silau (bug game yang sudah diketahui
di resolusi itu).

**Saat bermain**
- **`** (tombol di kiri angka 1) di keyboard, atau **R3 + L2** di controller, membuka menu port:
  keyboard Mac tidak punya tombol Insert.

**Berikutnya**
- Buka **God of War III** dari Applications, Launchpad, atau Dock. **Advanced → Add to Steam**
  menambahkannya ke Steam (tutup Steam dulu); setelah itu Steam memulai game tanpa launcher.
- **Advanced → Check for updates** mencari `gow3-macos.dmg` yang lebih baru dan mengunduhnya:
  seret aplikasi baru menimpa yang lama. Save dan pengaturan tetap aman.
- Save, pengaturan, cache shader, dan log ada di `~/Library/Application Support/gow3`
  (**Advanced → Port folder** membukanya); pengaturan launcher sendiri ada di
  `~/.config/gow3-launcher`.

**Jika ada masalah**
- "God of War III is damaged" atau "cannot be opened": jalankan lagi baris `xattr` di langkah 3.
- Layar hitam saat mulai: **Advanced → Clear shader cache**, lalu mulai lagi.
- Game tidak mau terbuka: periksa halaman **Log**, atau kirim
  `~/Library/Application Support/gow3/user/last_run.log` bersama laporan Anda.

## Linux (AppImage, eksperimental)

Versi Linux menjalankan runtime native dan launcher yang sama. Versi ini diuji di Arch Linux
dengan Radeon RX 7800 XT (RADV) di sesi Wayland.

**Yang Anda perlukan:** driver Vulkan 1.3 dan dump game milik Anda sendiri (CUSA01623, versi 01.02).

**Build dan jalankan**
```
bash build.sh
bash packaging/linux/build.sh   # perlu PyInstaller, Tk, dan Pillow untuk Python, plus patchelf dan curl
./dist/gow3-x86_64.AppImage
```
Pilih folder game di launcher. `--play` memulai game dengan opsi tersimpan tanpa jendela. Dari
source tree: `GOW3_GAME_DIR=/path/ke/CUSA01623 python3 run.py` setelah `bash build.sh`.

- **File:** save, pengaturan, cache shader, dan log ada di `~/.local/share/gow3`; pengaturan
  launcher sendiri di `~/.config/gow3-launcher`.
- **Update folder:** jika update di-dump terpisah dari game, pilih di **Game → Update folder**.
  Folder itu ditumpuk di atas folder game tanpa mengubah keduanya.
- **Advanced → Linux:** sistem jendela (otomatis, Wayland, atau X11), GameMode, dan MangoHud. Hanya
  alat yang terpasang di sistem yang bisa dinyalakan.
- **Steam:** **Advanced → Add to Steam** menambahkan "God of War III Remastered" sebagai game
  non-Steam, lengkap dengan ikon, capsule vertikal dan horizontal, latar library, dan logo yang
  dibuat dari gambar di dump. Tutup Steam dulu (Steam menulis ulang shortcut-nya saat ditutup).
  Tombol ini juga ada di launcher Windows dan Mac (di sana belum diuji dengan Steam).
- **Entri menu:** AppImage menulis `~/.local/share/applications/gow3.desktop` saat dijalankan,
  supaya launcher dan jendela game menampilkan ikonnya di dock; **Advanced → Linux** mematikannya
  dan menghapus entri itu.
- **Ikon:** AppImage, launcher, dan jendela game memakai `packaging/linux/gow3.png` jika file itu
  ada saat build, dan `launcher/gow3.png` jika tidak.
- **Patch:** file patch yang sama melayani update 01.02 lengkap (`eboot.bin` sha256 `6e72330e…`)
  dan judul Eropa (CUSA01715): patch dan cheat menemukan lokasinya lewat signature.
- **Graphics (experimental):** upscaler temporal (FSR 3.1, FSR 4, TAA, DLSS) mati secara bawaan;
  sakelarnya ada di bagian atas halaman itu.

## macOS (Apple Silicon, eksperimental)

Versi macOS menjalankan runtime native yang sama di bawah Rosetta 2, dengan grafik di driver
Vulkan KosmicKrisp dari Mesa (Vulkan di atas Metal). Versi ini butuh Mac dengan Apple Silicon dan
macOS 26 (Tahoe) atau lebih baru. Sudah diuji di M4 Pro (macOS 27) pada pembukaan dan
pertarungan-pertarungan awal, tetapi belum dimainkan sampai tamat. Untuk bermain, pakai disk
image ([Di Mac](#di-mac-apple-silicon-eksperimental)); untuk build dan menjalankan dari repository
(detail dan perbedaannya dengan Linux: [packaging/macos/README.md](packaging/macos/README.md)):

**Yang Anda perlukan:** Xcode command line tools (`xcode-select --install`),
`brew install cmake ninja pkg-config` dari Homebrew, Rosetta 2, dan Python 3 dengan Tk dan Pillow
untuk launcher (`brew install python python-tk`, lalu `python3 -m pip install pillow`).

```
git clone --recursive https://github.com/nizwar/gow3_macos && cd gow3_macos
git checkout macos-port            # pekerjaan macOS
./play-mac.sh                      # build, mencari dump di ~/Downloads, ~/Desktop, atau ~/Documents, lalu main
./play-mac.sh /path/ke/CUSA01623   # folder itu (eboot.bin, sce_module, sce_sys)
./play-mac.sh --launcher           # launcher: pengaturan, save, mod
bash packaging/macos/package.sh    # dist/gow3-macos.dmg dan dist/God of War III.app: untuk rilis
```

- **File:** dari repository, save, `gow3.ini`, cache shader, dan `user/last_run.log` ada di folder
  repository, seperti di Windows; aplikasi menyimpannya di `~/Library/Application Support/gow3`.
  Pengaturan launcher sendiri: `~/.config/gow3-launcher`.
- **Tanpa jendela launcher:** `./play-mac.sh` (di atas), atau
  `"/Applications/God of War III.app/Contents/MacOS/God of War III" --play` dengan pengaturan
  tersimpan.
- **Upscaling:** **Display → Upscale to the window** dengan **Upscaler → MetalFX (Apple)** atau
  FSR 1. Halaman **Graphics (experimental)** tidak ditampilkan di Mac: upscaler temporal belum
  mengubah apa pun di game ini, dan FSR 3.1 tidak bisa jalan di KosmicKrisp.
- **Display → Fewer GPU waits (experimental):** beban CPU lebih ringan saat pembacaan data GPU
  menahan proses menggambar; di M4 Pro FPS-nya tidak naik, karena yang membatasi adalah sisi GPU.
- **Advanced → macOS → Metal performance HUD:** overlay dari Apple berisi frame rate, frame time,
  dan memori di jendela game, padanan MangoHud di Linux.
- **Overlay performa:** FPS, frametime, upscaler, pemakaian GPU (seluruh GPU), CPU, RAM (angka
  yang sama dengan Activity Monitor), dan VRAM, seperti di Windows.
- **Performance → GPU data** tidak punya pilihan kartu grafis maupun memori stream buffer: Mac
  punya satu GPU yang berbagi memori dengan sistem.
- **Steam:** **Advanced → Add to Steam**, seperti di Linux (`~/Library/Application Support/Steam`),
  dengan Steam dalam keadaan tertutup.
- **Pembaruan:** **Advanced → Check for updates** memeriksa rilis di repository ini (tag
  `macos-v<versi>` dengan `gow3-macos.dmg`) dan mengunduh disk image yang lebih baru.
- **Shortcut desktop:** dari repository, **Advanced → Desktop shortcut** menaruh aplikasi di
  desktop yang membuka launcher; aplikasi hasil package adalah shortcut-nya sendiri (Dock,
  Launchpad).

## Persyaratan

- Windows 10 (1903 atau lebih baru) atau Windows 11, 64-bit; atau Linux dengan driver Vulkan 1.3;
  atau Mac dengan Apple Silicon (M1 atau lebih baru), macOS 26 (Tahoe) atau lebih baru, dan
  Rosetta 2.
- Prosesor 64-bit dengan instruction set PS4 (AVX, F16C, BMI1, MOVBE, LZCNT): Intel Haswell (2013)
  atau lebih baru, AMD Ryzen atau lebih baru. Kode game berjalan di CPU apa adanya, jadi prosesor
  yang lebih lama berhenti saat mulai dengan baris `STOP: this CPU lacks` di log. Di Mac, Rosetta 2
  yang menyediakan instruksi-instruksi itu.
- Kartu grafis dengan Vulkan 1.3 dan driver terbaru. Di Mac, driver Vulkan-nya (KosmicKrisp dari
  Mesa, di atas Metal) sudah disertakan dalam port.
- Salinan game Anda (folder berisi `eboot.bin`, `sce_module`, dan `sce_sys`). Sudah diuji:
  CUSA01623 dan CUSA01715, versi 01.00 dan 01.02.
- Patch dan hook di dalam game (cheat, pengali, bar darah) menemukan lokasinya lewat signature:
  byte asli di sekitar masing-masing, di mana pun sebuah rilis menaruh kode itu
  (`tools/make_signatures.py`). Lokasi yang tidak ditemukan hanya mematikan patch atau hook itu;
  game tetap jalan tanpanya, dan log serta menu menyebutkan alasannya.
- Untuk build dari source: [MSYS2](https://www.msys2.org) dan [Python 3](https://www.python.org)
  dengan Pillow (`pip install pillow`, dipakai untuk ikon dan gambar sampul game). Di Mac: Xcode
  command line tools, `cmake`, `ninja`, dan `pkg-config` dari Homebrew, serta Python 3 dengan Tk dan
  Pillow ([macOS](#macos-apple-silicon-eksperimental)).

## Build dari source

Pengembangan dilakukan di branch `gow3`; `main` menerima penggabungan yang sudah diuji.

1. Pasang MSYS2 (`winget install MSYS2.MSYS2`) dan, di shell **MSYS2 CLANG64**, paket-paket yang
   tercantum di [packaging/windows/README.md](packaging/windows/README.md).
2. Clone repository beserta submodule-nya, lalu build:

   ```bash
   git clone --recursive https://github.com/FelipePrado0/gow3_pc
   cd gow3_pc
   git checkout gow3   # pekerjaan terbaru; lewati untuk build main
   bash build.sh
   ```

3. Jalankan launcher (di PowerShell, di folder proyek):

   ```powershell
   python launcher/gow3_launcher_win.py
   ```

4. Di **Game**, pilih folder game. Ubah **Display**, **Performance**, dan **Patches** jika perlu,
   lalu tekan **PLAY**.

Untuk bermain dengan pengaturan tersimpan tanpa membuka jendela launcher (untuk shortcut atau
Steam):

```powershell
python launcher/gow3_launcher_win.py --play
```

Atau tanpa launcher sama sekali:

```powershell
$env:GOW3_GAME_DIR = 'D:\path\CUSA01623'
python run.py
```

Pengaturan disimpan di `gow3.ini` (game dan menu di dalam game) dan di
`%APPDATA%\gow3-launcher\settings.json` (launcher). Save, cache shader, dan `last_run.log` masuk
ke folder `user` milik proyek.

Di Mac, build-nya adalah `bash build.sh` di Terminal (pertama kali ia mengunduh dan membangun
library x86-64, sekitar 5 menit), lalu `./play-mac.sh --launcher` atau
`python3 launcher/gow3_launcher_win.py` (`--play` tanpa jendela), atau
`GOW3_GAME_DIR=/path/ke/CUSA01623 python3 run.py` tanpa launcher; lihat
[macOS](#macos-apple-silicon-eksperimental). Pengaturan launcher ada di
`~/.config/gow3-launcher/settings.json`; save dan log di folder `user` milik proyek, sama seperti
di sini.

Di **Performance → GPU data → Stream buffer memory** (Windows dan Linux), pilih **System RAM
(cached, default)**, **System RAM (uncached)**, atau **GPU memory (previous behavior)**. RAM sistem
bisa menaikkan frame rate di laptop NVIDIA. Pilihan ini disimpan dan berlaku saat game berikutnya
dimulai, termasuk lewat `Play God of War III.exe` atau `--play`. Pilih GPU memory untuk kembali ke
cara alokasi sebelumnya. Untuk menjalankan langsung tanpa launcher, `GOW3_STREAM_MEMORY` menerima
`cached`, `host`, atau `device`; pilihan tersimpan di launcher lebih diutamakan saat launcher
dipakai.

### Mengimpor save shadPS4

Hanya **save shadPS4 untuk God of War III Remastered** yang kompatibel. Save RPCS3 berasal dari
game PS3 dan tidak akan bisa dipakai. Gunakan title ID yang sama (contoh ini memakai `CUSA01623`).

1. Tutup game dan shadPS4. Cadangkan kedua folder save.
2. Cari sumbernya di folder data shadPS4: `user\savedata\1\CUSA01623\` di versi lama, atau
   `home\<user-id>\savedata\CUSA01623\` di versi baru. Di Mac, folder data shadPS4 biasanya
   `~/Library/Application Support/shadPS4`.
3. Tujuannya adalah **`user\savedata\1\CUSA01623\` di sebelah `God of War III.exe`**.
   Contoh: `C:\Games\gow3-windows\user\savedata\1\CUSA01623\`. Dengan **Saves folder** khusus di
   launcher, pakai `<folder khusus>\savedata\1\CUSA01623\`. Di Mac (aplikasi):
   `~/Library/Application Support/gow3/user/savedata/1/CUSA01623/`; dari build source,
   `user/savedata/1/CUSA01623/` di folder proyek.
4. Salin setiap folder slot ke sana, dengan nama dan file game-nya tetap. Jangan membuat folder
   `CUSA01623` lagi di dalamnya. Jangan ikutkan folder `sce_sys` internal slot ke data game.
5. Di samping setiap slot, buat `<nama slot>.sce_sys` jika belum ada. Anda bisa menyalin
   `sce_sys\icon0.png` dari shadPS4 ke sana. **Jangan mengganti nama `param.sfo` menjadi
   `param.bin`**: formatnya berbeda. Pertahankan `param.bin` milik port yang sudah ada. Tanpa
   file itu, slot tampil dengan nama foldernya; menyimpan lagi di port akan menulis metadatanya
   dan memperbarui informasi yang tampil.
6. Jalankan port dan pilih **Load Game**. Save yang sudah tamat juga bisa diimpor.

Dialog save di port menampilkan paling banyak **32 entri**. Jika slot yang diimpor tidak muncul
di daftar yang panjang, uji dengan **Saves folder** terpisah yang hanya berisi slot itu di bawah
`savedata\1\CUSA01623\`, beserta folder metadata di sampingnya. Simpan save aslinya dan kembalikan
pengaturan Saves folder sebelumnya setelah pengujian.

### Overlay pihak ketiga (Windows)

Proses game-nya adalah **`bin\gow3-probe.exe`**, bukan `God of War III.exe` (build dari source
memakai `out\gow3-probe.exe`). Tambahkan file itu ke RTSS dan aktifkan on-screen display
Afterburner. Di installer ReShade, pilih file yang sama dan API **Vulkan**.

Jika overlay tetap tidak muncul, laporkan versi port dan overlay, versi port terakhir yang masih
berfungsi, dan lampirkan `user\last_run.log`. Pemeriksaan ini tidak menjamin perbaikan untuk
semua konfigurasi overlay.

### Overlay di Mac

RTSS, Afterburner, dan ReShade tidak tersedia untuk macOS. Metal performance HUD dari Apple
menggantikannya: **Advanced → macOS → Metal performance HUD** di launcher, atau
`MTL_HUD_ENABLED=1` jika memulai tanpa launcher. Proses game-nya adalah `gow3-probe` (di dalam
aplikasi: `God of War III.app/Contents/Resources/bin/gow3-probe`), nama yang juga ditampilkan
Activity Monitor. Overlay performa milik port sendiri berjalan seperti di Windows.

### Kontrol keyboard

Bawaan; ubah di halaman **Controls** di launcher.

| Tombol | Tombol PS4 |
|---|---|
| WASD | stick kiri |
| Panah | stick kanan (kamera) |
| Space | Cross |
| Shift kiri | Circle |
| E | Square |
| Q | Triangle |
| 1 dan 3 | L1 dan R1 |
| R dan F | L2 dan R2 |
| Z dan C | L3 dan R3 |
| I, K, J, L | d-pad |
| Enter | Options |
| Tab | klik kiri touchpad (mode foto) |
| Backspace | klik kanan touchpad (menu jeda) |
| Insert (` di Mac) | menu port (R3+L2 di controller) |

Di controller tanpa touchpad (Xbox dan lainnya), View membuka menu jeda (klik kanan touchpad) dan
L1 + View membuka mode foto (klik kiri). Di DualSense touchpad berfungsi seperti di PS4, dan
Share/Create berfungsi seperti View.

Di menu: L1/R1 atau Q/E berpindah tab. Di daftar save: panah atau d-pad untuk memilih, Enter atau
Cross untuk konfirmasi, Esc atau Circle untuk batal. Keyboard Mac tidak punya tombol Insert: di
sana **`** (tombol di kiri angka 1) membuka menu, dan Insert tetap berfungsi di keyboard yang
memilikinya.

## Pengujian

```bash
python -m unittest discover -s tests
bash build.sh --test
ninja -C out/gpu stat-multipliers-test perf-overlay-test warmup-inbox-test graphics-settings-test
out/gpu/stat-multipliers-test.exe out/eboot.elf
out/gpu/perf-overlay-test.exe
out/gpu/warmup-inbox-test.exe
out/gpu/graphics-settings-test.exe
```

Tes C++ adalah target di [gpu/CMakeLists.txt](gpu/CMakeLists.txt); tes yang memeriksa kode game
menerima executable yang sudah disiapkan (`out/eboot.elf`) sebagai argumen. Di Linux dan macOS
programnya tanpa `.exe`; di Mac jalankan `source packaging/macos/env.sh` sebelum `ninja` supaya ia
menemukan compiler dan library x86-64 (`bash build.sh --test` melakukannya sendiri).

Untuk memeriksa bahwa setiap fungsi sistem yang dipakai game sudah diimplementasikan, tanpa
memulai game:

```bash
out/gow3-probe.exe out/boot-linked.bin --app0 <folder game> --check-imports
```

## Cara kerjanya

| Bagian | Lokasi | Fungsinya |
|---|---|---|
| Persiapan | `scripts/` | Membaca `eboot.bin`, menerapkan relokasinya, dan menggabungkan `libc` dan `libSceFios2` milik game menjadi satu image |
| Loader | `src/probe.c` | Memuat image, memasang hook yang sudah divalidasi (cheat, pengali, bar darah), dan memulai kode asli game |
| Runtime | `src/runtime_*.c` | Mengimplementasikan ulang fungsi sistem PS4: memori, thread, file, audio, video, controller, save, dan layanan |
| GPU | `gpu/shadps4/` | Video core shadPS4, yang menerjemahkan perintah GPU dan shader PS4 ke Vulkan, dengan perbaikan God of War III dari cuesta4 (branch `eltutz`) dan optimasi kecepatan dari port ini (cache shader di latar belakang, pembacaan data yang ditunda). Di Mac, Vulkan berjalan di atas Metal lewat KosmicKrisp dari Mesa, dan MetalFX bisa memperbesar gambar (`host_passes/metalfx_pass.cpp`, `vk_metalfx.mm`) |
| Menu di dalam game | `gpu/shim/gow3_overlay.cpp` | Menu Dear ImGui dan overlay performa; pengaturan di `gpu/shim/gow3_settings.cpp` |
| Patch | `patches/God_of_War_III_Remastered.xml`, `scripts/patches.py` | Diterapkan di memori saat mulai, tanpa mengubah file game |
| Launcher | `launcher/gow3_launcher_win.py` | Pengaturan, patch, mod, kontrol, dan tombol main |
| Paket | `packaging/windows/`, `packaging/linux/`, `packaging/macos/` | Zip Windows, AppImage, dan aplikasi Mac beserta disk image-nya (`package.sh`) |

Dokumentasi lain: [mod](docs/MODS.md), [peta frame dan upscaler](docs/upscaler.md),
[pekerjaan GPU paralel](docs/parallel_gpu.md), dan [roadmap](docs/gow3/ROADMAP.md).

## Kredit

Proyek ini adalah fork dari port Windows milik Supermedo, yang dibangun di atas port asli milik
deadinside28:

- **deadinside28**: [bloodborne_pc](https://github.com/deadinside28/bloodborne_pc), port native
  aslinya.
- **Supermedo** (Mohammed Albarghouthi): [bloodborne_pc untuk Windows](https://github.com/Supermedo/bloodborne_pc),
  fork asal proyek ini.
- **cuesta4**: [fork shadPS4 yang berfokus pada God of War III Remastered](https://github.com/cuesta4/shadPS4/tree/eltutz)
  (branch `eltutz`). Rendering game di port ini bergantung pada perbaikannya: penulisan ulang
  blend GNM, state depth/stencil, dan image aliasing. cuesta4 juga salah satu pembuat patch
  resolusi, perbaikan tekstur, 120 FPS, dan lewati video (di bawah). Jika Anda bermain di shadPS4,
  build inilah yang dianjurkan untuk God of War III.
- **Tim shadPS4**: video core [shadPS4](https://github.com/shadps4-emu/shadPS4), yang melakukan
  seluruh penerjemahan GPU dan shader ke Vulkan di port ini.

Patch: kvicken dan cuesta4 (resolusi, perbaikan tekstur, 120 FPS, dan lewati video, berdasarkan
patch PS3 dari illusion0001), dipublikasikan untuk shadPS4 di
[shadps4-emu/ps4_cheats#145](https://github.com/shadps4-emu/ps4_cheats/pull/145) dan diadaptasi
untuk executable ini. Patch 240 FPS diadaptasi oleh Felipe Prado dari patch 120 FPS.
Cheat: diadaptasi dari [cheat GoldHEN](https://github.com/GoldHEN/GoldHEN_Cheat_Repository) milik
Celogamez.

Juga dipakai: [LibAtrac9](https://github.com/Thealexbarney/LibAtrac9), [FFmpeg](https://ffmpeg.org),
[SDL3](https://github.com/libsdl-org/SDL), [Dear ImGui](https://github.com/ocornut/imgui),
[sirit](https://github.com/shadps4-emu/sirit), [magic_enum](https://github.com/Neargye/magic_enum),
[miniz](https://github.com/richgel999/miniz), [xbyak](https://github.com/herumi/xbyak),
[Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator),
[FSR-Vulkan](https://github.com/FireBurn/FSR-Vulkan) dan AMD FidelityFX SDK,
[MSYS2](https://www.msys2.org), [LLVM](https://llvm.org), dan [Pillow](https://python-pillow.org).
Di macOS: driver Vulkan KosmicKrisp dari [Mesa](https://mesa3d.org) (dari rilis shadPS4 untuk
macOS), MetalFX dari Apple, [mimalloc](https://github.com/microsoft/mimalloc), dan
[PyInstaller](https://pyinstaller.org) untuk aplikasinya.

## Lisensi

GNU GPL v2 atau yang lebih baru ([LICENSE](LICENSE)). Komponen pihak ketiga tetap memakai
lisensinya masing-masing.
