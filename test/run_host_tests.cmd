@echo off
setlocal
set PATH=C:\ProgramData\mingw64\mingw64\bin;%PATH%
cd /d %~dp0
if not exist out mkdir out
set HOST_TEST_SOURCES=test_smoke2.c test_mock.c mock_transport.c test_http_request.c test_http_response.c test_config.c test_ha_call.c test_ha_template.c ..\src\http\http_client.c ..\src\ha\ha_config.c ..\src\ha\cfg_io_host.c ..\src\ha\ha_client.c
set ESP_TEST_SOURCES=esp_main.c test_esp_at.c mock_uart.c ..\src\transport\esp_at.c
set CFLAGS=-std=gnu99 -Wall -Wextra -Werror -DHOST_BUILD -I..\src
gcc %CFLAGS% -o out\host_tests.exe main.c %HOST_TEST_SOURCES%
if errorlevel 1 exit /b 1
out\host_tests.exe
if errorlevel 1 exit /b 1
gcc %CFLAGS% -o out\esp_tests.exe %ESP_TEST_SOURCES%
if errorlevel 1 exit /b 1
out\esp_tests.exe
exit /b %errorlevel%
