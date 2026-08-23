@echo off
setlocal
set PATH=D:\ZXNextDev\z88dk\bin;%PATH%
set ZCCCFG=D:\ZXNextDev\z88dk\lib\config
cd /d %~dp0
rem -Ca-I lib\crt\newlib: needed so the printf pragma's CRT include resolves (z88dk quirk)
set ZCC_DOT=zcc +zxn -v -startup=30 -clib=sdcc_iy -SO3 --max-allocs-per-node200000 --opt-code-size -subtype=dotn -pragma-include:zpragma.inc "-Ca-ID:\ZXNextDev\z88dk\lib\crt\newlib" -Cz"--clean" -create-app
set DOT_SOURCES=src\dot\main.c src\dot\bank.c src\dot\errors.asm src\transport\esp_at.c src\transport\uart_zxn.c src\http\http_client.c src\ha\ha_client.c src\ha\ha_config.c src\ha\cfg_io_zxn.c
rem The dot is loaded at 0x8000-0xBFFF (MMU4/5, "Page 4/5" in appmake's notice). MMU6/7 -
rem "Page 0/1" in NextZXOS's default 128K map, or "Page 6/7" if appmake numbers the window
rem itself - is the 0xC000 bank window src/dot/bank.h pages user banks into. If the image
rem ever grows into it, mapping a bank overlays live code and data, so fail the build here
rem rather than ship a dot that corrupts itself.
%ZCC_DOT% %DOT_SOURCES% -o HA > build_ha.log 2>&1
if errorlevel 1 (type build_ha.log & del build_ha.log & exit /b 1)
type build_ha.log
findstr /C:"Page 0, main bank allocate and load" /C:"Page 1, main bank allocate and load" /C:"Page 6, main bank allocate and load" /C:"Page 7, main bank allocate and load" build_ha.log >nul && (echo ERROR: dot image reached MMU6/7 - bank window would overlay code & del build_ha.log & exit /b 1)
del build_ha.log
echo built HA

set LIB_SOURCES=src\http\http_client.c src\ha\ha_config.c src\ha\cfg_io_zxn.c src\ha\ha_client.c src\transport\esp_at.c src\transport\uart_zxn.c
zcc +zxn -clib=sdcc_iy -SO3 --max-allocs-per-node200000 --opt-code-size -x %LIB_SOURCES% -o ha.lib
if errorlevel 1 exit /b 1
echo built ha.lib

if not exist dist\include\transport mkdir dist\include\transport
if not exist dist\include\http mkdir dist\include\http
if not exist dist\include\ha mkdir dist\include\ha
copy /y ha.lib dist\ >nul
copy /y src\ha_errors.h dist\include\ >nul
copy /y src\transport\transport.h dist\include\transport\ >nul
copy /y src\http\http_client.h dist\include\http\ >nul
copy /y src\ha\ha_client.h dist\include\ha\ >nul
copy /y src\ha\ha_config.h dist\include\ha\ >nul
copy /y HA dist\ >nul
copy /y ha.cfg.example dist\ >nul
echo built dist

rem appmake truncates the dotn output path to 8 chars, so build as ATPROBE in the repo root and move it
%ZCC_DOT% spike\at_probe.c src\transport\uart_zxn.c -o ATPROBE
if errorlevel 1 exit /b 1
move /y ATPROBE spike\ATPROBE >nul
echo built spike\ATPROBE
