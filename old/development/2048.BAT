cl -c -Asnw -Gsw -Os -Zdpe 2048.c
rc -r 2048.rc
link4 2048, 2048/align:16, 2048/map, slibw swinlibc, 2048.def
rc 2048.res
mapsym 2048