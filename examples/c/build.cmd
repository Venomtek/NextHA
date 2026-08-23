@echo off
setlocal
set PATH=D:\ZXNextDev\z88dk\bin;%PATH%
set ZCCCFG=D:\ZXNextDev\z88dk\lib\config
cd /d %~dp0
rem -Ca-I lib\crt\newlib: needed so the printf pragma's CRT include resolves (z88dk quirk)
rem -o TOGGLE: appmake truncates the dotn output path to 8 chars, so no directory prefix
zcc +zxn -v -startup=30 -clib=sdcc_iy -SO3 --max-allocs-per-node200000 --opt-code-size -subtype=dotn -pragma-include:..\..\zpragma.inc "-Ca-ID:\ZXNextDev\z88dk\lib\crt\newlib" -I..\..\dist\include -L..\..\dist -lha toggle.c -o TOGGLE -Cz"--clean" -create-app
if errorlevel 1 exit /b 1
echo built TOGGLE
