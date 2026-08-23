10 REM NextHA demo - uses bank 40; change if your program uses it
15 REM .ha must be in /dot/ha and ha.cfg must be in /sys/ha.cfg before running this
20 BANK 40 ERASE
30 .ha toggle light.lounge
40 .ha state light.lounge -b 40
50 LET l = BANK 40 PEEK 0 + 256 * BANK 40 PEEK 1
60 PRINT "state: ";
70 FOR i = 2 TO l : PRINT CHR$ (BANK 40 PEEK i); : NEXT i
80 PRINT
85 BANK 40 ERASE
90 .ha tmpl -b 40
100 LET l = BANK 40 PEEK 0 + 256 * BANK 40 PEEK 1
110 FOR i = 2 TO l + 1 : PRINT CHR$ (BANK 40 PEEK i); : NEXT i
