cl -c -Asnw -Gsw -Os -Zdpe v1.c
cl -c -Asnw -Gsw -Os -Zdpe V1V1.c
cl -c -Asnw -Gsw -Os -Zdpe v1v2.c
cl -c -Asnw -Gsw -Os -Zdpe v2.c
rc -r 2048.rc
link4 v1, 2048v1/align:16, 2048v1/map, slibw swinlibc, 2048.def
link4 V1V1, 2048v1v1/align:16, 2048v1v1/map, slibw swinlibc, 2048.def
link4 v1v2, 2048v1v2/align:16, 2048v1v2/map, slibw swinlibc, 2048.def
link4 v2, 2048v2/align:16, 2048v2/map, slibw swinlibc, 2048.def
rc 2048.res 2048v1.exe
rc 2048.res 2048V1V1.exe
rc 2048.res 2048v1v2.exe
rc 2048.res 2048v2.exe