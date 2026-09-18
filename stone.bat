@echo off
setlocal enabledelayedexpansion
title STONE Build

REM =====================================================================
REM  stone.bat - build STONE (CMake + Gradle) jadi stone.apk, sekali klik
REM  Sesuaikan 4 baris di bawah ini kalau path di komputer kamu beda.
REM =====================================================================

set "PROJECT_DIR=C:\Users\hikam\Downloads\Music"
set "ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk"
set "JAVA_HOME=C:\Program Files\Microsoft\jdk-17.0.20.101-hotspot"

REM ANDROID_NDK_HOME dicari otomatis di bawah (ambil versi terbaru yang
REM ada di %ANDROID_HOME%\ndk\*). Kalau mau paksa versi tertentu, isi
REM manual di sini dan hapus REM di baris bawahnya:
REM set "ANDROID_NDK_HOME=%ANDROID_HOME%\ndk\26.1.10909125"

REM =====================================================================
REM  Jangan edit di bawah sini kecuali tahu apa yang diubah.
REM =====================================================================

echo ============================================================
echo  STONE build script
echo ============================================================
echo  PROJECT_DIR     = %PROJECT_DIR%
echo  ANDROID_HOME     = %ANDROID_HOME%
echo  ANDROID_NDK_HOME = %ANDROID_NDK_HOME%
echo  JAVA_HOME        = %JAVA_HOME%
echo ============================================================
echo.

if not exist "%PROJECT_DIR%\build.gradle" (
    echo [ERROR] Tidak ketemu build.gradle di "%PROJECT_DIR%".
    echo         Cek lagi PROJECT_DIR di bagian atas file stone.bat ini.
    goto :error
)

if not exist "%JAVA_HOME%\bin\java.exe" (
    echo [ERROR] JAVA_HOME salah / JDK 17 belum ketemu di: %JAVA_HOME%
    echo         Cek folder instalasi JDK kamu, lalu edit JAVA_HOME di stone.bat.
    goto :error
)

if not defined ANDROID_NDK_HOME (
    set "NDK_LATEST="
    if exist "%ANDROID_HOME%\ndk" (
        for /f "delims=" %%D in ('dir "%ANDROID_HOME%\ndk" /b /ad /o-n 2^>nul') do (
            if not defined NDK_LATEST set "NDK_LATEST=%%D"
        )
    )
    if defined NDK_LATEST (
        set "ANDROID_NDK_HOME=%ANDROID_HOME%\ndk\!NDK_LATEST!"
        echo [INFO] ANDROID_NDK_HOME auto-detect: !ANDROID_NDK_HOME!
    )
)

if not defined ANDROID_NDK_HOME (
    echo [ERROR] NDK tidak ketemu di %ANDROID_HOME%\ndk
    echo         Install dulu lewat Android Studio ^> More Actions / Tools ^>
    echo         SDK Manager ^> tab SDK Tools ^> centang "NDK (Side by side)"
    echo         dan "CMake" ^> Apply. Setelah selesai install, jalankan lagi
    echo         stone.bat ini.
    goto :error
)

if not exist "%ANDROID_NDK_HOME%" (
    echo [ERROR] ANDROID_NDK_HOME tidak ketemu: %ANDROID_NDK_HOME%
    echo         Cek NDK yang terpasang ^(lewat Android Studio ^> SDK Manager ^> SDK Tools^),
    echo         lalu edit ANDROID_NDK_HOME di stone.bat.
    goto :error
)

set "PATH=%JAVA_HOME%\bin;%ANDROID_HOME%\platform-tools;%PATH%"

cd /d "%PROJECT_DIR%" || goto :error

REM ---- Generate gradle wrapper kalau gradlew.bat belum ada -------------
if not exist "gradlew.bat" (
    echo [1/3] gradlew.bat belum ada, generate wrapper dulu...
    where gradle >nul 2>nul
    if errorlevel 1 (
        echo [ERROR] Perintah 'gradle' tidak ditemukan di PATH.
        echo         Install Gradle dulu ^(https://gradle.org/install^) lalu jalankan
        echo         ulang stone.bat ini. Ini cuma perlu sekali, setelah wrapper
        echo         ter-generate, Gradle terpisah tidak dibutuhkan lagi.
        goto :error
    )
    call gradle wrapper --gradle-version 8.7
    if errorlevel 1 goto :error
) else (
    echo [1/3] gradlew.bat sudah ada, skip generate wrapper.
)

echo.
echo [2/3] Build APK debug via Gradle...
call gradlew.bat assembleDebug
if errorlevel 1 goto :error

set "APK=%PROJECT_DIR%\app\build\outputs\apk\debug\stone.apk"

if not exist "%APK%" (
    echo [ERROR] Build sukses tapi stone.apk tidak ketemu di lokasi yang diharapkan:
    echo         %APK%
    goto :error
)

echo.
echo [3/3] Build selesai:
echo       %APK%
echo.

REM ---- Opsional: install & jalankan ke device yang tersambung ----------
set /p INSTALL="Install & jalankan ke device via adb sekarang? (y/n): "
if /i "%INSTALL%"=="y" (
    echo.
    adb devices
    echo.
    adb install -r "%APK%"
    if errorlevel 1 goto :error
    adb shell am start -n com.stone.app/android.app.NativeActivity
    echo.
    echo Menampilkan log ^(Ctrl+C untuk berhenti^)...
    adb logcat -s STONE
)

echo.
echo ============================================================
echo  SELESAI
echo ============================================================
pause
exit /b 0

:error
echo.
echo ============================================================
echo  GAGAL - lihat pesan error di atas.
echo ============================================================
pause
exit /b 1
