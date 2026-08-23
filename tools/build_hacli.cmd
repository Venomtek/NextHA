@echo off
setlocal
set PATH=C:\ProgramData\mingw64\mingw64\bin;%PATH%
cd /d %~dp0
if not exist out mkdir out
gcc -std=gnu99 -Wall -Wextra -Werror -DHOST_BUILD -I..\src -o out\hacli.exe hacli.c ..\src\transport\host_socket.c ..\src\http\http_client.c ..\src\ha\ha_client.c ..\src\ha\ha_config.c ..\src\ha\cfg_io_host.c -lws2_32
exit /b %errorlevel%
