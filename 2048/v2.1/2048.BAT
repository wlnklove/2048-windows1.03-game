cl -c -Asnw -Gsw -Os -Zdpe 2048e.c
cl -c -Asnw -Gsw -Os -Zdpe 2048v.c
rc -r 2048.rc
link4 2048e, 2048e/align:16, 2048e/map, slibw swinlibc, 2048.def
rc 2048.res 2048e.exe
link4 2048v, 2048v/align:16, 2048v/map, slibw swinlibc, 2048.def
rc 2048.res 2048v.exe
mapsym 2048e
mapsym 2048v