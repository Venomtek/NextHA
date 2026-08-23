SECTION rodata_user

PUBLIC _err_esp_none, _err_esp_init, _err_connect, _err_timeout, _err_send, _err_break
PUBLIC _err_http, _err_auth, _err_status, _err_template
PUBLIC _err_cfg_missing, _err_cfg_key, _err_cfg_toolong
PUBLIC _err_usage, _err_bank, _err_len

_err_esp_none:    defm "1 No response from ES", 'P' + 0x80
_err_esp_init:    defm "6 WiFi init faile", 'd' + 0x80
_err_connect:     defm "2 Cannot reach HA hos", 't' + 0x80
_err_timeout:     defm "B Timeout talking to H", 'A' + 0x80
_err_send:        defm "7 Send to HA faile", 'd' + 0x80
_err_break:       defm "D BREAK into progra", 'm' + 0x80
_err_http:        defm "8 Bad HTTP respons", 'e' + 0x80
_err_auth:        defm "U Check token in ha.cf", 'g' + 0x80
_err_status:      defm "S HA rejected reques", 't' + 0x80
_err_template:    defm "T HA rejected templat", 'e' + 0x80
_err_cfg_missing: defm "C ha.cfg not foun", 'd' + 0x80
_err_cfg_key:     defm "K ha.cfg needs host+toke", 'n' + 0x80
_err_cfg_toolong: defm "L ha.cfg value too lon", 'g' + 0x80
_err_usage:       defm "H Usage: .ha on|off|toggle|state|call|tmp", 'l' + 0x80
_err_bank:        defm "E Bank must be 0-11", '1' + 0x80
_err_len:         defm "E Length must be a numbe", 'r' + 0x80
