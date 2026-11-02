@echo off
rem Rebuild WinClock.exe with MinGW-w64 (e.g. MSYS2 or w64devkit on PATH)
windres resource.rc -O coff -o resource.o || goto :err
g++ -std=c++17 -Os -municode -mwindows -static -static-libgcc -static-libstdc++ ^
  -ffunction-sections -fdata-sections -Wl,--gc-sections -s ^
  main.cpp resource.o -o ..\WinClock.exe ^
  -lgdiplus -lcomctl32 -ldwmapi -lwtsapi32 -lshell32 -ladvapi32 -lole32 -lpsapi -lwinmm -lwinhttp -lcrypt32 -lcomdlg32 || goto :err
echo Built ..\WinClock.exe
goto :eof
:err
echo Build failed.
exit /b 1
