# STONE

**Offline fitness, diet & workout tracker — written in C, running on Android NativeActivity.**

Created by **sobing4413** · **Exter Interactive**

---

## 1. Apa itu STONE

STONE adalah aplikasi Android yang seluruh logikanya ditulis dalam bahasa **C**. Ada tepat
**satu** file Java di project ini — `StoneActivity.java`, subclass tipis dari
`android.app.NativeActivity` — dan file itu tidak memegang logika aplikasi sama sekali.
Tugasnya hanya dua, dan keduanya *wajib* dijalankan di UI thread milik Activity, yang mana
`android_main()` bukan:

1. meminta **immersive mode** (menyembunyikan status bar dan navigation bar sistem), dan
2. meneruskan **window insets** yang sebenarnya ke renderer C.

Selebihnya tetap C: `libstone.so` yang dimuat lewat `ANativeActivity_onCreate`.

* **100% offline.** Tidak ada permission `INTERNET`, tidak ada akun, login, API, server,
  cloud, iklan, analytics, maupun telemetry. Aplikasi secara fisik tidak bisa menjangkau
  jaringan.
* **Data milik pengguna.** Semuanya disimpan sebagai JSON biasa di direktori privat aplikasi,
  bisa di-backup dan di-restore sendiri.
* **UI digambar sendiri** dengan OpenGL ES 2.0 — card, chart, ring progress, animasi, dan
  keyboard on-screen semuanya dirender oleh kode C di project ini.

> STONE memberikan **estimasi kebugaran umum**, bukan diagnosis atau nasihat medis.
> Konsultasikan dengan tenaga kesehatan sebelum mengubah program diet atau latihan.

---

## 2. Requirement

| Kebutuhan | Versi yang diuji | Catatan |
|---|---|---|
| Visual Studio 2026 | workload **Mobile development / C++ Android** atau **Desktop C++** + komponen CMake | untuk edit & build native |
| Android SDK Platform | API 34 (`compileSdk 34`) | minimal API 21 untuk device |
| Android NDK | **r26b (26.1.10909125)** | r23+ mana pun seharusnya jalan |
| CMake | 3.22.1+ | ikut bundle di SDK |
| JDK | 17 | dibutuhkan Gradle/AGP 8.5 |
| Gradle | 8.7 (via wrapper) | hanya untuk mengemas APK |

Ukuran build: satu `libstone.so` per ABI (~250–400 KB), satu kelas Java kecil, beberapa
resource, dan `assets/stone_pack.stpk` (32,00 MB — texture pack tak terkompresi, lihat
bagian 9). APK rilis ada di kisaran **32–34 MB**. Tidak ada dependency pihak ketiga sama
sekali.

---

## 3. Struktur direktori

```
STONE/
├─ build.gradle                 # root Gradle script (AGP saja)
├─ settings.gradle
├─ gradle.properties
├─ gradle/wrapper/gradle-wrapper.properties
├─ README.md
├─ BUILD_VS2026.md              # langkah detail Visual Studio 2026
├─ docs/sample/                 # contoh isi setiap file JSON
│  ├─ profile.json  settings.json  foods.json
│  ├─ workouts.json history.json   progress.json
│  └─ stone_backup.json
├─ tools/gen_font.py            # regenerasi atlas font (opsional)
├─ tools/gen_assets.py          # regenerasi assets/stone_pack.stpk (opsional)
└─ app/
   ├─ build.gradle              # applicationId com.stone.app, rename → stone.apk
   ├─ proguard-rules.pro
   └─ src/main/
      ├─ AndroidManifest.xml
      ├─ res/
      │  ├─ values/{strings,colors,themes}.xml
      │  ├─ drawable/ic_launcher_{foreground,background}.xml
      │  ├─ mipmap-anydpi-v26/ic_launcher{,_round}.xml
      │  └─ mipmap-*dpi/ic_launcher{,_round}.png
      └─ cpp/
         ├─ CMakeLists.txt      # build system native
         ├─ main.c              # android_main: EGL, event loop, lifecycle
         ├─ utils/              strbuf.{h,c}
         ├─ storage/            json.{h,c}  storage.{h,c}
         ├─ core/               model.h  calc.{h,c}  app.{h,c}
         ├─ data/               defaults.{h,c}   # 40 makanan, 12 program latihan
         ├─ renderer/           render.{h,c}  font.{h,c}  font_data.c  chart.{h,c}
         └─ ui/                 theme.h  ui.{h,c}  ui_input.c
                                page_dashboard.c  page_diet.c  page_workout.c
                                page_progress.c   page_history.c page_profile.c
                                page_about.c
```

### Peta dependency antar modul

```
main.c
  └── ui/  ──────► renderer/ ──► (EGL, GLESv2)
       │              └── font/chart
       └── core/ ──► data/
              └── storage/ ──► utils/
```

Aturannya satu arah: `storage` tidak tahu apa-apa soal `ui`, `renderer` tidak tahu apa-apa
soal `core`. Hanya `main.c` dan `ui` yang menyentuh keduanya.

---

## 4. Modul

| Modul | Isi |
|---|---|
| `utils/strbuf` | string builder yang tumbuh sendiri + `stone_strlcpy` |
| `storage/json` | parser & writer JSON tanpa dependency; batas kedalaman 32, escape `\uXXXX`, tolak trailing garbage |
| `storage/storage` | baca/tulis file **atomik** (`.tmp` → `fsync` → `rename`), karantina file rusak ke `*.corrupt`, batas 8 MB |
| `core/model.h` | seluruh struct & enum data |
| `core/calc` | BMI, BMR Mifflin–St Jeor, TDEE, target kalori & makro, berat ideal |
| `core/app` | state aplikasi tunggal, serialisasi tiap file, autosave, tanggal, query diet/workout/progress, backup |
| `data/defaults` | 40 makanan Indonesia + 12 program latihan (54 gerakan) |
| `renderer/render` | dua shader GLES2: rounded-box SDF & atlas teks; clipping via scissor |
| `renderer/font` | atlas bitmap Liberation Sans (regular + bold) yang di-embed & didekode saat runtime |
| `renderer/chart` | grafik garis (grid, target line, area fill) dan grafik batang |
| `ui/*` | layer immediate-mode + seluruh halaman |

---

## 5. Halaman aplikasi

* **Dashboard** — berat kini & selisih 7 hari, target, ring progress, BMI, target kalori,
  workout minggu ini, streak, kalori hari ini + bar makro, quick action, tren berat, aktivitas terbaru.
* **Diet** — ringkasan kalori/makro harian, filter waktu makan, pencarian, database 40 makanan
  offline + makanan custom, catat porsi, hapus entri.
* **Workout** — filter level (Beginner/Intermediate/Advanced) dan kategori (Full Body, Upper Body,
  Lower Body, Core, Strength, Cardio); detail program dengan set/repetisi/durasi/istirahat/instruksi;
  **sesi latihan** dengan timer ring, navigasi antar set, dan pencatatan otomatis.
* **Progress** — grafik berat (dengan garis target), kalori 14 hari, workout per minggu, 5 achievement.
* **History** — rekap per tanggal: kalori masuk, kalori terbakar, workout, entri berat.
* **Profile** — nama, umur, jenis kelamin, tinggi, berat, target, level aktivitas, tujuan,
  target workout mingguan, tema, Backup / Restore / Reset, link ke About.
* **About** — credits: *Created by sobing4413* · *Exter Interactive*.

---

## 6. Penyimpanan data

Semua file berada di `internalDataPath` (`/data/data/com.stone.app/files/`):

| File | Isi |
|---|---|
| `profile.json` | data diri & target |
| `settings.json` | tema, satuan, suara, animasi |
| `foods.json` | database makanan (bawaan + custom) |
| `workouts.json` | program latihan |
| `history.json` | `food_log` + `workout_log` |
| `progress.json` | riwayat berat badan |

Backup manual ditulis ke direktori eksternal aplikasi sebagai
`stone_backup_YYYY-MM-DD.json` (satu file berisi semuanya). Contoh lengkap setiap file ada
di `docs/sample/` — file-file itu **dihasilkan langsung oleh serializer aplikasi**, jadi
formatnya persis sama dengan yang ditulis di perangkat.

### Error handling

| Kasus | Perilaku |
|---|---|
| File tidak ada | dataset default dipakai, file dibuat saat save pertama |
| JSON rusak / terpotong | file dipindah ke `<nama>.corrupt`, default dipulihkan, toast muncul |
| Baris data invalid | baris itu dibuang, sisanya tetap dimuat |
| Nilai di luar akal (berat 0, umur 900) | di-clamp ke rentang wajar |
| Storage tidak tersedia | app tetap jalan di memori, badge "storage error" muncul di header |
| App ditutup saat menyimpan | tulis atomik: file lama tetap utuh, tidak pernah setengah jadi |
| Ukuran layar berbeda | skala dp dihitung dari lebar layar (clamp 0.7×–2.6×) |

---

## 7. Build cepat (command line)

```bash
# Debug
./gradlew assembleDebug
# hasil: app/build/outputs/apk/debug/stone.apk

# Release (unsigned kalau properti keystore tidak diberikan)
./gradlew assembleRelease
# hasil: app/build/outputs/apk/release/stone.apk
```

Untuk release yang tertandatangani, buat keystore sekali:

```bash
keytool -genkeypair -v -keystore stone-release.jks -alias stone \
        -keyalg RSA -keysize 2048 -validity 10000
```

lalu tambahkan ke `gradle.properties` (jangan di-commit):

```properties
STONE_STORE_FILE=stone-release.jks
STONE_STORE_PASSWORD=...
STONE_KEY_ALIAS=stone
STONE_KEY_PASSWORD=...
```

### Install ke perangkat

```bash
adb devices
adb install -r app/build/outputs/apk/release/stone.apk
# jika ada versi lama dengan signature berbeda:
adb uninstall com.stone.app && adb install app/build/outputs/apk/release/stone.apk
```

Melihat log native:

```bash
adb logcat -s STONE
```

---

## 8. Build dengan Visual Studio 2026

Lihat **[BUILD_VS2026.md](BUILD_VS2026.md)** untuk langkah lengkapnya (edit + IntelliSense +
build native lewat CMake, lalu pengemasan APK lewat Gradle).

---

## 9. Catatan implementasi

**Keyboard on-screen sendiri.** NativeActivity tidak punya akses mudah ke IME Android tanpa
JNI. Karena syaratnya "Java seminimal mungkin", STONE menggambar keyboardnya sendiri dengan
GL: mode numerik, desimal, dan QWERTY penuh. Konsekuensinya: tidak ada autocorrect, tidak ada
clipboard sistem, dan tidak ada input non-ASCII. Kalau keyboard sistem lebih dibutuhkan,
satu-satunya jalan adalah menambahkan sedikit JNI (`InputMethodManager`) — itu satu-satunya
tempat di project ini yang membutuhkannya.

**Font.** Atlas di `renderer/font_data.c` dirasterisasi dari **Liberation Sans**
(SIL Open Font License 1.1) dan disimpan sebagai RLE + Base64 (~74 KB) lalu didekode saat
runtime. Regenerasi dengan `python3 tools/gen_font.py` bila ingin ukuran atau glyph lain.
Salin teks lisensi OFL ke rilis publik Anda.

**Tidak ada `android.permission.*` sama sekali.** Backup ditulis ke direktori eksternal
milik aplikasi sendiri (scoped storage), jadi tidak butuh izin penyimpanan.

**Orientasi.** Manifest mengunci `portrait`. Hapus baris `android:screenOrientation` bila
ingin landscape — layout sudah responsif dan dites dari 320×480 sampai 1600×2560.

---

## 10. Lisensi & credits

Kode aplikasi: **Created by sobing4413** — **Exter Interactive**.
Font: Liberation Sans, SIL Open Font License 1.1.


---

## 9. Texture pack (`assets/stone_pack.stpk`)

Seluruh artwork raster STONE ada di satu file: emblem splash, tiga ilustrasi onboarding,
atlas banner per-tab, dan sheet 16 badge achievement. Formatnya dijelaskan lengkap di
`app/src/main/cpp/platform/assets.h`.

Tiga keputusan desain di baliknya:

* **RGBA mentah, bukan PNG.** Project ini tidak punya decoder gambar apa pun — tidak ada
  libpng, tidak ada stb_image, tidak ada Java. Renderer hanya mau RGBA mentah.
* **Disimpan tanpa kompresi** di dalam APK (`noCompress 'stpk'` di `app/build.gradle`).
  Karena itu `AAsset_getBuffer()` mengembalikan pointer langsung ke APK yang sudah di-mmap:
  nol copy, nol inflate, dan halamannya bisa dibuang kernel saat memori menipis.
* **Setiap tekstur membawa mip chain penuh.** Loader memilih level yang cocok dengan layar
  perangkat, jadi master 1024 px hanya jadi tekstur 256 px di HP kecil. Master-nya tetap
  ada di file untuk panel ber-densitas tinggi dan tablet.

Regenerasi (butuh Python 3 + Pillow + NumPy), dijalankan dari root project:

```bash
python3 tools/gen_assets.py
```

Kalau file pack-nya dihapus, aplikasi **tetap jalan**: setiap pemanggilan gambar otomatis
jatuh balik ke penggambaran vektor.

---

## 10. Catatan rilis 2.0.0

**Perbaikan**

* **Field edit tidak bisa dibuka.** Tap pada field dieksekusi saat pointer dilepas, lalu
  panel keyboard digambar di frame yang sama dan membaca event `released` yang sama itu
  sebagai "tap di luar panel" — keyboard menutup dirinya sendiri sebelum sempat muncul.
  Sekarang `ui_keyboard_open()` menelan sisa tap tersebut, dan flag `just_opened`
  mematikan seluruh input keyboard khusus di frame pembuka.
* **Navigation bar sistem menimpa tab bar aplikasi saat cold start.** Permintaan immersive
  dulu dikirim lewat JNI dari thread `android_main`, sementara decor view masih dibangun di
  UI thread — request-nya kalah balapan. Waktu kembali dari Recents view sudah attached
  sehingga retry berhasil, dan itulah kenapa bug-nya hanya muncul di pembukaan pertama.
  Sekarang `StoneActivity.java` yang menanganinya di UI thread, plus jembatan insets
  sehingga tata letak tetap aman kalau bar sistem memang tidak bisa disembunyikan.

**Tambahan**

* Wizard setup untuk first run.
* Halaman Achievements: 16 badge yang dihitung ulang dari log yang sudah ada — tidak ada
  file penyimpanan baru, dan restore backup langsung menyalakan badge yang benar.
* Dukungan tekstur di renderer: shader gambar dengan tint dan sudut membulat, cache
  tekstur, pemilihan mip otomatis.
* Keyboard on-screen: baris angka di mode teks, tombol `clear`, label menyesuaikan lebar
  tombol, dan panel yang menghormati safe area.
