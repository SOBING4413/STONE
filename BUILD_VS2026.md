# Build STONE dengan Visual Studio 2026

Dokumen ini menjelaskan alur yang benar-benar dipakai: **Visual Studio untuk menulis,
meng-IntelliSense, dan mengompilasi kode C**, lalu **Gradle untuk mengemasnya menjadi
`stone.apk`**. Keduanya memakai `CMakeLists.txt` yang sama, jadi tidak ada dua deskripsi
build yang harus disinkronkan.

> Jujur soal batasannya: template "Android Native Activity" lama di Visual Studio memakai
> sistem project `.vcxproj` + Ant/Ndk-build yang sudah ditinggalkan Google. Project ini
> sengaja memakai CMake + Gradle karena itulah yang didukung Android hari ini. Visual Studio
> tetap menjadi editor dan compiler-nya; Gradle hanya melakukan zip, align, dan sign.

---

## Langkah 1 — Pasang komponennya

Buka **Visual Studio Installer** → *Modify*, lalu centang:

* **Desktop development with C++**
  * komponen individual: **C++ CMake tools for Windows**
* **Mobile development with C++** (jika tersedia pada instalasi Anda)
  * komponen individual: **Android SDK setup**, **Android NDK**

Kalau workload Android tidak ada di edisi Anda, pasang SDK/NDK lewat **Android Studio** atau
`cmdline-tools` — STONE tidak peduli dari mana asalnya, hanya butuh path-nya.

Pastikan juga terpasang:

* **JDK 17** (misalnya Microsoft Build of OpenJDK)
* **Android SDK Platform 34** dan **Android SDK Build-Tools 34**
* **NDK r26b** (`26.1.10909125`)

---

## Langkah 2 — Set environment variable

Sekali saja, di *System Properties → Environment Variables*:

```
ANDROID_HOME       = C:\Android\Sdk
ANDROID_NDK_HOME   = C:\Android\Sdk\ndk\26.1.10909125
JAVA_HOME          = C:\Program Files\Microsoft\jdk-17.x.x
```

Tambahkan `%ANDROID_HOME%\platform-tools` ke `PATH` supaya `adb` bisa dipanggil dari mana saja.

Kalau versi NDK Anda berbeda, ubah baris `ndkVersion` di `app/build.gradle` agar cocok.

---

## Langkah 3 — Buka project

**File → Open → Folder…** lalu pilih folder `STONE/`.

Visual Studio akan menemukan `app/src/main/cpp/CMakeLists.txt`. Kalau ia tidak langsung
mengenalinya, klik kanan file tersebut → **Configure**, atau buka folder
`STONE/app/src/main/cpp` secara langsung.

### Konfigurasi CMake untuk Android

Buka **Project → CMake Settings** (atau edit `CMakeSettings.json`) dan tambahkan konfigurasi
berikut. Ini yang membuat IntelliSense memakai header NDK, bukan header Windows:

```json
{
  "configurations": [
    {
      "name": "Android-arm64-Debug",
      "generator": "Ninja",
      "configurationType": "Debug",
      "buildRoot": "${projectDir}\\out\\build\\${name}",
      "installRoot": "${projectDir}\\out\\install\\${name}",
      "cmakeCommandArgs": "-DCMAKE_TOOLCHAIN_FILE=$env{ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 -DANDROID_NDK=$env{ANDROID_NDK_HOME}",
      "buildCommandArgs": "",
      "ctestCommandArgs": "",
      "inheritEnvironments": [ "msvc_x64_x64" ]
    },
    {
      "name": "Android-arm64-Release",
      "generator": "Ninja",
      "configurationType": "Release",
      "buildRoot": "${projectDir}\\out\\build\\${name}",
      "cmakeCommandArgs": "-DCMAKE_TOOLCHAIN_FILE=$env{ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 -DANDROID_NDK=$env{ANDROID_NDK_HOME}",
      "inheritEnvironments": [ "msvc_x64_x64" ]
    }
  ]
}
```

Pilih konfigurasi di dropdown toolbar, lalu **Build → Build All**.
Hasilnya `out\build\Android-arm64-Debug\libstone.so`.

Tahap ini adalah **gerbang kompilasi**: kalau ada error C, di sinilah Anda melihatnya,
lengkap dengan navigasi error di *Error List*. APK belum dibuat.

---

## Langkah 4 — Hasilkan `stone.apk`

Di Visual Studio buka **View → Terminal** (Developer PowerShell), lalu dari root project:

```powershell
# pertama kali saja: buat Gradle wrapper jar kalau belum ada
gradle wrapper --gradle-version 8.7

# debug
.\gradlew.bat assembleDebug

# release
.\gradlew.bat assembleRelease
```

Gradle akan memanggil CMake sendiri untuk setiap ABI di `abiFilters`
(`arm64-v8a`, `armeabi-v7a`, `x86_64`), lalu mengemas semuanya.

Setelah selesai, task tambahan di `app/build.gradle` menyalin hasilnya menjadi:

```
app\build\outputs\apk\debug\stone.apk
app\build\outputs\apk\release\stone.apk
```

Terminal akan mencetak `STONE -> ...\stone.apk` sebagai konfirmasi.

### Menjadikannya satu tombol

**Project → Edit `tasks.vs.json`**, tambahkan:

```json
{
  "version": "0.2.1",
  "tasks": [
    {
      "taskLabel": "STONE: assemble debug APK",
      "appliesTo": "/",
      "type": "launch",
      "command": "${workspaceRoot}\\gradlew.bat",
      "args": [ "assembleDebug" ]
    },
    {
      "taskLabel": "STONE: assemble release APK",
      "appliesTo": "/",
      "type": "launch",
      "command": "${workspaceRoot}\\gradlew.bat",
      "args": [ "assembleRelease" ]
    },
    {
      "taskLabel": "STONE: install to device",
      "appliesTo": "/",
      "type": "launch",
      "command": "adb",
      "args": [ "install", "-r", "${workspaceRoot}\\app\\build\\outputs\\apk\\debug\\stone.apk" ]
    }
  ]
}
```

Sesudah itu klik kanan folder root di *Solution Explorer* → task-nya muncul di menu.

---

## Langkah 5 — Pasang & jalankan

```powershell
adb devices
adb install -r app\build\outputs\apk\debug\stone.apk
adb shell am start -n com.stone.app/com.stone.app.StoneActivity
adb logcat -s STONE
```

Baris log pertama yang sehat terlihat seperti:

```
I/STONE: GL ready: Adreno (TM) 660 / OpenGL ES 3.2 ...
I/STONE: STONE 2.0.0 - Created by sobing4413 / Exter Interactive
```

---

## Debugging native

1. Build konfigurasi **Debug** (sudah `jniDebuggable true`).
2. **Debug → Attach to Process…** → transport **Android**, pilih `com.stone.app`.
3. Breakpoint di file `.c` mana pun akan kena.

Kalau app crash tanpa debugger, simbolkan stack-nya:

```powershell
adb logcat | %ANDROID_NDK_HOME%\ndk-stack -sym app\build\intermediates\cmake\debug\obj\arm64-v8a
```

---

## Masalah yang sering muncul

| Gejala | Penyebab & solusi |
|---|---|
| `native_app_glue not found` saat configure | `ANDROID_NDK_HOME` belum di-set atau salah path. CMake sengaja gagal cepat dengan pesan ini. |
| `Unable to find native library entry point` | linker membuang `ANativeActivity_onCreate`. Pastikan `-u ANativeActivity_onCreate` masih ada di `CMakeLists.txt`. |
| `clang: error: no such file or directory: 'Java_com_stone_app_...'` | dua entri `-u` terpisah di `target_link_options` akan di-deduplikasi CMake, sehingga `-u` kedua hilang dan nama simbolnya dianggap nama file. Pakai satu token utuh: `"-Wl,-u,NAMA_SIMBOL"`. |
| `UnsatisfiedLinkError: nativeSetInsets` | linker membuang simbol JNI-nya. Pastikan `-u Java_com_stone_app_StoneActivity_nativeSetInsets` masih ada di `CMakeLists.txt`. Aplikasi tetap jalan tanpa ini, hanya saja jatuh ke fallback immersive native. |
| Navigation bar sistem masih menimpa tab bar | cek logcat: `window: Java inset bridge is live` harus muncul. Kalau tidak, `StoneActivity` tidak terpakai — periksa `android:name=".StoneActivity"` dan `android:hasCode="true"` di manifest. |
| `assets: stone_pack.stpk not found` | pack-nya belum dibuat. Jalankan `python3 tools/gen_assets.py` dari root project. Aplikasi tetap jalan, artwork-nya saja yang jatuh ke vektor. |
| Layar hitam, tidak ada log | konfigurasi EGL gagal. Cek `adb logcat -s STONE` — akan ada `no usable EGL config`. |
| `INSTALL_FAILED_UPDATE_INCOMPATIBLE` | signature berbeda dari versi terpasang. `adb uninstall com.stone.app` dulu. |
| `ndkVersion ... did not match` | ubah `ndkVersion` di `app/build.gradle` sesuai NDK terpasang. |
| Gradle mencoba mengunduh saat offline | jalankan sekali dengan internet agar AGP & Gradle ter-cache; setelah itu `--offline` bisa dipakai. **Aplikasinya sendiri tetap 100% offline saat runtime** — yang butuh jaringan hanya toolchain build-nya. |
| IntelliSense merah di `<android/...>` | konfigurasi CMake belum memakai `android.toolchain.cmake` (Langkah 3). |

---

Created by **sobing4413** · **Exter Interactive**
